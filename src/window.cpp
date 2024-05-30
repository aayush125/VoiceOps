#include <iostream>

#include "Dialogs.hpp"
#include "Window.hpp"
#include <ws2tcpip.h>

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_GENERATION
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_WASAPI
#define MA_NO_DECODING
#define MA_NO_ENCODING
// #define MA_NO_NODE_GRAPH
#include <thirdparty/miniaudio.h>

#include <common/text_packet.h>

#include <time.h>

struct DataStore {
    char buffer[4096];
    size_t bytes;
};

static Glib::RefPtr<Gtk::TextBuffer> chatHistory;
static DataStore data;
static GMutex mutex;

static SOCKET clientUDPSocket;

static sockaddr_in server;

gboolean update_textbuffer(void*) {
    // g_mutex_lock(&mutex);
    auto msg = std::string(data.buffer, data.bytes);
    g_mutex_unlock(&mutex);
    std::cout << "Incoming: " << msg.c_str() << std::endl; // .c_str seems to fix emoji crash
    chatHistory->place_cursor(chatHistory->end());
    chatHistory->insert_at_cursor(msg);
    chatHistory->insert_at_cursor("\n");
    return G_SOURCE_REMOVE;
}

bool createSocket(ServerInfo& server_info, SOCKET* tcpSocket, SOCKET* udpSocket) {

    // Setup Client Socket
    *tcpSocket = INVALID_SOCKET;
    *tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*tcpSocket == INVALID_SOCKET) {
        std::cout << "[TCP] Error at Socket(): " << WSAGetLastError() << std::endl;
        closesocket(*tcpSocket);
        return false;
    } else {
        std::cout << "[TCP] Socket() is OK!" << std::endl;
    }

    // Connect to server and bind :: Fill in hint structure, which server to connect to
    server.sin_family = AF_INET;
    server.sin_port = htons(static_cast<u_short>(std::stoul(server_info.port)));
    InetPton(AF_INET, server_info.url.c_str(), &server.sin_addr.s_addr);
    if (connect(*tcpSocket, (SOCKADDR*)&server, sizeof(server)) == SOCKET_ERROR) {
        std::cout << "[TCP] Client:connect()- Failed to connect." << std::endl;
        closesocket(*tcpSocket);
        return false;
    } else {
        // Send the username to the server
        Packet packet, passwordPacket;
        packet.packetType = PACKET_TYPE_STRING;

        // Todo (@kripesh101 | @aayush125): add a proper username field to ServerInfo class
        std::string userInput = server_info.name;
        packet.length = static_cast<uint32_t>(userInput.length());
        memcpy(packet.data, userInput.c_str(), userInput.length());

        // Todo (@aveens13 | @kripesh101): sizeof(Packet) is overkill.
        int byteCount = send(*tcpSocket, reinterpret_cast<const char*>(&packet), sizeof(Packet), 0);
        if (byteCount == SOCKET_ERROR) {
            std::cerr << "Error in sending data to server: " << WSAGetLastError() << std::endl;
        }

        // [Critical] Todo (@aveens13) - fix: client-side authentication???

        // Receive password from the server
        byteCount = recv(*tcpSocket, reinterpret_cast<char*>(&passwordPacket), sizeof(Packet), 0);
        if (byteCount == SOCKET_ERROR) {
            std::cerr << "Error in Receiving data to server: " << WSAGetLastError() << std::endl;
        }
        std::string password(passwordPacket.data, passwordPacket.data + passwordPacket.length);
        
        // Validate password regardless of what's received
        
        std::cout << "[TCP] Client: connect() is OK!" << std::endl;
        std::cout << "[TCP] Client: Can Start Sending and receiving data" << std::endl;
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

    return true;
}

void ReceiveMessages(SOCKET clientSocket) {
    Packet receivePacket;
    while (true) {
        int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&receivePacket), sizeof(Packet), 0);
        if (bytesReceived > 0 && receivePacket.packetType == PACKET_TYPE_STRING) {
            // std::string str(receivePacket.data, receivePacket.data + receivePacket.length);
            // std::cout << str << std::endl;
            g_mutex_lock(&mutex);
            memcpy(data.buffer, receivePacket.data, receivePacket.length);
            data.bytes = receivePacket.length;
            // g_mutex_unlock(&mutex);
            g_idle_add(update_textbuffer, NULL);
        } else if (bytesReceived == 0) {
            std::cout << "Connection closed by server." << std::endl;
            break;
        } else {
            std::cerr << "Error in receiving data from server: " << WSAGetLastError() << std::endl;
            break;
        }
    }
}

