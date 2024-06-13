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
#include <fstream>
#include <cstdint>
#include "database.h"
#include <server/voice_server.h>
#include <common/text_packet.h>

std::vector<data> databaseQuery;
// Map socket to usernames
std::map<SOCKET, std::string> clientUsernames;

std::string server_password;

bool handleNewConnection(SOCKET clientSocket, std::string& username) {
    // Receive authentication packet
    Packet authPacket;
    ReceiveResult res = recv_pkt(clientSocket, authPacket);

    if (res != RECEIVE_RESULT_SUCCESS) {
        return false;
    }

    if (authPacket.packetType == PACKET_TYPE_AUTH_REQUEST) {
        auto& auth_data = authPacket.data.auth_request;

        std::string password(auth_data.password, auth_data.password + auth_data.pLength);
        username = std::string(auth_data.username, auth_data.username + auth_data.uLength);

        // TODO: ensure username is unique

        std::cout << "Expected: " << server_password << " | Received: " << password << std::endl;

        if (server_password.compare(password) == 0) {
            std::cout << "Passwords matched" << std::endl;
            return true;
        }
    }
    return false;
}

void receivePicture(SOCKET sock, Packet initialPacket, const std::string& username, fd_set& master, SOCKET &serverSocket) {
    Packet sendPacket;
    sendPacket.packetType = PACKET_TYPE_IMAGE_FROM_SERVER_FIRST_PACKET;
    sendPacket.length = username.length();
    memset(sendPacket.data.image_sender, 0, 50);
    memcpy(sendPacket.data.image_sender, username.c_str(), username.length());

    for (int j = 0; j < master.fd_count; j++) {
        SOCKET outSock = master.fd_array[j];
        if (outSock != serverSocket && outSock != sock) {
            send(outSock, reinterpret_cast<char*>(&sendPacket), sizeof(Packet), 0);
            send(outSock, reinterpret_cast<char*>(&initialPacket), sizeof(Packet), 0);
        }
    }

    bool failed = false;
    
    while (true) {
        Packet packet;

        ReceiveResult res = recv_pkt(sock, packet);

        if (res == RECEIVE_RESULT_CONN_CLOSED) {
            failed = true;
            break;
        }

        if (res == RECEIVE_RESULT_ERROR) {
            std::cout << "[receivePicture] Error while receiving: " << WSAGetLastError() << std::endl;
            failed = true;
            break;
        }

        if (packet.packetType != PACKET_TYPE_IMAGE) {
            // Handle unexpected packet type
            std::cout << "Received non-image packet. Aborting." << std::endl;
            failed = true;
            break;
        }

        for (int j = 0; j < master.fd_count; j++) {
            SOCKET outSock = master.fd_array[j];
            if (outSock != serverSocket && outSock != sock) {
                send(outSock, reinterpret_cast<char*>(&packet), sizeof(Packet), 0);
            }
        }

        if (packet.length < MAX_PACKET_SIZE) {
            // Last packet received, stop receiving
            break;
        }
    }

    if (failed) {
        sendPacket.packetType = PACKET_TYPE_IMAGE_FAILURE;
        for (int j = 0; j < master.fd_count; j++) {
            SOCKET outSock = master.fd_array[j];
            if (outSock != serverSocket && outSock != sock) {
                send(outSock, reinterpret_cast<char*>(&sendPacket), sizeof(Packet), 0);
            }
        }
    }

}

int main(int argc, char* argv[]) {    
    std::string filename = "server_pw.ini";
    std::ifstream file(filename);

    if (file) {
        std::getline(file, server_password);
        file.close();
    } else {
        std::cout << "Create server password: ";
        std::getline(std::cin, server_password);

        std::ofstream file(filename);
        file << server_password;
        file.close();
    }
    
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
                    FD_SET(acceptSocket, &master);
                    
                    Packet pkt;
                    pkt.packetType = PACKET_TYPE_AUTH_RESPONSE;
                    pkt.data.auth_response = true;
                    send(acceptSocket, reinterpret_cast<char*>(&pkt), sizeof(Packet), 0);
                    
                    std::cout << "The username outside handleconnection is " << username << std::endl;
                    clientUsernames[acceptSocket] = username; // Store the username

                    /*
                    pkt.packetType = PACKET_TYPE_MSG_FROM_SERVER;
                    for (const auto& row : databaseQuery) {
                        messagePacket.length = static_cast<uint32_t>(row.message.length());
                        memcpy(messagePacket.data.message_from_server.text, row.message.c_str(), row.message.length());
                        memset(messagePacket.data.message_from_server.username, 0, 50);
                        memcpy(messagePacket.data.message_from_server.username, row.sender.c_str(), row.sender.length());

                        send(acceptSocket, reinterpret_cast<char*>(&messagePacket), sizeof(Packet), 0);
                    }
                    */
                } else {
                    Packet pkt;
                    pkt.packetType = PACKET_TYPE_AUTH_RESPONSE;
                    pkt.data.auth_response = false;
                    send(acceptSocket, reinterpret_cast<char*>(&pkt), sizeof(Packet), 0);
                }

            } else {
                // Accept a new message
                Packet packet;
                // int byteCount = 0;
                // byteCount = recv(sock, reinterpret_cast<char*>(&packet), sizeof(Packet), 0);

                ReceiveResult res = recv_pkt(sock, packet);

                if (res != RECEIVE_RESULT_SUCCESS) {
                    if (res = RECEIVE_RESULT_CONN_CLOSED) {
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

                } else if (packet.packetType == PACKET_TYPE_IMAGE) {
                    // Broadcast the picture packets to other clients
                    std::string& username = clientUsernames[sock];
                    receivePicture(sock, packet, username, master, serverSocket);
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
