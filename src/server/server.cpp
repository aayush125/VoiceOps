#include <WinSock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <filesystem>
#include <tchar.h>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
#include <mutex>
#include <map>
#include <cstdint>
#include "filefunction.h"
#include "database.h"
#include <server/voice_server.h>
#include <common/text_packet.h>

#define MAX_PACKET_SIZE 1500

std::vector<data> databaseQuery;
//Map socket to usernames
std::map<SOCKET, std::string> clientUsernames;

void receivePicture(SOCKET socket, Packet initialPacket) {
    std::vector<unsigned char> pictureData;
    pictureData.insert(pictureData.end(), initialPacket.data.bytes, initialPacket.data.bytes + initialPacket.length);
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

        pictureData.insert(pictureData.end(), packet.data.bytes, packet.data.bytes + packet.length);

        if (packet.length < MAX_PACKET_SIZE) {
            // Last packet received, stop receiving
            break;
        }
    }

    // Process the received picture data
    std::cout << pictureData.size() << std::endl;
    if (!pictureData.empty()) {
        // Save or display the picture data
        std::cout << "I received the image i guess" << std::endl;
        std::string outputFilename = "output.jpg";
        if (savePicture(pictureData, outputFilename)) {
            std::cout << "Picture saved to " << outputFilename << std::endl;
        } else {
            std::cout << "Failed to save picture" << std::endl;
        }
    }
}

bool handleNewConnection(SOCKET clientSocket, std::string &username) {
    // Receive authentication packet
    AuthPacket authPacket;

    int byteCount = recv(clientSocket, reinterpret_cast<char*>(&authPacket), sizeof(AuthPacket), 0);
    std::string password(authPacket.password, authPacket.password + authPacket.pLength);
    username = std::string(authPacket.username, authPacket.username + authPacket.uLength);
    if (byteCount <= 0) {
        std::cerr << "Failed to receive authentication packet." << std::endl;
        return false;
    }

    if (strcmp(password.c_str(), "password") == 0) {
        std::cout << "Passwords matched" << std::endl;
        return true;
    }
    return false;
}

