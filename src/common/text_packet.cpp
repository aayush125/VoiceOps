#define NOGDI
#include <winsock2.h>
#include <iostream>

#include "common/text_packet.h"

// This works because we have FIXED size data transmissions
ReceiveResult recv_pkt(uint64_t socket, Packet& pkt) {
    size_t remaining_bytes = sizeof(Packet);
    char* data_position = reinterpret_cast<char*>(&pkt);

    while (remaining_bytes > 0) {
        int received = recv(socket, data_position, remaining_bytes, 0);

        if (received < 0) {
            std::cout << "[recv_pkt] Socket Error" << std::endl;
            return RECEIVE_RESULT_ERROR;
        }

        if (received == 0) {
            std::cout << "[recv_pkt] Connection closed" << std::endl;
            return RECEIVE_RESULT_CONN_CLOSED;
        }

        remaining_bytes -= received;
        data_position += received;
    }

    return RECEIVE_RESULT_SUCCESS;
}