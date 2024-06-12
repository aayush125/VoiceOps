#include "Networking.hpp"
#include "Window.hpp"
#include "common/text_packet.h"

void receivePicture(SOCKET socket, std::string username, VoiceOpsWindow& windowref) {
    std::vector<unsigned char> pictureData;

    int counter = 0;

    while (true) {
        Packet packet;
        int bytesReceived = recv(socket, reinterpret_cast<char*>(&packet), sizeof(Packet), 0);
        if (bytesReceived <= 0) {
            // Handle error or connection closed
            break;
        }

        if (packet.packetType != PACKET_TYPE_IMAGE) {
            // Handle unexpected packet type
            std::cout << "Unexpected packet type: " << packet.packetType << std::endl;
            std::cout << "Counter: " << counter << std::endl;
            continue;
        }

        pictureData.insert(pictureData.end(), packet.data.image, packet.data.image + packet.length);

        if (packet.length < MAX_PACKET_SIZE) {
            // Last packet received, stop receiving
            break;
        }

        counter++;
    }

    // Process the received picture data here
    
    // Create a Glib::MemoryInputStream from the PNG data
    Glib::RefPtr<Gio::MemoryInputStream> stream = Gio::MemoryInputStream::create();
    stream->add_data(&pictureData[0], pictureData.size(), nullptr);

    // Glib::RefPtr<Glib::Bytes> bytes = Glib::Bytes::create(png.data(), png.size());

    // Create a new Gdk::Pixbuf from the PNG data in the stream
    Glib::RefPtr<Gdk::Pixbuf> pixbuf = Gdk::Pixbuf::create_from_stream(stream);

    if (!pixbuf) {
        std::cout << "[receivePicture] Failed to create Gdk::Pixbuf from PNG data" << std::endl;
        return;
    }

    windowref.add_new_message(username, pixbuf);
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

    u_long mode = 1;
    ioctlsocket(*tcpSocket, FIONBIO, &mode);

    fd_set master;
    FD_ZERO(&master);
    FD_SET(*tcpSocket, &master);

    // Connect to server and bind :: Fill in hint structure, which server to connect to
    server.sin_family = AF_INET;
    server.sin_port = htons(static_cast<u_short>(std::stoul(server_info.port)));
    InetPton(AF_INET, server_info.url.c_str(), &server.sin_addr.s_addr);
    if (connect(*tcpSocket, (SOCKADDR*)&server, sizeof(server)) == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            std::cout << "[TCP] Client:connect()- Failed to connect. Error: " << WSAGetLastError() << std::endl;
            closesocket(*tcpSocket);
            return false;
        }
    }

    TIMEVAL timeout = {2, 0};
    select(0, nullptr, &master, nullptr, &timeout);
    FD_CLR(*tcpSocket, &master);

    mode = 0;
    ioctlsocket(*tcpSocket, FIONBIO, &mode);

    AuthPacket auth;

    std::string username = server_info.username;
    auth.uLength = static_cast<uint32_t>(username.length());
    memcpy(auth.username, username.c_str(), username.length());

    std::string password = "password";
    auth.pLength = static_cast<uint32_t>(password.length());
    memcpy(auth.password, password.c_str(), password.length());

    // Send authentication packet
    int bytes = send(*tcpSocket, reinterpret_cast<const char*>(&auth), sizeof(AuthPacket), 0);
    if (bytes <= 0) {
        std::cout << "[TCP] send() - Error: " << WSAGetLastError() << std::endl;
        closesocket(*tcpSocket);
        return false;
    }


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
    auto data = static_cast<std::tuple<VoiceOpsWindow&, std::string, std::string>*>(user_data);
    VoiceOpsWindow& windowref = std::get<0>(*data);
    std::string message = std::get<1>(*data);
    std::string username = std::get<2>(*data);

    windowref.add_new_message(username, message.c_str());

    delete data;
    return G_SOURCE_REMOVE;
}


void ReceiveMessages(SOCKET clientSocket, GMutex& mutex, DataStore& data, VoiceOpsWindow& windowref) {
    Packet receivePacket;
    while (true) {
        int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&receivePacket), sizeof(Packet), 0);
        if (bytesReceived > 0 && receivePacket.packetType == PACKET_TYPE_MSG_FROM_SERVER) {
            // g_mutex_lock(&mutex);
            // Appending message to the chat box
            std::string str(receivePacket.data.message_from_server.text, receivePacket.length);
            std::string username(receivePacket.data.message_from_server.username);
            
            // g_mutex_unlock(&mutex);

            std::cout << "Received message: " << str << '\n';

            auto* user_data = new std::tuple<VoiceOpsWindow&, std::string, std::string>(windowref, str, username);
            g_idle_add(idle_callback, user_data);
        } else if (bytesReceived > 0 && receivePacket.packetType == PACKET_TYPE_IMAGE_FROM_SERVER_FIRST_PACKET) {
            std::string username(receivePacket.data.image_sender);
            std::cout << "Receiving image from user: " << username << std::endl;
            receivePicture(clientSocket, username, windowref);
        } else if (bytesReceived == 0) {
            std::cout << "Connection closed by server." << std::endl;
            break;
        } else {
            std::cerr << "Error in receiving data from server: " << WSAGetLastError() << std::endl;
            // break;
        }
    }
}
