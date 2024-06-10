#include <chrono>
#include <iostream>

#include "Dialogs.hpp"
#include "Window.hpp"
#include <ws2tcpip.h>

#include "Networking.hpp"

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_GENERATION
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_WASAPI
#define MA_NO_DECODING
#define MA_NO_ENCODING
// #define MA_NO_NODE_GRAPH
#include <thirdparty/miniaudio.h>

#include <common/text_packet.h>

static DataStore data;
static GMutex mutex;

static SOCKET clientUDPSocket;

sockaddr_in server;

static gboolean testhotkey(void*) {
    auto dialog = Gtk::AlertDialog::create("Failed to connect to server.");
    dialog->show();

    return G_SOURCE_REMOVE;
}

void handleHotkeys() {
    static bool registered = false;
    if (!registered) {
        if (!RegisterHotKey(NULL, 1, MOD_CONTROL, VK_SNAPSHOT)) {
            std::cout << "Error!\n";
        }

        registered = true;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) != 0) {
        if (msg.message == WM_HOTKEY) {
            std::cout << "Hotkey received\n";
            g_idle_add(testhotkey, NULL);
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

    int iResult = send(clientUDPSocket, (const char*)&outgoing_pkt, len + 4, 0);
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

    // ma_device_start(&device);

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

    if (mHKThread.joinable()) {
        mHKThread.join();
    }

    mHKThread = std::thread(&handleHotkeys);

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
        "SERVER_USERNAME    TEXT,"\
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

void VoiceOpsWindow::insert_server_to_database(const std::string& pName, const std::string& pUsername, const std::string& pURL, const std::string& pPort) {
    if (pURL.empty()) {
        std::cout << "Empty server URL provided. Cannot add to database.\n";
        return;
    }

    char* zErrMsg = nullptr;
    std::string sql = "INSERT INTO SERVER_LIST (SERVER_NAME, SERVER_USERNAME, SERVER_URL, SERVER_PORT) VALUES ('" + pName + "', '" + pUsername + "', '" + pURL + "', '" + pPort + "');";
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
        label = Gtk::make_managed<Gtk::Label>("You have not joined a server yet.\nClick the + button above to join one.");
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
        serverCard.previousSender = "";
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

    closesocket(mClientTCPSocket);
    closesocket(clientUDPSocket);

    if (mListenThreadTCP.joinable()) {
        mListenThreadTCP.join();
    }

    if (mListenThreadUDP.joinable()) {
        mListenThreadUDP.join();
    }

    mClientTCPSocket = newTCPSocket;
    clientUDPSocket = newUDPSocket;

    mListenThreadTCP = std::thread([this]() {
        ReceiveMessages(mClientTCPSocket, std::ref(mutex), std::ref(data), *this);
    });
    mListenThreadUDP = std::thread(ReceiveVoice, clientUDPSocket);

    mSelectedServer = &pServer;
    std::cout << "URL: " << pServer.info.url << '\n';
    std::cout << "Port: " << pServer.info.port << '\n';
    server_content_panel(true);
}

void VoiceOpsWindow::refresh_server_list(const std::string& pServerName, const std::string& pUsername, const std::string& pServerURL, const std::string& pServerPort) {
    bool firstEntry = mServers.empty();
    ServerInfo newServer = {pServerName, pUsername, pServerURL, pServerPort};
    mServers.push_back(newServer);

    auto serverButton = Gtk::make_managed<Gtk::Button>(pServerName);
    ServerCard newServerCard;
    newServerCard.button = serverButton;
    newServerCard.info = newServer;
    newServerCard.previousSender = "";
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
        mServerContentBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);
        auto innerWrapper = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
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
    innerWrap->set_valign(Gtk::Align::FILL);
    innerWrap->set_halign(Gtk::Align::FILL);

    // Define scrollWindow -> mChatList -> messageBox -> Name -> Message
    auto scrollWindow = Gtk::make_managed<Gtk::ScrolledWindow>();
    mChatList = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    auto messageBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);

    auto nameLabel = Gtk::make_managed<Gtk::Label>("Test Name");
    nameLabel->set_selectable(true);
    nameLabel->set_name("chat-name-label");
    nameLabel->set_hexpand(true);
    nameLabel->set_halign(Gtk::Align::START);
    auto messageLabel = Gtk::make_managed<Gtk::Label>("Test message: This is the message label");\
    messageLabel->set_selectable(true);
    messageLabel->set_name("chat-msg-label");
    messageLabel->set_hexpand(true);
    messageLabel->set_halign(Gtk::Align::START);
    
    messageBox->append(*nameLabel);
    messageBox->append(*messageLabel);
    
    mChatList->append(*messageBox);
    mChatList->set_expand(true);
    
    scrollWindow->set_child(*mChatList);


    // Gtk::Label* serverName = Gtk::make_managed<Gtk::Label>(mSelectedServer->info.name);
    // Gtk::Label* serverURL = Gtk::make_managed<Gtk::Label>(mSelectedServer->info.url);
    // Gtk::Label* serverPort = Gtk::make_managed<Gtk::Label>(mSelectedServer->info.port);

    // innerWrap->append(*serverName);
    // innerWrap->append(*serverURL);
    // innerWrap->append(*serverPort);

    auto textBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);

    auto textMessagePortion = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);

    auto voiceCallButton = Gtk::make_managed<Gtk::Button>();
    auto pixbuf = Gdk::Pixbuf::create_from_file("./css/mic.png");
    auto voiceImage = Gtk::make_managed<Gtk::Image>(pixbuf);
    voiceCallButton->set_child(*voiceImage);
    voiceCallButton->signal_clicked().connect(sigc::mem_fun(*this, &VoiceOpsWindow::on_voice_join));
    
    mMessageEntry = Gtk::make_managed<Gtk::Entry>();
    mMessageEntry->signal_activate().connect(sigc::mem_fun(*this, &VoiceOpsWindow::on_send_button_clicked));
    
    auto choosePhotoButton = Gtk::make_managed<Gtk::Button>();
    auto image = Gtk::make_managed<Gtk::Image>("document-open");
    choosePhotoButton->set_child(*image);
    choosePhotoButton->signal_clicked().connect(sigc::mem_fun(*this, &VoiceOpsWindow::on_photo_button_clicked));
    
    auto sendButton = Gtk::make_managed<Gtk::Button>("Send");
    sendButton->signal_clicked().connect(sigc::mem_fun(*this, &VoiceOpsWindow::on_send_button_clicked));

    mMessageEntry->set_hexpand(true);
    textBox->set_hexpand(true);

    auto fileNameLabel = Gtk::make_managed<Gtk::Label>();

    textBox->append(*fileNameLabel);
    
    textMessagePortion->append(*voiceCallButton);
    textMessagePortion->append(*mMessageEntry);
    textMessagePortion->append(*choosePhotoButton);
    textMessagePortion->append(*sendButton);

    textBox->append(*textMessagePortion);

    textBox->set_name("message-text-btn-box");

    innerWrap->append(*scrollWindow);
    innerWrap->append(*textBox);
}

