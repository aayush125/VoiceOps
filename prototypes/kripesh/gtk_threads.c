#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <opus/opus.h>

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_GENERATION
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_WASAPI
#define MA_NO_DECODING
#define MA_NO_ENCODING
// #define MA_NO_NODE_GRAPH
#include <miniaudio.h>

#include "jitter_buffer.h"

// For outgoing sound
OpusEncoder *enc;
struct addrinfo *outgoing = NULL;
SOCKET outgoingSocket = INVALID_SOCKET;
uint32_t current_pkt_number = 1;
uint32_t last_processed_pkt;

// For incoming sound
jitterbuffer buf;
OpusDecoder *dec;
uint32_t prev_pkt = 0;
struct packet input_pkt;

// Packet
// struct packet {
//   uint32_t packet_number;
//   byte encoded_data[400];
// };

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
  if (frameCount != 480) {
    printf("gg. framecount was unexpectly %u\n", frameCount);
  }

  uint16_t size;
  if (jb_get(&buf, &input_pkt, &size)) {
    if (prev_pkt != 0) {
      // this is our silly way of handling pkt loss
      for (uint32_t i = prev_pkt + 1; i < input_pkt.packet_number; i++) {
        printf("Packet loss - BAD!\n");

        int len = opus_decode(dec, NULL, 0, pOutput, 480, 0);
        if (len < 0) {
          printf("OPUS ERROR: %d\n", len);
        }
      }
    }

    prev_pkt = input_pkt.packet_number;

    int len = opus_decode(dec, input_pkt.encoded_data, size, pOutput, 480, 0);
    if (len < 0) {
      printf("OPUS ERROR: %d\n", len);
    }

  } else {
    MA_ZERO_MEMORY(pOutput, 1920);
  }

  // Outgoing part below:
  struct packet outgoing_pkt;
  outgoing_pkt.packet_number = current_pkt_number;

  int len = opus_encode(enc, pInput, frameCount, outgoing_pkt.encoded_data, 400);

  int iResult = sendto(outgoingSocket, &outgoing_pkt, len + 4, 0, outgoing->ai_addr, outgoing->ai_addrlen);
  if (iResult == SOCKET_ERROR) {
    printf("sendto() failed with error code : %d" , WSAGetLastError());
  }

  current_pkt_number++;
}