#include <thirdparty/opus/opus.h>
#include <common/jitter_buffer.h>

struct ClientData {
    uint32_t prev_consumed_pkt = 0;

    OpusDecoder* dec;
    JitterBuffer jb;
};

#define MAX_CLIENTS 10
struct Clients {
    ClientData data[MAX_CLIENTS];
};

static Clients* clients_ptr;

static OpusEncoder* enc;

static ma_device device;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    static uint32_t current_pkt_number = 1;

    if (frameCount != 480) {
        std::cout << "[WARNING] frameCount is not 480! It is " << frameCount << " instead." << std::endl;
        return;
    }

    VoicePacketToServer pkt;
    uint16_t size;
    opus_int32 workingBuffer[960] = { 0 };
    opus_int16 tempStore[960];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        auto& current = clients_ptr->data[i];
        if (current.jb.get(&pkt, &size)) {
            // handle pkt loss
            if (current.prev_consumed_pkt != 0) {
                for (uint32_t i2 = current.prev_consumed_pkt + 1; i2 < pkt.packet_number; i2++) {
                    std::cout << "Packet loss - BAD!" << std::endl;

                    int len = opus_decode(current.dec, NULL, 0, tempStore, 480, 0);
                    if (len < 0) {
                        std::cout << "OPUS ERROR: " << len << std::endl;
                    }
                }
            }
            current.prev_consumed_pkt = pkt.packet_number;

            int len = opus_decode(current.dec, pkt.encoded_data, size, tempStore, 480, 0);
            if (len < 0) {
                std::cout << "OPUS ERROR: " << len << std::endl;
            }

            // Mixing
            for (uint16_t j = 0; j < 960; j++) {
                workingBuffer[j] += (opus_int32)tempStore[j];
            }
        }
    }
    ma_clip_samples_s16((ma_int16*)pOutput, workingBuffer, 960);

    // Outgoing part below:
    VoicePacketToServer outgoing_pkt;
    outgoing_pkt.packet_number = current_pkt_number;

    int len = opus_encode(enc, (const opus_int16*)pInput, frameCount, outgoing_pkt.encoded_data, 400);

    int iResult = sendto(clientUDPSocket, (const char*)&outgoing_pkt, len + 4, 0, (SOCKADDR*)&server, sizeof(server));
    if (iResult == SOCKET_ERROR) {
        printf("sendto() failed with error code : %d", WSAGetLastError());
    }

    current_pkt_number++;
}

void ReceiveVoice(SOCKET voiceSocket) {
    Clients clients;
    clients_ptr = &clients;

    VoicePacketFromServer incoming_pkt;

    int error;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients.data[i].dec = opus_decoder_create(48000, 2, &error);
        if (error) {
            std::cout << "Error while creating Opus Decoder #" << i << std::endl;
            return;
        }
    }

    enc = opus_encoder_create(48000, 2, OPUS_APPLICATION_VOIP, &error);
    if (error) {
        std::cout << "Error while creating Opus Encoder" << std::endl;
        return;
    }

    ma_device_start(&device);

    while (true) {
        int recv_len = recv(voiceSocket, (char*)&incoming_pkt, sizeof(VoicePacketFromServer), 0);
        if (recv_len == SOCKET_ERROR) {
            std::cout << "recv() failed with error code  " << WSAGetLastError() << std::endl;
            continue;
        }

        VoicePacketToServer pkt;
        // Byte alignment makes this - 8
        memcpy(pkt.encoded_data, incoming_pkt.encoded_data, recv_len - 8);
        pkt.packet_number = incoming_pkt.packet_number;

        clients.data[incoming_pkt.userID].jb.insert(&pkt, recv_len - 8); // - 4 bytes is of packet/sequence number
    }
}

