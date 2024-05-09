#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <tchar.h>
#include <string>
#include <thread>

void ReceiveMessages(SOCKET clientSocket) {
	char buffer[4096];
	ZeroMemory(buffer, 4096);
	while (true) {
		int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
		if (bytesReceived > 0) {
			std::cout << std::string(buffer, bytesReceived) << std::endl;
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

void SendMessages(SOCKET clientSocket) {
	std::string userInput;
	do {
		std::getline(std::cin, userInput);
		if (!userInput.empty()) {
			int byteCount = send(clientSocket, userInput.c_str(), userInput.size(), 0);
			if (byteCount == SOCKET_ERROR) {
				std::cerr << "Error in sending data to server: " << WSAGetLastError() << std::endl;
				break;
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
		std::cout << "Client: connect() is OK!" << std::endl;
		std::cout << "Client: Can Start Sending and receiving data" << std::endl;
	}
	
	//Create a thread for receiving messages from the server
	std::thread receiveThread(ReceiveMessages, clientSocket);	
	SendMessages(clientSocket);

	//Join the thread to the main thread
	receiveThread.join();

	//std::string userInput;

	//do {
	//	std::cout << "PLease enter a message: ";
	//	getline(std::cin, userInput);
	//	if (userInput.size() > 0) {
	//		int byteCount = send(clientSocket, userInput.c_str(), userInput.size() + 1, 0);
	//		if (byteCount != SOCKET_ERROR) {
	//			//Wait for response
	//			int bytesReceived = recv(clientSocket, buffer, 4096, 0);
	//			if (bytesReceived > 0) {
	//				std::cout << std::string(buffer, 0, bytesReceived) << std::endl;
	//			}
	//		}
	//	}
	//} while (userInput.size() > 0);
	

	//Close the socket
	system("pause");
	WSACleanup();

	return 0;
}