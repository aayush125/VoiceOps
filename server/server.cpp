#include <WinSock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <filesystem>
#include <tchar.h>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
// #include <sqlite3.h>

//Creates the database if not available
// static int createDB(const char* databasePath) {
// 	sqlite3* DB;
// 	int exit = sqlite3_open(databasePath, &DB);
// 	sqlite3_close(DB);
// 	return 0;
// }

// static int createTable(const char* databasePath) {
// 	sqlite3* DB;
// 	std::string sqlCommand = "Create TABLE IF NOT EXISTS MESSAGES("
// 		"ID INTEGER PRIMARY KEY AUTOINCREMENT,"
// 		"SENDER		TEXT NOT NULL,"
// 		"MESSAGE	TEXT NOT NULL,"
// 		"CHANNELID	INTEGER NOT NULL);";

// 	try {
// 		int exit = 0;
// 		exit = sqlite3_open(databasePath, &DB);

// 		char* messageError;
// 		exit = sqlite3_exec(DB, sqlCommand.c_str(), NULL, 0, &messageError);

// 		if (exit != SQLITE_OK) {
// 			std::cerr << "Error Creating a table" << std::endl;
// 			sqlite3_free(messageError);
// 		}
// 		sqlite3_close(DB);
// 	}
// 	catch (const std::exception & e) {
// 		std::cerr << e.what() << std::endl;
// 	}
// 	return 0;
// }

// static int insertData(const char* databasePath, std::string clientID, std::string message, int channelID) {
// 	sqlite3* DB;
// 	char* messageError;
// 	int exit = sqlite3_open(databasePath, &DB);

// 	std::string sql = "INSERT INTO MESSAGES (SENDER, MESSAGE) VALUES('" + clientID + "','" + message + "'," +  std::to_string(channelID) + ");";
// 	std::cout << sql.c_str() << std::endl;

// 	exit = sqlite3_exec(DB, sql.c_str(), NULL, 0, &messageError);

// 	if (exit != SQLITE_OK) {
// 		std::cerr << "Error Inserting data" << messageError << std::endl;
// 		sqlite3_free(messageError);
// 	}
// 	return 0;
// }

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
  char buffer[4096];
  ZeroMemory(buffer, 4096);
  //Loading the database file
  // const char* directoryDatabase = "C:\\Codes\\C++ Socket Connection - Server\\Database\\server.db";
  // createDB(directoryDatabase);
  // createTable(directoryDatabase);
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

        //Add the new connection to the list of connected Clients
        FD_SET(acceptSocket, &master);

        //Send a welcome message to the connected client
        // std::string  welcomeMsg = "Welcome to the Server!";
        // send(acceptSocket, welcomeMsg.c_str(), welcomeMsg.size(), 0);
      }
      else {
        //Accept a new message
        memset(buffer, 0, sizeof(buffer));
        int byteCount = 0;
        byteCount = recv(sock, buffer, 4096, 0);
        if (byteCount > 0) {
          //Broadcast the message to connected clients
          auto str = std::string(buffer, byteCount);
          
          std::ostringstream clientId;
          clientId << "Client #" << sock;
          std::cout << clientId.str()<< str << std::endl;
          const int channelID = 1;
          //insertData(directoryDatabase, clientId.str(), buffer, channelID);
          for (int j = 0; j < master.fd_count; j++) {
            SOCKET outSock = master.fd_array[j];
            if (outSock != serverSocket && outSock != sock) {
              std::ostringstream ss;
              ss <<clientId.str() << ": " << str;
              std::string strOut = ss.str();
              send(outSock, strOut.c_str(), strOut.size(), 0);
              // std::cout << byteCount << std::endl;
              // send(outSock, buffer, byteCount, 0);
            }
          }
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