void VoiceOpsWindow::on_voice_join() {
    if (voiceConnected) {
        ma_device_stop(&device);
        voiceConnected = false;
    } else {
        ma_device_start(&device);
        voiceConnected = true;
    }
}

void VoiceOpsWindow::on_photo_button_clicked() {
    auto fileChooser = Gtk::FileChooserNative::create("Select an image", *this, Gtk::FileChooser::Action::OPEN);

    auto imageFilter = Gtk::FileFilter::create();
    imageFilter->set_name("Image Files");
    imageFilter->add_pattern("*.png");
    imageFilter->add_pattern("*.jpg");
    imageFilter->add_pattern("*.jpeg");

    fileChooser->add_filter(imageFilter);


    fileChooser->signal_response().connect([this, fileChooser](int response_id) {on_photo_response(*fileChooser, response_id);});
    fileChooser->show();
}

void VoiceOpsWindow::on_photo_response(const Gtk::FileChooserNative& pFileChooser, int pResponseID) {
    if (pResponseID == Gtk::ResponseType::ACCEPT) {
        std::string filePath = pFileChooser.get_file()->get_path();
        std::cout << filePath << '\n';

        auto innerWrap = mServerContentBox->get_first_child();
        auto textBox = innerWrap->get_last_child();

        if (textBox) {
            auto label = dynamic_cast<Gtk::Label*>(textBox->get_first_child());
            if (label) label->set_text("Selected file: " + filePath.substr(filePath.find_last_of('\\') + 1, filePath.length()));
            else std::cerr << "Error accessing label for the file name.\n";
        } else {
            std::cerr << "Error accessing parent of the label for the file name.\n";
        }
    }
}