int main(int argc, char* argv[]) {
    int port;
    if (argc == 2) {
        port = atoi(argv[1]);
    } else {
        std::cout << "Enter a port: ";
        std::cin >> port;
    }

    SOCKET serverSocket, acceptSocket;
    int MAXCONN = 30;
    int queryMessagesLimit = 5;
    // Loading the database file
    std::string directoryDatabaseString = "database/server.db";
    const char* directoryDatabase = directoryDatabaseString.c_str();

    createDB(directoryDatabase);
    createTable(directoryDatabase);
    databaseQuery = selectData(directoryDatabase, queryMessagesLimit);

    // Loading the dll file
    WSADATA wsaData;
    int wsaerr;
    WORD wVersionRequested = MAKEWORD(2, 2);
    wsaerr = WSAStartup(wVersionRequested, &wsaData);
    if (wsaerr != 0) {
        std::cout << "The winsock dll not found!" << std::endl;
        return 0;
    }

    // Setup server Socket
    serverSocket = INVALID_SOCKET;
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cout << "Error at Socket(): " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 0;
    }

    // Bind the ip and port number to the socket
    sockaddr_in service;
    service.sin_family = AF_INET;

    InetPton(AF_INET, _T("0.0.0.0"), &service.sin_addr.s_addr);
    service.sin_port = htons(port);
    if (bind(serverSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        std::cout << "Bind() failed!" << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 0;
    }

    // Listen on the socket

    if (listen(serverSocket, MAXCONN) == SOCKET_ERROR) {
        std::cout << "Listen(): Error listening on socket!" << WSAGetLastError() << std::endl;
    } else {
        std::cout << "Server Initialised, I am waiting Connections" << std::endl;
    }

    // Voice Chat socket
    SOCKET voiceServerSocket = INVALID_SOCKET;
    voiceServerSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (voiceServerSocket == INVALID_SOCKET) {
        std::cout << "Error at Socket(): " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 0;
    }

    // Bind the UDP voice socket on the same port and ip as TCP socket
    if (bind(voiceServerSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        std::cout << "Bind() failed!" << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 0;
    }

    // Spin off the voice server to a separate thread
    auto voiceProducerThread = std::thread(voiceReceiver, voiceServerSocket);

    fd_set master;
    FD_ZERO(&master);

    FD_SET(serverSocket, &master);

    while (true) {

        fd_set copy = master;
        int socketCount = select(0, &copy, nullptr, nullptr, nullptr);

        for (int i = 0; i < socketCount; i++) {
            SOCKET sock = copy.fd_array[i];
            if (sock == serverSocket) {

                // Accept a new Connection
                acceptSocket = accept(serverSocket, nullptr, nullptr);
                if (acceptSocket == INVALID_SOCKET) {
                    std::cout << "Accept Failed: " << WSAGetLastError() << std::endl;
                    WSACleanup();
                    return -1;
                } else {
                    std::cout << "Accepted Connection" << std::endl;
                }

                std::string username;
                // Add the new connection to the list of connected Clients
                if (handleNewConnection(acceptSocket, username)) {
                    Packet messagePacket;
                    const char* authSuccess = "goodauth";
                    send(acceptSocket, authSuccess, strlen(authSuccess), 0);
                    FD_SET(acceptSocket, &master);
                    std::cout << "The username outside handleconnection is " << username << std::endl;
                    clientUsernames[acceptSocket] = username; // Store the username

                    messagePacket.packetType = PACKET_TYPE_MSG_FROM_SERVER;
                    for (const auto& row : databaseQuery) {
                        messagePacket.length = static_cast<uint32_t>(row.message.length());
                        memcpy(messagePacket.data.message_from_server.text, row.message.c_str(), row.message.length());
                        memset(messagePacket.data.message_from_server.username, 0, 50);
                        memcpy(messagePacket.data.message_from_server.username, row.sender.c_str(), row.sender.length());

                        send(acceptSocket, reinterpret_cast<char*>(&messagePacket), sizeof(Packet), 0);
                    }
                }
                else {
                    const char* authFailure = "badauth";
                    send(acceptSocket, authFailure, strlen(authFailure), 0);
                }

            } else {
                // Accept a new message
                Packet packet;
                int byteCount = 0;
                byteCount = recv(sock, reinterpret_cast<char*>(&packet), sizeof(Packet), 0);

                if (byteCount <= 0) {
                    if (byteCount == 0) {
                        std::cout << "Client #" << sock << " disconnected." << std::endl;
                    } else {
                        std::cout << "Error with Client #" << sock << ": " << WSAGetLastError() << std::endl;
                    }
                    closesocket(sock);
                    FD_CLR(sock, &master);
                    continue;
                }

                if (packet.packetType == PACKET_TYPE_MSG_TO_SERVER) {
                    // Broadcast the message to connected clients
                    std::string username = clientUsernames[sock];
                    std::string str(packet.data.message_to_server, packet.data.message_to_server + packet.length);
                    std::cout << username << ": " << str << std::endl;
                    const int channelID = 1;
                    insertData(directoryDatabase, username, str, channelID);

                    Packet broadcastingPacket;
                    broadcastingPacket.packetType = PACKET_TYPE_MSG_FROM_SERVER;
                    broadcastingPacket.length = static_cast<uint32_t>(str.length());
                    memcpy(broadcastingPacket.data.message_from_server.text, str.c_str(), str.length());
                    memset(broadcastingPacket.data.message_from_server.username, 0, 50);
                    memcpy(broadcastingPacket.data.message_from_server.username, username.c_str(), username.length());

                    for (int j = 0; j < master.fd_count; j++) {
                        SOCKET outSock = master.fd_array[j];
                        if (outSock != serverSocket && outSock != sock) {
                            send(outSock, reinterpret_cast<char*>(&broadcastingPacket), sizeof(Packet), 0);
                        }
                    }

                } else if (packet.packetType == PACKET_TYPE_PICTURE) {
                    receivePicture(sock, packet);
                } else if (packet.packetType == PACKET_TYPE_VOICE_JOIN) {
                    // add user to voice
                }
            }
        }
    }

    // CLose socket
    system("pause");
    WSACleanup();
    return 0;
}
