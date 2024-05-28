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
	int port = 55555;

	//Loading the dll file
	WSADATA wsaData;
	int wsaerr;
	WORD wVersionRequested = MAKEWORD(2, 2);
	wsaerr = WSAStartup(wVersionRequested, &wsaData);
	if (wsaerr != 0) {
		std::cout << "The winsock dll not found!" << std::endl;
		return 0;
	}
	else {
		std::cout << "The winsock dll was found!" << std::endl;
		std::cout << "Status: " << wsaData.szSystemStatus << std::endl;
	}

	//Setup Client Socket
	clientSocket = INVALID_SOCKET;
	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clientSocket == INVALID_SOCKET) {
		std::cout << "Error at Socket(): " << WSAGetLastError() << std::endl;
		WSACleanup();
		return 0;
	}
	else {
		std::cout << "Socket() is OK!" << std::endl;
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
	else {
		//Send the username to the server
		Packet packet, passwordPacket;
		packet.packetType = PACKET_TYPE_STRING;
		std::cout << "Username:";
		std::string userInput;
		std::getline(std::cin, userInput);
		packet.length = static_cast<uint32_t>(userInput.length());
		std::memcpy(packet.data, userInput.c_str(), userInput.length());
		int byteCount = send(clientSocket, reinterpret_cast<const char*>(&packet), sizeof(Packet), 0);
		if (byteCount == SOCKET_ERROR) {
			std::cerr << "Error in sending data to server: " << WSAGetLastError() << std::endl;
		}
		std::cout << "Password:";
		std::getline(std::cin, userInput);

		//Receive password from the server
		byteCount = recv(clientSocket, reinterpret_cast<char*>(&passwordPacket), sizeof(Packet), 0);
		if (byteCount == SOCKET_ERROR) {
			std::cerr << "Error in Receiving data to server: " << WSAGetLastError() << std::endl;
		}
		std::string password(passwordPacket.data, passwordPacket.data + passwordPacket.length);
		if (std::strcmp(userInput.c_str(), password.c_str()) != 0) {
			std::cout << "Incorrect password!" << std::endl;
			WSACleanup();
			return 0;
		}
		std::cout << "Client: connect() is OK!" << std::endl;
		std::cout << "Client: Can Start Sending and receiving data" << std::endl;
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