VoiceOpsWindow::VoiceOpsWindow() {
    set_title("VoiceOps");
    set_name("main-window");

    chatHistory = Gtk::TextBuffer::create();

    int rc;

    rc = sqlite3_open("./database/voiceops.db", &mDBHandle);

    if (rc) {
        std::cout << "Can't open database: " << sqlite3_errmsg(mDBHandle);
    } else {
        std::cout << "Opened database successfully!\n";
        setup_database();
    }

    auto hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);

    server_list_panel();
    server_content_panel(false);

    if (mServerListBox)
        hbox->append(*mServerListBox);
    else
        std::cerr << "problem 1\n";
    if (mServerContentBox)
        hbox->append(*mServerContentBox);
    else
        std::cerr << "problem 2\n";

    set_child(*hbox);
    set_default_size(900, 900);

    // auto children = hbox->get_last_child();
    // mServerContentBox = dynamic_cast<Gtk::Box*>(children);

    mServerContentBox->set_name("content-box");
    mServerListBox->set_name("list-box");

    // MiniAudio initialization
    ma_result maResult;
    ma_device_config deviceConfig;

    deviceConfig = ma_device_config_init(ma_device_type_duplex);
    deviceConfig.capture.pDeviceID = NULL;
    deviceConfig.capture.format = ma_format_s16;
    deviceConfig.capture.channels = 2;
    deviceConfig.capture.shareMode = ma_share_mode_shared;
    deviceConfig.playback.pDeviceID = NULL;
    deviceConfig.playback.format = ma_format_s16;
    deviceConfig.playback.channels = 2;
    deviceConfig.dataCallback = data_callback;
    deviceConfig.noClip = TRUE;
    deviceConfig.wasapi.noAutoConvertSRC = TRUE;
    deviceConfig.noPreSilencedOutputBuffer = TRUE;
    deviceConfig.periodSizeInFrames = 480;
    maResult = ma_device_init(NULL, &deviceConfig, &device);
    if (maResult != MA_SUCCESS) {
        std::cout << "Error initializing audio device" << std::endl;
    }
}

void VoiceOpsWindow::setup_database() {
    char* zErrMsg = 0;
    int rc;

    const char* sql = "CREATE TABLE IF NOT EXISTS SERVER_LIST(" \
        "SERVER_NAME    TEXT," \
        "SERVER_URL     TEXT    NOT NULL," \
        "SERVER_PORT    TEXT    NOT NULL);";
    
    rc = sqlite3_exec(mDBHandle, sql, nullptr, nullptr, &zErrMsg);

    if (rc != SQLITE_OK) {
        std::cout << "Sqlite error: " << zErrMsg;
        sqlite3_free(zErrMsg);
    } else {
        std::cout << "Table created successfully!\n";
    }
}

void VoiceOpsWindow::insert_server_to_database(const std::string& pName, const std::string& pURL, const std::string& pPort) {
    if (pURL.empty()) {
        std::cout << "Empty server URL provided. Cannot add to database.\n";
        return;
    }

    char* zErrMsg = nullptr;
    std::string sql = "INSERT INTO SERVER_LIST (SERVER_NAME, SERVER_URL, SERVER_PORT) VALUES ('" + pName + "', '" + pURL + "', '" + pPort + "');";
    int rc = sqlite3_exec(mDBHandle, sql.c_str(), nullptr, nullptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cout << "SQLite error: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
    } else {
        std::cout << "Inserted values successfully\n";
    }
}

VoiceOpsWindow::~VoiceOpsWindow() {
    if (mDBHandle) {
        sqlite3_close(mDBHandle);
        std::cout << "Database connection closed\n";
    }
}

void VoiceOpsWindow::server_list_panel() {
    database_functions::retrieve_servers(mDBHandle, mServers);

    mServerListBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    mServerListBox->set_margin_start(3);
    auto topBarVBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    auto serverListVBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    serverListVBox->set_vexpand(true);
    serverListVBox->set_name("server-list-box");

    Gtk::Label* label = nullptr;

    auto topBar = top_bar();
    topBarVBox->append(*topBar);
    mServerListBox->append(*topBarVBox);

    if (mServers.empty()) {
        label = Gtk::make_managed<Gtk::Label>("You have not joined a server yet.");
        serverListVBox->append(*label);
        serverListVBox->set_valign(Gtk::Align::CENTER);
        mServerListBox->append(*serverListVBox);

        return;
    }

    for (ServerInfo& server : mServers) {
        auto serverButton = Gtk::make_managed<Gtk::Button>(server.name);
        ServerCard serverCard;
        serverCard.button = serverButton;
        serverCard.info = server;
        mServerCards.push_back(serverCard);
    }

    for (ServerCard& card : mServerCards) {
        card.button->signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &VoiceOpsWindow::on_server_button_clicked),
        card));
        serverListVBox->append(*card.button);
    }

    mServerListBox->append(*serverListVBox);
}

