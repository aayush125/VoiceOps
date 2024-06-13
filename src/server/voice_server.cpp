#include <server/voice_server.h>
#include <common/jitter_buffer.h>
#include <iostream>
#include <ws2tcpip.h>
#include <thirdparty/opus/opus.h>
#include <thread>
#include <timeapi.h>
#include <thirdparty/renamenoise.h>

struct ClientSocket {
    uint32_t address;
    uint16_t port;

    bool operator==(const ClientSocket& b) {
        return address == b.address && port == b.port;
    }
};

struct ClientData {
    // Client's jitter buffer
    // Also: ping packet stuff? for keep-alive connections?
    // Each client also has their own decoder
    uint32_t prev_consumed_pkt = 0;
    uint32_t last_sent_pkt = 0;

    OpusDecoder* dec;
    OpusEncoder* enc;
    JitterBuffer jb;

    ReNameNoiseDenoiseState* denoiser_l;
    ReNameNoiseDenoiseState* denoiser_r;
};

#define MAX_CLIENTS 10
struct Clients {
    uint8_t count = 0;
    GMutex count_lock;

    ClientSocket sockets[MAX_CLIENTS];
    sockaddr_in sockaddrs[MAX_CLIENTS];
    ClientData data[MAX_CLIENTS];
};

// #include <time.h>

// double get_time_diff(struct timespec end, struct timespec start) {
//   double difference = (double) (end.tv_sec - start.tv_sec) + 1.0e-9 * (double) (end.tv_nsec - start.tv_nsec);
//   return difference * 1.0e6;
// }

struct SharedData {
    Clients* clients;
    SOCKET voiceSocket;
};

void CALLBACK TimeProc(UINT uID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
    SharedData* data = (SharedData*)dwUser;
    auto& clients = data->clients;
    auto& voiceSocket = data->voiceSocket;

    VoicePacketToServer pkt;
    uint16_t size;

    // 10 ms at 48000 hz, 16 bit pcm, 2 channels
    int16_t audio[960];

    // Do processing
    uint8_t count;
    g_mutex_lock(&clients->count_lock);
    count = clients->count;
    g_mutex_unlock(&clients->count_lock);

    for (uint8_t i = 0; i < count; i++) {
        auto& current = clients->data[i];

        if (current.jb.get(&pkt, &size)) {
            // handle pkt loss
            if (current.prev_consumed_pkt != 0) {
                for (uint32_t i2 = current.prev_consumed_pkt + 1; i2 < pkt.packet_number; i2++) {
                    std::cout << "Packet loss - BAD!" << std::endl;

                    int len = opus_decode(current.dec, NULL, 0, audio, 480, 0);
                    if (len < 0) {
                        std::cout << "OPUS ERROR: " << len << std::endl;
                    }
                }
            }
            current.prev_consumed_pkt = pkt.packet_number;

            int len = opus_decode(current.dec, pkt.encoded_data, size, audio, 480, 0);
            if (len < 0) {
                std::cout << "OPUS ERROR: " << len << std::endl;
            }

            float temp_l[480];
            float temp_r[480];

            int16_t processed_l[480];
            int16_t processed_r[480];

            for (int j = 0; j < 480; j++) {
                temp_l[j] = audio[j * 2];
                temp_r[j] = audio[j * 2 + 1];
            }

            renamenoise_process_frame_clamped(current.denoiser_l, processed_l, temp_l);
            renamenoise_process_frame_clamped(current.denoiser_r, processed_r, temp_r);

            for (int j = 0; j < 480; j++) {
                audio[j * 2] = processed_l[j];
                audio[j * 2 + 1] = processed_r[j];
            }

            // reencode and sent to client
            VoicePacketFromServer outgoing_pkt;
            outgoing_pkt.packet_number = ++current.last_sent_pkt;
            // outgoing_pkt.packet_number = pkt.packet_number;
            outgoing_pkt.userID = i;
            // memcpy(outgoing_pkt.encoded_data, pkt.encoded_data, size);
            len = opus_encode(current.enc, audio, 480, outgoing_pkt.encoded_data, 400);
            if (len < 0) {
                std::cout << "OPUS ERROR: " << len << std::endl;
            }

            // Send to all other clients
            for (uint8_t j = 0; j < count; j++) {
                if (j == i) {
                    continue;
                }

                int bytesSent = sendto(voiceSocket, (const char*)&outgoing_pkt, len + 4 + 4, 0, (sockaddr*)&clients->sockaddrs[j], sizeof(sockaddr_in));
                if (bytesSent == SOCKET_ERROR) {
                    std::cout << "sendto() failed with error code : " << WSAGetLastError() << std::endl;
                    continue;
                }
            }
        }
    }
}