int main(int argc, char *argv[])
{
  if (argc != 5) {
    printf("Peer to Peer VoiceOps Experiment.\n");
    printf("Uses: Opus codec, winsock2, miniaudio.\nPlatform: Windows only.\n\n");
    
    printf("Usage:\n");
    printf("./p2p_client.exe <Your IP address> <Host Port> <IP Address of other client> <Other client's port>\n");
    printf("So running \"./client_p2p.exe 192.168.1.1 27015 192.168.1.2 27019\" would host the app on port 27015, binding to interface with IP 192.168.1.1. The app will then connect to peer at 192.168.1.2:27019\n\n");
    
    printf("Press Enter to exit...");
    getchar();
    return 1;
  }

  WSADATA wsadata;

  // Initialize WinSock 2.2
  int iResult = WSAStartup(MAKEWORD(2, 2), &wsadata);
  if (iResult != 0)
  {
    printf("failed\n");
    return 1;
  }

  // Temporary pointer for address info
  struct addrinfo *result = NULL, hints;
  
  // Hints for getaddrinfo function
  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;

  // Get address info for other client
  iResult = getaddrinfo(argv[3], argv[4], &hints, &result);
  if (iResult != 0)
  {
    printf("getaddrinfo failed: %d\n", iResult);
    WSACleanup();
    return 1;
  }

  // Outgoing Socket
  outgoing = result;
  outgoingSocket = socket(outgoing->ai_family, outgoing->ai_socktype, outgoing->ai_protocol);
  if (outgoingSocket == INVALID_SOCKET)
  {
    printf("Error at socket(): %ld\n", WSAGetLastError());
    freeaddrinfo(result);
    WSACleanup();
    return 1;
  }

  // Incoming Socket - get address info
  iResult = getaddrinfo(argv[1], argv[2], &hints, &result);
  if (iResult != 0)
  {
    printf("getaddrinfo failed: %d\n", iResult);
    WSACleanup();
    return 1;
  }

  // Create the listen/incoming socket
  SOCKET listenSocket = INVALID_SOCKET;

  listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (listenSocket == INVALID_SOCKET)
  {
    printf("Error at socket(): %ld\n", WSAGetLastError());
    freeaddrinfo(result);
    WSACleanup();
    return 1;
  }

  // Bind the incoming socket, to a specific interface & port
  iResult = bind(listenSocket, result->ai_addr, (int)result->ai_addrlen);
  if (iResult == SOCKET_ERROR)
  {
    freeaddrinfo(result);
    closesocket(listenSocket);
    WSACleanup();
    return 1;
  }

  freeaddrinfo(result);

  // Struct to hold sender info (only used to print "received packet" line)
  struct sockaddr_in sender;
  int sender_len = sizeof(sender);
  
  // SOUND STUFF BELOW

  // Opus initialization
  int error;
  enc = opus_encoder_create(48000, 2, OPUS_APPLICATION_VOIP, &error);
  if (error) {
    return 1;
  }
  opus_encoder_ctl(enc, OPUS_SET_BITRATE(64000));
  opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(10));
  // opus_encoder_ctl(enc, OPUS_SET)
  
  dec = opus_decoder_create(48000, 2, &error);
  if (error) {
    return 1;
  }

  // MiniAudio initialization
  ma_result maResult;
  ma_device_config deviceConfig;
  ma_device device;

  deviceConfig = ma_device_config_init(ma_device_type_duplex);
  deviceConfig.capture.pDeviceID          = NULL;
  deviceConfig.capture.format             = ma_format_s16;
  deviceConfig.capture.channels           = 2;
  deviceConfig.capture.shareMode          = ma_share_mode_shared;
  deviceConfig.playback.pDeviceID         = NULL;
  deviceConfig.playback.format            = ma_format_s16;
  deviceConfig.playback.channels          = 2;
  deviceConfig.dataCallback               = data_callback;
  deviceConfig.noClip                     = TRUE;
  deviceConfig.wasapi.noAutoConvertSRC    = TRUE;
  deviceConfig.noPreSilencedOutputBuffer  = TRUE;
  maResult = ma_device_init(NULL, &deviceConfig, &device);
  if (maResult != MA_SUCCESS) {
    return maResult;
  }

  // Packet stuff
  struct packet incoming_pkt;
  uint32_t expected_pkt_number = 0; // Before jitter buffer does its thing

  jb_initialize(&buf);

  ma_device_start(&device);

  while (1) {
    int recv_len = recvfrom(listenSocket, &incoming_pkt, 404, 0, (struct sockaddr *) &sender, &sender_len);
    if (recv_len == SOCKET_ERROR) {
      printf("recvfrom() failed with error code : %d" , WSAGetLastError());
      return 1;
    }

    // Are packets being received in proper order?
    if (incoming_pkt.packet_number != expected_pkt_number) {
      if (expected_pkt_number == 0) {
        expected_pkt_number = incoming_pkt.packet_number;
      } else {
        printf("Expected pkt no. %u. Got packet %u instead\n", expected_pkt_number, incoming_pkt.packet_number);
      }
    }
    expected_pkt_number++;

    jb_insert(&buf, &incoming_pkt, recv_len - 4);
  }

  ma_device_stop(&device);

  closesocket(outgoingSocket);
  closesocket(listenSocket);
  freeaddrinfo(outgoing);

  WSACleanup();
  opus_encoder_destroy(enc);

  jb_terminate(&buf);

  return 0;
}