void VoiceOpsWindow::scroll_to_latest_message() {
    Gtk::Widget* widget = mChatList;
    while (widget) {
        if (auto parent = dynamic_cast<Gtk::ScrolledWindow*>(widget->get_parent())) {
            auto adjustment = parent->get_vadjustment();
            adjustment->set_value(adjustment->get_upper() - adjustment->get_page_size());
            parent->set_vadjustment(adjustment);
            break;
        }
        widget = widget->get_parent();
    }
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
    auto message = mMessageEntry->get_text();

    if (message.empty() || message.find_first_not_of(' ') == std::string::npos) return;

    std::cout << "Send button clicked. Message: " << message.c_str() << "\n";

    Packet packet;
    packet.packetType = PACKET_TYPE_STRING;
    packet.length = message.bytes();
    memcpy(packet.data, message.c_str(), message.bytes());

    // Todo: @aveens13 | @kripesh101: sizeof(Packet) is overkill.
    int byteCount = send(mClientTCPSocket, reinterpret_cast<const char*>(&packet), sizeof(Packet), 0);
    if (byteCount == SOCKET_ERROR) {
        std::cout << "Error in sending data to server: " << WSAGetLastError() << std::endl;
        return;
    }

    add_new_message(message.c_str(), mSelectedServer->info.username);

    scroll_to_latest_message();

    send(mClientTCPSocket, message.c_str(), message.bytes(), 0);

    mMessageEntry->set_text("");
    mMessageEntry->grab_focus();
}

void VoiceOpsWindow::add_new_message(const char* pMessage, std::string pUsername) {
    auto messageBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    auto nameAndTimeLabel = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);

    auto now = std::chrono::system_clock::now();
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_time_t);
    
    std::string timeString = "[" + std::to_string(now_tm->tm_hour) + ":" +
                                     std::to_string(now_tm->tm_min) +
                                     "]";
    
    std::string nameString = pUsername;
    static std::string senderName = "";
    
    auto timeLabel = Gtk::make_managed<Gtk::Label>(timeString.c_str());
    timeLabel->set_selectable(true);
    timeLabel->set_name("chat-time-label");
    timeLabel->set_halign(Gtk::Align::START);

    auto nameLabel = Gtk::make_managed<Gtk::Label>(nameString.c_str());
    nameLabel->set_selectable(true);
    (pUsername == mSelectedServer->info.username) ? nameLabel->set_name("chat-selfname-label") : nameLabel->set_name("chat-friendname-label");
    nameLabel->set_hexpand(true);
    nameLabel->set_halign(Gtk::Align::START);

    nameAndTimeLabel->append(*timeLabel);
    nameAndTimeLabel->append(*nameLabel);

    auto messageLabel = Gtk::make_managed<Gtk::Label>(pMessage);
    messageLabel->set_selectable(true);
    messageLabel->set_name("chat-msg-label");
    messageLabel->set_hexpand(true);
    messageLabel->set_halign(Gtk::Align::START);
    
    if (mSelectedServer->previousSender != pUsername) messageBox->append(*nameAndTimeLabel);
    messageBox->append(*messageLabel);

    mChatList->append(*messageBox);

    mSelectedServer->previousSender = pUsername;
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
            std::cout << pDialog.get_server_username() << '\n';
            std::cout << pDialog.get_server_url() << '\n';
            std::cout << pDialog.get_server_port() << '\n';
            std::cout << std::flush;
            insert_server_to_database(pDialog.get_server_name(), pDialog.get_server_username(), pDialog.get_server_url(), pDialog.get_server_port());
            refresh_server_list(pDialog.get_server_name(), pDialog.get_server_username(), pDialog.get_server_url(), pDialog.get_server_port());
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