void VoiceOpsWindow::on_server_button_clicked(ServerCard& pServer) {
    SOCKET newTCPSocket, newUDPSocket;
    if (!createSocket(pServer.info, &newTCPSocket, &newUDPSocket)) {
        auto dialog = Gtk::AlertDialog::create("Failed to connect to server.");
        dialog->show(*this);
        ma_device_stop(&device);
        return;
    }

    closesocket(clientTCPSocket);
    closesocket(clientUDPSocket);

    if (listenThreadTCP.joinable()) {
        listenThreadTCP.join();
    }

    if (listenThreadUDP.joinable()) {
        listenThreadUDP.join();
    }

    clientTCPSocket = newTCPSocket;
    clientUDPSocket = newUDPSocket;

    listenThreadTCP = std::thread(ReceiveMessages, clientTCPSocket);
    listenThreadUDP = std::thread(ReceiveVoice, clientUDPSocket);

    mSelectedServer = &pServer;
    std::cout << "URL: " << pServer.info.url << '\n';
    std::cout << "Port: " << pServer.info.port << '\n';
    server_content_panel(true);
}

void VoiceOpsWindow::refresh_server_list(const std::string& pServerName, const std::string& pServerURL, const std::string& pServerPort) {
    bool firstEntry = mServers.empty();
    ServerInfo newServer = { pServerName, pServerURL, pServerPort };
    mServers.push_back(newServer);

    auto serverButton = Gtk::make_managed<Gtk::Button>(pServerName);
    ServerCard newServerCard;
    newServerCard.button = serverButton;
    newServerCard.info = newServer;
    mServerCards.push_back(newServerCard);
    mServerCards.back().button->signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &VoiceOpsWindow::on_server_button_clicked),
        mServerCards.back()));

    if (mServerListBox && mServerListBox->get_last_child()) {
        auto serverListVBox = dynamic_cast<Gtk::Box*>(mServerListBox->get_last_child());
        if (serverListVBox) {
            if (firstEntry) {
                auto child = serverListVBox->get_first_child();
                while (child != nullptr) {
                    serverListVBox->remove(*child);
                    child = serverListVBox->get_first_child();
                }
                serverListVBox->set_valign(Gtk::Align::START);
            }

            serverListVBox->append(*newServerCard.button);
        }
    }

    // mSelectedServer = &mServerCards.back();
    // server_content_panel(true);
}

