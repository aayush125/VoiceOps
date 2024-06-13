#include "Networking.hpp"
#include "Window.hpp"
#include "common/text_packet.h"

void receivePicture(SOCKET socket, std::string username, VoiceOpsWindow& windowref) {
    std::vector<unsigned char> pictureData;

    int counter = 0;

    while (true) {
        counter++;
        
        Packet packet;

        ReceiveResult res = recv_pkt(socket, packet);

        if (res == RECEIVE_RESULT_CONN_CLOSED) {
            std::cout << "[receivePicture] Connection closed by server." << std::endl;
            return;
        }

        if (res == RECEIVE_RESULT_ERROR) {
            std::cerr << "[receivePicture] Error in receiving data from server: " << WSAGetLastError() << std::endl;
            return;
        }

        if (packet.packetType != PACKET_TYPE_IMAGE) {
            if (packet.packetType == PACKET_TYPE_IMAGE_FAILURE) {
                std::cout << "Image Failure Packet received. Aborting." << std::endl;
                return;
            }
            // Handle unexpected packet type
            std::cout << "Unexpected packet type: " << packet.packetType << std::endl;
            std::cout << "Counter: " << counter << std::endl;
            return;
        }

        pictureData.insert(pictureData.end(), packet.data.image, packet.data.image + packet.length);

        if (packet.length < MAX_PACKET_SIZE) {
            // Last packet received, stop receiving
            break;
        }
    }

    // Process the received picture data here
    std::cout << "Receive picture of size: " << pictureData.size() << std::endl;

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

    Packet pkt;
    pkt.packetType = PACKET_TYPE_AUTH_REQUEST;
    auto& req = pkt.data.auth_request;

    std::string username = server_info.username;
    req.uLength = username.length();
    memcpy(req.username, username.c_str(), username.length());

    std::string password = server_info.pass;
    req.pLength = password.length();
    memcpy(req.password, password.c_str(), password.length());

    // Send authentication packet
    int bytes = send(*tcpSocket, reinterpret_cast<const char*>(&pkt), sizeof(Packet), 0);
    if (bytes <= 0) {
        std::cout << "[TCP] send() - Error: " << WSAGetLastError() << std::endl;
        closesocket(*tcpSocket);
        return false;
    }

    ReceiveResult res = recv_pkt(*tcpSocket, pkt);

    if (res == RECEIVE_RESULT_SUCCESS) {
        if (pkt.data.auth_response) {
            std::cout << "Authenticated by server!" << std::endl;
        } else {
            std::cout << "Incorrect Password" << std::endl;
            closesocket(*tcpSocket);
            return false;
        }
    } else {
        if (res == RECEIVE_RESULT_ERROR) {
            std::cout << "Error: " << WSAGetLastError() << std::endl;
        }
        return false;
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
        ReceiveResult res = recv_pkt(clientSocket, receivePacket);

        if (res == RECEIVE_RESULT_CONN_CLOSED) {
            std::cout << "[ReceiveMessages] Connection closed by server." << std::endl;
            windowref.disconnect();
            break;
        }

        if (res == RECEIVE_RESULT_ERROR) {
            int error_code = WSAGetLastError();
            if (error_code != WSAECONNABORTED && error_code != WSAENOTSOCK) {
                std::cerr << "[ReceiveMessages] Error in receiving data from server: " << error_code << std::endl;
                windowref.disconnect();
            }
            break;
        }

        if (receivePacket.packetType == PACKET_TYPE_MSG_FROM_SERVER) {
            std::string str(receivePacket.data.message_from_server.text, receivePacket.length);
            std::string username(receivePacket.data.message_from_server.username);
            std::cout << "Received message: " << str << std::endl;
            auto* user_data = new std::tuple<VoiceOpsWindow&, std::string, std::string>(windowref, str, username);
            g_idle_add(idle_callback, user_data);
            continue;
        }

        if (receivePacket.packetType == PACKET_TYPE_IMAGE_FROM_SERVER_FIRST_PACKET) {
            std::string username(receivePacket.data.image_sender, receivePacket.length);
            std::cout << "Receiving image from user: " << username << std::endl;
            receivePicture(clientSocket, username, windowref);
        }
    }
}
