#include "Networking.hpp"
#include "Window.hpp"
#include "common/text_packet.h"

void receivePicture(SOCKET socket, Packet initialPacket) {
    std::vector<unsigned char> pictureData;
    pictureData.insert(pictureData.end(), initialPacket.data, initialPacket.data + initialPacket.length);
    uint32_t expectedPacketType = PACKET_TYPE_PICTURE;

    while (true) {
        Packet packet;
        int bytesReceived = recv(socket, reinterpret_cast<char*>(&packet), sizeof(Packet), 0);
        if (bytesReceived <= 0) {
            // Handle error or connection closed
            break;
        }

        if (packet.packetType != expectedPacketType) {
            // Handle unexpected packet type
            break;
        }

        pictureData.insert(pictureData.end(), packet.data, packet.data + packet.length);

        if (packet.length < MAX_PACKET_SIZE) {
            // Last packet received, stop receiving
            break;
        }
    }

    // Process the received picture data here
}

bool createSocket(ServerInfo& server_info, SOCKET* tcpSocket, SOCKET* udpSocket) {
    sockaddr_in server;

    // Setup Client Socket
    *tcpSocket = INVALID_SOCKET;
    *tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*tcpSocket == INVALID_SOCKET) {
        std::cout << "[TCP] Error at Socket(): " << WSAGetLastError() << std::endl;
        closesocket(*tcpSocket);
        return false;
    }

    // Connect to server and bind :: Fill in hint structure, which server to connect to
    server.sin_family = AF_INET;
    server.sin_port = htons(static_cast<u_short>(std::stoul(server_info.port)));
    InetPton(AF_INET, server_info.url.c_str(), &server.sin_addr.s_addr);
    if (connect(*tcpSocket, (SOCKADDR*)&server, sizeof(server)) == SOCKET_ERROR) {
        std::cout << "[TCP] Client:connect()- Failed to connect." << std::endl;
        closesocket(*tcpSocket);
        return false;
    }

    AuthPacket auth;

    std::string username = server_info.username;
    auth.uLength = static_cast<uint32_t>(username.length());
    memcpy(auth.username, username.c_str(), username.length());

    std::string password = "password";
    auth.pLength = static_cast<uint32_t>(password.length());
    memcpy(auth.password, password.c_str(), password.length());

    // Send authentication packet
    send(*tcpSocket, reinterpret_cast<const char*>(&auth), sizeof(AuthPacket), 0);

    // Receive server response
    char buffer[256];
    int bytesReceived = recv(*tcpSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        if (strcmp(buffer, "goodauth") == 0) {
            std::cout << "Server response: " << buffer << std::endl;
        } else {
            std::cout << "Incorrect Password" << std::endl;
            closesocket(*tcpSocket);
            return false;
        }
    }

    // UDP Socket
    *udpSocket = INVALID_SOCKET;
    *udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (*udpSocket == INVALID_SOCKET) {
        closesocket(*tcpSocket);
        closesocket(*udpSocket);
        std::cout << "[UDP] Error at Socket(): " << WSAGetLastError() << std::endl;
        return false;
    } else {
        std::cout << "[UDP] Socket() is OK!" << std::endl;
    }

    if (connect(*udpSocket, (SOCKADDR*)&server, sizeof(server)) == SOCKET_ERROR) {
        std::cout << "[UDP] Client:connect()- Failed to connect." << std::endl;
        closesocket(*udpSocket);
        return false;
    }

    return true;
}

static gboolean idle_callback(gpointer user_data) {
    auto data = static_cast<std::pair<VoiceOpsWindow&, std::string>*>(user_data);
    VoiceOpsWindow& windowref = data->first;
    std::string message = data->second;

    windowref.add_new_message(message.c_str(), "Client Name");

    delete data;
    return G_SOURCE_REMOVE;
}

void ReceiveMessages(SOCKET clientSocket, GMutex& mutex, DataStore& data, VoiceOpsWindow& windowref) {
    Packet receivePacket;
    while (true) {
        int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&receivePacket), sizeof(Packet), 0);
        if (bytesReceived > 0 && receivePacket.packetType == PACKET_TYPE_STRING) {
            g_mutex_lock(&mutex);
            // Appending message to the chat box
            std::string str(receivePacket.data, receivePacket.length);

            g_mutex_unlock(&mutex);

            std::cout << "Received message: " << str << '\n';

            auto* user_data = new std::pair<VoiceOpsWindow&, std::string>(windowref, str);
            g_idle_add(idle_callback, user_data);
        } else if (bytesReceived > 0 && receivePacket.packetType == PACKET_TYPE_PICTURE) {
            receivePicture(clientSocket, receivePacket);
        } else if (bytesReceived == 0) {
            std::cout << "Connection closed by server." << std::endl;
            break;
        } else {
            std::cerr << "Error in receiving data from server: " << WSAGetLastError() << std::endl;
            break;
        }
    }
}
