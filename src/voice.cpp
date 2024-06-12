#include "Voice.hpp"

#include <thread>
#include <iostream>

#include <common/jitter_buffer.h>

#include <thirdparty/opus/opus.h>
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_GENERATION
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_WASAPI
#define MA_NO_DECODING
#define MA_NO_ENCODING
// #define MA_NO_NODE_GRAPH
#include <thirdparty/miniaudio.h>

// Voice related structs
struct ClientData {
    uint32_t prev_consumed_pkt = 0;

    OpusDecoder* dec;
    JitterBuffer jb;
};

struct Clients {
    ClientData data[MAX_CLIENTS];
};

// unit scope variables
static Clients* clients_ptr;
static OpusEncoder* enc;
static std::thread voiceListener;
static ma_device device;
static SOCKET UDPSocket;
static boolean voiceConnected = false;

static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    static uint32_t current_pkt_number = 1;

    if (frameCount != 480) {
        std::cout << "[WARNING] frameCount is not 480! It is " << frameCount << " instead." << std::endl;
        return;
    }

    VoicePacketToServer pkt;
    uint16_t size;
    opus_int32 workingBuffer[960] = { 0 };
    opus_int16 tempStore[960];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        auto& current = clients_ptr->data[i];
        if (current.jb.get(&pkt, &size)) {
            // handle pkt loss
            if (current.prev_consumed_pkt != 0) {
                for (uint32_t i2 = current.prev_consumed_pkt + 1; i2 < pkt.packet_number; i2++) {
                    std::cout << "Packet loss - BAD!" << std::endl;

                    int len = opus_decode(current.dec, NULL, 0, tempStore, 480, 0);
                    if (len < 0) {
                        std::cout << "OPUS ERROR: " << len << std::endl;
                    }
                }
            }
            current.prev_consumed_pkt = pkt.packet_number;

            int len = opus_decode(current.dec, pkt.encoded_data, size, tempStore, 480, 0);
            if (len < 0) {
                std::cout << "OPUS ERROR: " << len << std::endl;
            }

            // Mixing
            for (uint16_t j = 0; j < 960; j++) {
                workingBuffer[j] += (opus_int32)tempStore[j];
            }
        }
    }
    ma_clip_samples_s16((ma_int16*)pOutput, workingBuffer, 960);

    // Outgoing part below:
    VoicePacketToServer outgoing_pkt;
    outgoing_pkt.packet_number = current_pkt_number;

    int len = opus_encode(enc, (const opus_int16*)pInput, frameCount, outgoing_pkt.encoded_data, 400);

    int iResult = send(UDPSocket, (const char*)&outgoing_pkt, len + 4, 0);
    if (iResult == SOCKET_ERROR) {
        printf("sendto() failed with error code : %d", WSAGetLastError());
    }

    current_pkt_number++;
}

static void ReceiveVoice(SOCKET voiceSocket) {
    Clients clients;
    clients_ptr = &clients;

    VoicePacketFromServer incoming_pkt;

    int error;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients.data[i].dec = opus_decoder_create(48000, 2, &error);
        if (error) {
            std::cout << "Error while creating Opus Decoder #" << i << std::endl;
            return;
        }
    }

    enc = opus_encoder_create(48000, 2, OPUS_APPLICATION_VOIP, &error);
    if (error) {
        std::cout << "Error while creating Opus Encoder" << std::endl;
        return;
    }

    while (true) {
        int recv_len = recv(voiceSocket, (char*)&incoming_pkt, sizeof(VoicePacketFromServer), 0);
        if (recv_len == SOCKET_ERROR) {
            std::cout << "recv() failed with error code  " << WSAGetLastError() << std::endl;
            continue;
        }

        VoicePacketToServer pkt;
        // Byte alignment makes this - 8
        memcpy(pkt.encoded_data, incoming_pkt.encoded_data, recv_len - 8);
        pkt.packet_number = incoming_pkt.packet_number;

        clients.data[incoming_pkt.userID].jb.insert(&pkt, recv_len - 8); // - 4 bytes is of packet/sequence number
    }
}

void Voice::init() {
    // MiniAudio initialization
    ma_result maResult;
    ma_device_config deviceConfig;

    deviceConfig = ma_device_config_init(ma_device_type_duplex);
    deviceConfig.capture.pDeviceID = NULL;
    deviceConfig.capture.format = ma_format_s16;
    deviceConfig.capture.channels = 2;
    deviceConfig.capture.shareMode = ma_share_mode_shared;
    deviceConfig.playback.pDeviceID = NULL;
    deviceConfig.playback.format = ma_format_s16;
    deviceConfig.playback.channels = 2;
    deviceConfig.dataCallback = data_callback;
    deviceConfig.noClip = TRUE;
    deviceConfig.wasapi.noAutoConvertSRC = TRUE;
    deviceConfig.noPreSilencedOutputBuffer = TRUE;
    deviceConfig.periodSizeInFrames = 480;
    maResult = ma_device_init(NULL, &deviceConfig, &device);
    if (maResult != MA_SUCCESS) {
        std::cout << "Error initializing audio device" << std::endl;
    }
}

void Voice::newThread(SOCKET s) {
    UDPSocket = s;
    voiceListener = std::thread(ReceiveVoice, UDPSocket);
}

void Voice::joinThread() {
    if (voiceListener.joinable()) {
        voiceListener.join();
    }
}

void Voice::toggle() {
    if (voiceConnected) {
        ma_device_stop(&device);
        voiceConnected = false;
    } else {
        ma_device_start(&device);
        voiceConnected = true;
    }
}

void Voice::forceStop() {
    ma_device_stop(&device);
    voiceConnected = false;
}
