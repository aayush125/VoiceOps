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
#define MAX_PACKET_SIZE 1500

std::vector<data> databaseQuery;
//Map socket to usernames
std::map<SOCKET, std::string> clientUsernames;

enum PacketType {
    PACKET_TYPE_STRING = 1,
    PACKET_TYPE_TEXT = 2,
    PACKET_TYPE_PICTURE = 3
};

struct Packet {
    uint32_t packetType;
    uint32_t length;
    char data[MAX_PACKET_SIZE];
};

struct AuthPacket {
    char username[50];
    char password[50];
    uint32_t uLength;
    uint32_t pLength;
};


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

    // Process the received picture data
    std::cout << pictureData.size() << std::endl;
    if (!pictureData.empty()) {
        // Save or display the picture data
        std::cout << "I received the image i guess" << std::endl;
        std::string outputFilename = "output.jpg";
        if (savePicture(pictureData, outputFilename)) {
            std::cout << "Picture saved to " << outputFilename << std::endl;
        }
        else {
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
    SOCKET serverSocket, acceptSocket;
    int port = 55555;
    int MAXCONN = 30;
    int queryMessagesLimit = 5;
    //Loading the database file
    std::string directoryDatabaseString = "C:\\Codes\\C++ Socket Connection - Server\\Database\\" + std::to_string(port) + ".db";
    const char* directoryDatabase = directoryDatabaseString.c_str();

    createDB(directoryDatabase);
    createTable(directoryDatabase);
    databaseQuery = selectData(directoryDatabase,queryMessagesLimit);
    
    //Loading the dll file
    WSADATA wsaData;
    int wsaerr;
    WORD wVersionRequested = MAKEWORD(2, 2);
    wsaerr = WSAStartup(wVersionRequested, &wsaData);
    if (wsaerr != 0) {
        std::cout << "The winsock dll not found!" << std::endl;
        return 0;
    }
    

    //Setup server Socket
    serverSocket = INVALID_SOCKET;
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cout << "Error at Socket(): " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 0;
    }

    //Bind the ip and port number to the socket
    sockaddr_in service;
    service.sin_family = AF_INET;
    
    InetPton(AF_INET, _T("0.0.0.0"), &service.sin_addr.s_addr);
    service.sin_port = htons(port);
    if (bind(serverSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        std::cout << "Bind() failed!" << WSAGetLastError()<< std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 0;
    }

    //Listen on the socket

    if(listen(serverSocket, MAXCONN) == SOCKET_ERROR) {
        std::cout << "Listen(): Error listening on socket!" << WSAGetLastError() << std::endl;
    }
    else {
        std::cout << "Server Initialised, I am waiting Connections" << std::endl;
    }
    

    fd_set master;
    FD_ZERO(&master);


    FD_SET(serverSocket, &master);

    while (true) {

        fd_set copy = master;
        int socketCount = select(0, &copy, nullptr, nullptr, nullptr);
        
        
        for (int i = 0; i < socketCount;i++) {
            SOCKET sock = copy.fd_array[i];
            if (sock == serverSocket) {

                //Accept a new Connection
                acceptSocket = accept(serverSocket, nullptr, nullptr);
                if (acceptSocket == INVALID_SOCKET) {
                    std::cout << "Accept Failed: " << WSAGetLastError() << std::endl;
                    WSACleanup();
                    return -1;
                }
                else {
                    std::cout << "Accepted Connection" << std::endl;
                }
                
                std::string username;
                //Add the new connection to the list of connected Clients
                if (handleNewConnection(acceptSocket, username)) {
                    Packet messagePacket;
                    const char* authSuccess = "goodauth";
                    send(acceptSocket, authSuccess, strlen(authSuccess), 0);
                    FD_SET(acceptSocket, &master);
                    std::cout << "The username outside handleconnection is " << username << std::endl;
                    clientUsernames[acceptSocket] = username; // Store the username

                    messagePacket.packetType = PACKET_TYPE_STRING;
                    for (const auto& row : databaseQuery) {
                        std::ostringstream sendThis;
                        sendThis << row.sender << ":" << row.message << "\r\n";
                        std::string outMessage = sendThis.str();
                        messagePacket.length = static_cast<uint32_t>(outMessage.length());
                        std::memcpy(messagePacket.data, outMessage.c_str(), outMessage.length());
                        send(acceptSocket, reinterpret_cast<char*>(&messagePacket), sizeof(Packet), 0);
                    }
                }
                else {
                    const char* authFailure = "badauth";
                    send(acceptSocket, authFailure, strlen(authFailure), 0);
                }


            }
            else {
                //Accept a new message
                Packet packet;
                int byteCount = 0;
                byteCount = recv(sock, reinterpret_cast<char*>(&packet), sizeof(Packet), 0);
                
                if (byteCount > 0 && packet.packetType == PACKET_TYPE_STRING) {
                    //Broadcast the message to connected clients
                    std::string username = clientUsernames[sock];
                    std::string str(packet.data, packet.data + packet.length);
                    std::cout << username<< str << std::endl;
                    const int channelID = 1;
                    std::cout << str << std::endl;
                    insertData(directoryDatabase, username, str, channelID);

                    for (int j = 0; j < master.fd_count; j++) {
                        SOCKET outSock = master.fd_array[j];
                        if (outSock != serverSocket && outSock != sock) {
                            std::ostringstream ss;
                            ss <<username<< ":" << str << "\r\n";
                            std::string strOut = ss.str();
                            Packet broadcastingPacket;
                            broadcastingPacket.packetType = packet.packetType;
                            broadcastingPacket.length = static_cast<uint32_t>(strOut.length());
                            std::memcpy(broadcastingPacket.data, strOut.c_str(), strOut.length());
                            send(outSock, reinterpret_cast<char*>(&broadcastingPacket), sizeof(Packet), 0);
                        }
                    }

                }
                else if(packet.packetType == PACKET_TYPE_PICTURE){
                    receivePicture(sock, packet);
                }
                else {
                    closesocket(sock);
                    FD_CLR(sock, &master);
                }
            }
        }
    }

    //CLose socket
    system("pause");
    WSACleanup();
    return 0;
}