void VoiceOpsWindow::server_content_panel(bool pSelectedServer) {
    if (mServerContentBox == nullptr) {
        mServerContentBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
        auto innerWrapper = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);
        innerWrapper->set_expand(true);
        mServerContentBox->append(*innerWrapper);
        mServerContentBox->set_margin_start(10);
    } else {
        auto child = mServerContentBox->get_last_child();
        if (child != mServerContentBox->get_first_child()) {
            mServerContentBox->remove(*child);
        }
    }

    auto innerWrap = dynamic_cast<Gtk::Box*>(mServerContentBox->get_first_child());

    Gtk::Label* emptyLabel = nullptr;

    if (!pSelectedServer) {
        emptyLabel = Gtk::make_managed<Gtk::Label>("Select a server from the left to view its contents.");
        innerWrap->append(*emptyLabel);
        innerWrap->set_valign(Gtk::Align::CENTER);
        innerWrap->set_halign(Gtk::Align::CENTER);

        return;
    }

    auto child = innerWrap->get_first_child();
    while (child != nullptr) {
        innerWrap->remove(*child);
        child = innerWrap->get_first_child();
    }

    Gtk::Label* serverName = Gtk::make_managed<Gtk::Label>(mSelectedServer->info.name);
    Gtk::Label* serverURL = Gtk::make_managed<Gtk::Label>(mSelectedServer->info.url);
    Gtk::Label* serverPort = Gtk::make_managed<Gtk::Label>(mSelectedServer->info.port);

    innerWrap->append(*serverName);
    innerWrap->append(*serverURL);
    innerWrap->append(*serverPort);

    chatHistory->set_text("CHAT:\n");

    auto scrolled_window = Gtk::make_managed<Gtk::ScrolledWindow>();
    auto chat_view = Gtk::make_managed<Gtk::TextView>();
    chat_view->set_buffer(chatHistory);
    chat_view->set_editable(false);
    chat_view->set_cursor_visible(false);
    scrolled_window->set_child(*chat_view);
    scrolled_window->set_min_content_height(500);
    mServerContentBox->append(*scrolled_window);

    chatInput = Gtk::make_managed<Gtk::Entry>();
    chatInput->signal_activate().connect(sigc::mem_fun(*this, &VoiceOpsWindow::on_send_button_clicked));
    innerWrap->append(*chatInput);

    auto send_button = Gtk::make_managed<Gtk::Button>("Send ->");
    send_button->signal_clicked().connect(sigc::mem_fun(*this, &VoiceOpsWindow::on_send_button_clicked));
    send_button->set_margin(5);
    innerWrap->append(*send_button);
}

Gtk::Box* VoiceOpsWindow::top_bar() {
    auto hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);
    auto add_button = Gtk::make_managed<Gtk::Button>("+");
    add_button->signal_clicked().connect(sigc::mem_fun(*this, &VoiceOpsWindow::on_add_button_clicked));

    add_button->set_margin(5);

    hbox->append(*add_button);

    hbox->set_halign(Gtk::Align::END);
    hbox->set_name("top-bar");

    return hbox;
}

void VoiceOpsWindow::on_send_button_clicked() {
    auto msg = chatInput->get_text();
    std::cout << "Send button clicked. Message: " << msg.c_str() << "\n";

    Packet packet;
    packet.packetType = PACKET_TYPE_STRING;
    packet.length = msg.bytes();
    memcpy(packet.data, msg.c_str(), msg.bytes());

    // Todo: @aveens13 | @kripesh101: sizeof(Packet) is overkill.
    int byteCount = send(clientTCPSocket, reinterpret_cast<const char*>(&packet), sizeof(Packet), 0);
    if (byteCount == SOCKET_ERROR) {
        std::cout << "Error in sending data to server: " << WSAGetLastError() << std::endl;
        return;
    }

    chatHistory->place_cursor(chatHistory->end());
    chatHistory->insert_at_cursor("You: ");
    chatHistory->insert_at_cursor(msg);
    chatHistory->insert_at_cursor("\n");

    chatInput->set_text("");
    chatInput->grab_focus();
}

void VoiceOpsWindow::on_add_button_clicked() {
    std::cout << "Add server button was clicked\n";

    auto dialog = Gtk::make_managed<AddServerDialog>(this);
    dialog->set_name("add-dialog");
    dialog->signal_response().connect([this, dialog](int response_id) { on_add_server_response(*dialog, response_id); });
    dialog->show();
}

void VoiceOpsWindow::on_add_server_response(AddServerDialog& pDialog, int pResponseID) {
    switch (pResponseID) {
        case Gtk::ResponseType::OK:
            std::cout << pDialog.get_server_name() << '\n';
            std::cout << pDialog.get_server_url() << '\n';
            std::cout << pDialog.get_server_port() << '\n';
            std::cout << std::flush;
            insert_server_to_database(pDialog.get_server_name(), pDialog.get_server_url(), pDialog.get_server_port());
            refresh_server_list(pDialog.get_server_name(), pDialog.get_server_url(), pDialog.get_server_port());
            pDialog.hide();
            std::cout << "OK was clicked\n";
            break;
        case Gtk::ResponseType::CLOSE:
            std::cout << "The dialog was closed\n";
            break;
        case Gtk::ResponseType::DELETE_EVENT:
            std::cout << "The dialog was deleted\n";
            break;
        default:
            std::cout << "Unexpected response: " << pResponseID << '\n';
            break;
    }
}
