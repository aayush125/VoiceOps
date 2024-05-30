#include <algorithm>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <iostream>
#include <tchar.h>
#include <string>
#include <thread>
#include <cstdint>
#include <vector>
#include "file.h"
#define MAX_PACKET_SIZE 1500
#define min(a,b) (a < b ? a : b)



std::string pictureFilename = "C:\\Users\\bhatt\\OneDrive\\Pictures\\Screenshots\\file.png";


void ReceiveMessages(SOCKET clientSocket) {

    Packet receivePacket;
    while (true) {
        int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&receivePacket), sizeof(Packet), 0);
        if (bytesReceived > 0 && receivePacket.packetType == PACKET_TYPE_STRING) {
            std::string str(receivePacket.data, receivePacket.data + receivePacket.length);
            std::cout << str << std::endl;
        }
        else if (bytesReceived == 0) {
            std::cout << "Connection closed by server." << std::endl;
            break;
        }
        else {
            std::cerr << "Error in receiving data from server: " << WSAGetLastError() << std::endl;
            break;
        }
    }
}

void sendPicture(const std::string& filename, SOCKET socket) {
    std::vector<unsigned char> pictureData = loadPicture(filename);
    std::cout << pictureData.size() << std::endl;
    if (pictureData.empty()) {
        // Handle error
        return;
    }

    const uint32_t packetType = PACKET_TYPE_PICTURE;
    const size_t dataSize = pictureData.size();
    const size_t numPackets = (dataSize + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;

    for (size_t i = 0; i < numPackets; ++i) {
        Packet packet;
        packet.packetType = packetType;
        packet.length = static_cast<uint32_t>(min(dataSize - i * MAX_PACKET_SIZE, MAX_PACKET_SIZE));
        std::memcpy(packet.data, pictureData.data() + i * MAX_PACKET_SIZE, packet.length);

        // Send the packet over the network
        send(socket, reinterpret_cast<const char*>(&packet), sizeof(Packet), 0);
    }
}


void SendMessages(SOCKET clientSocket) {
    std::string userInput;
    Packet packet;
    do {
        std::getline(std::cin, userInput);
        packet.packetType = PACKET_TYPE_STRING;
        if (!userInput.empty()) {
            if (std::strcmp(userInput.c_str(), "/image") == 0){
                sendPicture(pictureFilename, clientSocket);
            }
            else {
                packet.length = static_cast<uint32_t>(userInput.length());
                std::memcpy(packet.data, userInput.c_str(), userInput.length());
                int byteCount = send(clientSocket, reinterpret_cast<const char*>(&packet), sizeof(Packet), 0);
                if (byteCount == SOCKET_ERROR) {
                    std::cerr << "Error in sending data to server: " << WSAGetLastError() << std::endl;
                    break;
                }
            }
        }
    } while (!userInput.empty());
}


int main(int argc, char* argv[]) {
    SOCKET clientSocket;
    AuthPacket auth;
    int port = 55555;
    std::cout << "Username:";
    std::string userInput;
    std::getline(std::cin, userInput);
    auth.uLength = static_cast<uint32_t>(userInput.length());
    std::memcpy(auth.username, userInput.c_str(), userInput.length());
    std::cout << "Password:";
    std::getline(std::cin, userInput);
    auth.pLength = static_cast<uint32_t>(userInput.length());
    std::memcpy(auth.password, userInput.c_str(), userInput.length());

    //Loading the dll file
    WSADATA wsaData;
    int wsaerr;
    WORD wVersionRequested = MAKEWORD(2, 2);
    wsaerr = WSAStartup(wVersionRequested, &wsaData);
    if (wsaerr != 0) {
        std::cout << "The winsock dll not found!" << std::endl;
        return 0;
    }
  

    //Setup Client Socket
    clientSocket = INVALID_SOCKET;
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cout << "Error at Socket(): " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 0;
    }
   

    //Connect to server and bind :: Fill in hint structure, which server to connect to
    sockaddr_in clientService;
    clientService.sin_family = AF_INET;
    clientService.sin_port = htons(port);
    InetPton(AF_INET, _T("127.0.0.1"), &clientService.sin_addr.s_addr);
    if (connect(clientSocket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
        std::cout << "Client:connect()- Failed to connect." << std::endl;
        WSACleanup();
        return 0;
    }
    
    //Send authentication packet
    send(clientSocket, reinterpret_cast<const char*>(&auth) , sizeof(AuthPacket), 0);

    // Receive server response
    char buffer[256];
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        if (strcmp(buffer, "goodauth") == 0) {
            std::cout << "Server response: " << buffer << std::endl;
        }
        else {
            std::cout << "Incorrect Password" << std::endl;
            closesocket(clientSocket);
            WSACleanup();
            return 0;
        }
    }

    //Create a thread for receiving messages from the server
    std::thread receiveThread(ReceiveMessages, clientSocket);	
    SendMessages(clientSocket);

    //Join the thread to the main thread
    receiveThread.join();


    //Close the socket
    system("pause");
    WSACleanup();

    return 0;
}