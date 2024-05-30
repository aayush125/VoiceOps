#include "Networking.hpp"
#include "Window.hpp"

SOCKET createSocket(ServerInfo &server_info) {
    SOCKET clientSocket;

    clientSocket = INVALID_SOCKET;
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cout << "Error at Socket(): " << WSAGetLastError() << '\n';
        WSACleanup();
        return 0;
    } else {
        std::cout << "Socket() is OK!\n";
    }

    sockaddr_in clientService;
    clientService.sin_family = AF_INET;
    clientService.sin_port = htons(static_cast<u_short>(std::stoul(server_info.port)));
    InetPton(AF_INET, server_info.url.c_str(), &clientService.sin_addr.s_addr);
    std::cout << "Connecting to URL: " + server_info.url + " | Port: " + server_info.port << '\n';
    if (connect(clientSocket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
        std::cout << "Client:connect() - Failed to connect: " << WSAGetLastError() << '\n';
        return 0;
    } else {
        std::cout << "Client: connect() is OK!\n";
        std::cout << "Client: Can Start Sending and receiving data\n";
    }

    return clientSocket;
}

static gboolean idle_callback(gpointer user_data) {
    auto data = static_cast<std::pair<VoiceOpsWindow&, std::string>*>(user_data);
    VoiceOpsWindow& windowref = data->first;
    std::string message = data->second;

    windowref.add_new_message(message.c_str(), "Client Name");

    delete data;
    return G_SOURCE_REMOVE;
}

void ReceiveMessages(SOCKET& clientSocket, GMutex& mutex, DataStore& data, VoiceOpsWindow& windowref) {
    char buffer[4096];
    ZeroMemory(buffer, 4096);

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            // buffer[bytesReceived] = '\0';

            g_mutex_lock(&mutex);
            memcpy(data.buffer, buffer, bytesReceived);
            std::cout << "Received message: " << buffer << '\n';
            // Appending message to the chat box
            data.bytes = bytesReceived;
            g_mutex_unlock(&mutex);

            auto* user_data = new std::pair<VoiceOpsWindow&, std::string>(windowref, std::string(buffer, bytesReceived));
            g_idle_add(idle_callback, user_data);
        } else if (bytesReceived == 0) {
            std::cout << "Connection closed by server.\n";
            break;
        } else {
            std::cerr << "Error in receiving data from server: " << WSAGetLastError() << '\n';
            break;
        }
    }
}