void packetConsumer(SOCKET voiceSocket, Clients* clients) {
    timeBeginPeriod(1);

    SharedData data;
    data.clients = clients;
    data.voiceSocket = voiceSocket;

    timeSetEvent(10, 0, TimeProc, (DWORD_PTR)&data, TIME_PERIODIC);

    while (true) {
        Sleep(1000);
    }

    // struct timespec test;
    // clock_gettime(CLOCK_MONOTONIC, &test);
    // std::cout << test.tv_nsec / 1000000 << std::endl;

    // timeEndPeriod(1);
}

void voiceReceiver(SOCKET voiceSocket) {
    struct sockaddr_in sender;
    int sender_len = sizeof(sender);

    VoicePacketToServer incoming_pkt;

    Clients clients;
    g_mutex_init(&clients.count_lock);

    int error;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients.data[i].dec = opus_decoder_create(48000, 2, &error);
        if (error) {
            std::cout << "Error while creating Opus Decoder #" << i << std::endl;
            return;
        }

        clients.data[i].enc = opus_encoder_create(48000, 2, OPUS_APPLICATION_VOIP, &error);
        if (error) {
            std::cout << "Error while creating Opus Encoder #" << i << std::endl;
            return;
        }

        opus_encoder_ctl(clients.data[i].enc, OPUS_SET_BITRATE(64000));
        opus_encoder_ctl(clients.data[i].enc, OPUS_SET_COMPLEXITY(10));

        clients.data[i].denoiser_l = renamenoise_create(NULL);
        clients.data[i].denoiser_r = renamenoise_create(NULL);
    }

    // Start another thread to consume and process voice packets. This thread is put on a timer.
    auto consumer = std::thread(packetConsumer, voiceSocket, &clients);

    while (true) {
        int recv_len = recvfrom(voiceSocket, (char*)&incoming_pkt, sizeof(VoicePacketToServer), 0, (struct sockaddr*)&sender, &sender_len);

        if (recv_len == SOCKET_ERROR) {
            std::cout << "recvfrom() failed with error code  " << WSAGetLastError() << std::endl;
            return;
        }

        if (recv_len == 4) {
            // Just 4 bytes, this is ping packet
            char* ping = (char *) &incoming_pkt;

            if (ping[0] == 'p' && ping[1] == 'i' && ping[2] == 'n' && ping[3] == 'g') {
                // Just reply
                sendto(voiceSocket, "ping", 4, 0, (struct sockaddr*)&sender, sender_len);
                continue;
            }
        }

        ClientSocket curSocket = { sender.sin_addr.S_un.S_addr, sender.sin_port };
        ClientData* curData = nullptr;
        int current = -1;
        for (int i = 0; i < clients.count; i++) {
            if (clients.sockets[i] == curSocket) {
                curData = &clients.data[i];
                current = i;
                break;
            }
        }

        if (curData == nullptr) {
            if (clients.count == MAX_CLIENTS) {
                std::cout << "Max clients already. Cannot accept new client." << std::endl;
                continue;
            }
            std::cout << "Accepted a new voice client. IP: " << inet_ntoa(sender.sin_addr) << ", Port: " << ntohs(sender.sin_port) << std::endl;
            clients.sockets[clients.count] = curSocket;
            clients.sockaddrs[clients.count] = sender;
            curData = &clients.data[clients.count];
            current = clients.count;

            g_mutex_lock(&clients.count_lock);
            clients.count++;
            g_mutex_unlock(&clients.count_lock);
        }

        /* // Alternative 1: No server-side processing. Just forward packets.
        VoicePacketFromServer outgoing_pkt;
        outgoing_pkt.packet_number = incoming_pkt.packet_number;
        outgoing_pkt.userID = current;
        memcpy(outgoing_pkt.encoded_data, incoming_pkt.encoded_data, recv_len - 4);

        Send to all other clients
        for (uint8_t j = 0; j < clients.count; j++) {
          // if (j == current) {
          //   continue;
          // }

          // + 4 due to byte alignment
          int bytesSent = sendto(voiceSocket, (const char *)&outgoing_pkt, recv_len + 4, 0, (sockaddr*)&clients.sockaddrs[j], sizeof(sockaddr_in));
          if (bytesSent == SOCKET_ERROR) {
            std::cout << "sendto() failed with error code : " << WSAGetLastError() << std::endl;
            continue;
          }
        }
        */

        // Now that we know which client sent the packet, we can put the packet in the appropriate jitter buffer
        curData->jb.insert(&incoming_pkt, recv_len - 4); // - 4 bytes is of packet/sequence number
    }

    // g_mutex_clear(&clients.count_lock);
}
