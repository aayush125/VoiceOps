#include <chrono>
#include <iostream>
#include <fstream>

#include "Dialogs.hpp"
#include "Window.hpp"
#include <ws2tcpip.h>

#include "Networking.hpp"
#include "Voice.hpp"
#include "Screenshot.hpp"

#include <common/text_packet.h>

static DataStore data;
static GMutex mutex;

std::vector<unsigned char> loadPicture(const std::string& fileName) {
    std::ifstream file(fileName, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << fileName << std::endl;
        return {};
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<unsigned char> pictureData(fileSize);
    file.read(reinterpret_cast<char*>(pictureData.data()), fileSize);

    file.close();

    return pictureData;
}

void sendPicture(const std::string& filename, SOCKET socket) {
    std::vector<unsigned char> pictureData = loadPicture(filename);
    std::cout << pictureData.size() << std::endl;
    if (pictureData.empty()) {
        // Handle error
        return;
    }

    const size_t dataSize = pictureData.size();
    const size_t numPackets = (dataSize + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;

    int counter = 0;
    for (size_t i = 0; i < numPackets; ++i) {
        Packet packet;
        packet.packetType = PACKET_TYPE_IMAGE;
        packet.length = static_cast<uint32_t>(min(dataSize - i * MAX_PACKET_SIZE, MAX_PACKET_SIZE));
        memcpy(packet.data.image, pictureData.data() + i * MAX_PACKET_SIZE, packet.length);

        // Send the packet over the network
        send(socket, reinterpret_cast<const char*>(&packet), sizeof(Packet), 0);

        counter++;
    }

    std::cout << "Counter: " << counter << std::endl;
}

Glib::RefPtr<Gdk::Pixbuf> create_pixbuf_from_screenshot(VoiceOpsWindow* windowref) {
    std::vector<BYTE> pixels;
    int width, height, stride;
    std::tie(pixels, width, height, stride) = get_screenshot();

    for (size_t i = 0; i < pixels.size(); i += 4) {
        std::swap(pixels[i], pixels[i + 2]);  // Swap B and R
    }

    std::vector<unsigned char> png;
    unsigned error = lodepng::encode(png, pixels, width, height);
    if (error) {
        std::cout << "PNG encoder error " << error << ": " << lodepng_error_text(error) << std::endl;
        return Glib::RefPtr<Gdk::Pixbuf>();
    }

    // Create a Glib::MemoryInputStream from the PNG data
    Glib::RefPtr<Gio::MemoryInputStream> stream = Gio::MemoryInputStream::create();
    stream->add_data(&png[0], png.size(), nullptr);

    // Glib::RefPtr<Glib::Bytes> bytes = Glib::Bytes::create(png.data(), png.size());

    // Create a new Gdk::Pixbuf from the PNG data in the stream
    Glib::RefPtr<Gdk::Pixbuf> pixbuf = Gdk::Pixbuf::create_from_stream(stream);

    if (!pixbuf) {
        std::cout << "Failed to create Gdk::Pixbuf from PNG data" << std::endl;
        return Glib::RefPtr<Gdk::Pixbuf>();
    }

    // sending to server
    const size_t dataSize = png.size();
    const size_t numPackets = (dataSize + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;

    for (size_t i = 0; i < numPackets; ++i) {
        Packet packet;
        packet.packetType = PACKET_TYPE_IMAGE;
        packet.length = static_cast<uint32_t>(min(dataSize - i * MAX_PACKET_SIZE, MAX_PACKET_SIZE));
        memcpy(packet.data.image, png.data() + i * MAX_PACKET_SIZE, packet.length);

        // Send the packet over the network
        send(windowref->get_tcp_socket(), reinterpret_cast<const char*>(&packet), sizeof(Packet), 0);
    }

    return pixbuf;
}

static gboolean take_screenshot(gpointer window) {
    auto windowref = static_cast<VoiceOpsWindow*>(window);

    auto image = create_pixbuf_from_screenshot(windowref);

    if (image) {
        std::cout << "screenshot gotten(?)\n";
    } else {
        std::cerr << "Failed to get screenshot image\n";
    }

    windowref->add_new_message(windowref->get_selected_server()->info.username, image);

    return G_SOURCE_REMOVE;
}

ServerCard* VoiceOpsWindow::get_selected_server() {
    return mSelectedServer;
}

SOCKET VoiceOpsWindow::get_tcp_socket() {
    return mClientTCPSocket;
}

void handleHotkeys(VoiceOpsWindow& windowref) {
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
            g_idle_add(take_screenshot, &windowref);
        }
    }
}

VoiceOpsWindow::VoiceOpsWindow() {
    set_title("VoiceOps");
    set_name("main-window");

    if (mHKThread.joinable()) {
        mHKThread.join();
    }

    mHKThread = std::thread([this]() {
        handleHotkeys(*this);
    });

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

    Voice::init();
}

void VoiceOpsWindow::setup_database() {
    char* zErrMsg = 0;
    int rc;

    const char* sql = "CREATE TABLE IF NOT EXISTS SERVER_LIST(" \
        "SERVER_NAME    TEXT    NOT NULL," \
        "SERVER_PASSWORD    TEXT,"\
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

void VoiceOpsWindow::insert_server_to_database(const std::string& pName, const std::string& pPassword, const std::string& pUsername, const std::string& pURL, const std::string& pPort) {
    if (pURL.empty()) {
        std::cout << "Empty server URL provided. Cannot add to database.\n";
        return;
    }

    char* zErrMsg = nullptr;
    std::string sql = "INSERT INTO SERVER_LIST (SERVER_NAME, SERVER_PASSWORD, SERVER_USERNAME, SERVER_URL, SERVER_PORT) VALUES ('" + pName + "', '" + pPassword + "', '" + pUsername + "', '" + pURL + "', '" + pPort + "');";
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

void VoiceOpsWindow::server_list_panel(bool pRefreshing) {
    database_functions::retrieve_servers(mDBHandle, mServers);

    if (pRefreshing) {
        Gtk::Widget* widget = mServerListBox->get_first_child();
        while (widget) {
            mServerListBox->remove(*widget);
            widget = mServerListBox->get_first_child();
        }
        mServers.clear();
        mServerCards.clear();
    }

    if (!mServerListBox) {
        mServerListBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
        mServerListBox->set_margin_start(3);
    }
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
        auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);
        box->set_name("server-holding-box");
        auto removeBtn = Gtk::make_managed<Gtk::Button>("x");
        removeBtn->set_name("server-remove-button");
        removeBtn->signal_clicked().connect([this, card, box]() {
            auto dialog = Gtk::make_managed<OptionsDialog>(this, "Are you sure you want to remove this server?");
            dialog->signal_response().connect([this, dialog, box, card](int response_id) { 
                auto vBox = dynamic_cast<Gtk::Box*>(mServerListBox->get_last_child());
                switch (response_id) {
                    case Gtk::ResponseType::YES:
                        database_functions::remove_server(mDBHandle, card.info.name);
                        vBox->remove(*box);
                        if (!vBox->get_last_child()) {
                            server_list_panel(true);
                        }
                        dialog->hide();
                    case Gtk::ResponseType::CANCEL:
                        dialog->hide();
                        return;
                    default:
                        dialog->hide();
                        std::cerr << "Error occurred in remove button callback\n";
                }
             });
             dialog->show();
        });
        box->append(*removeBtn);
        box->append(*card.button);
        serverListVBox->append(*box);
    }

    mServerListBox->append(*serverListVBox);
}

void VoiceOpsWindow::on_server_button_clicked(ServerCard& pServer) {
    if (mSelectedServer && (pServer.info.name == mSelectedServer->info.name)) {
        return;
    } else if (mSelectedServer && (pServer.info.name != mSelectedServer->info.name)) {
        reset_content_panel();
        Voice::forceStop();
    }

    std::cout << "getting here\n";
    std::cout << "Password for server " << pServer.info.name << ": " << pServer.info.pass << '\n';

    SOCKET newTCPSocket, newUDPSocket;
    if (!createSocket(pServer.info, &newTCPSocket, &newUDPSocket)) {
        auto dialog = Gtk::AlertDialog::create("Failed to connect to server.");
        dialog->show(*this);
        Voice::forceStop();
        return;
    }

    closesocket(mClientTCPSocket);
    closesocket(mClientUDPSocket);

    if (mListenThreadTCP.joinable()) {
        mListenThreadTCP.join();
    }

    Voice::joinThread();

    mClientTCPSocket = newTCPSocket;
    mClientUDPSocket = newUDPSocket;

    mListenThreadTCP = std::thread([this]() {
        ReceiveMessages(mClientTCPSocket, std::ref(mutex), std::ref(data), *this);
    });
    Voice::newThread(mClientUDPSocket);

    std::cout << "Checkpoint 1\n";

    if (mSelectedServer != nullptr) mSelectedServer->button->get_parent()->set_name("wrong-name");
    mSelectedServer = &pServer;
    mSelectedServer->button->get_parent()->set_name("currently-selected-server");
    std::cout << "URL: " << pServer.info.url << '\n';
    std::cout << "Port: " << pServer.info.port << '\n';
    std::cout << "Checkpoint 2\n";
    server_content_panel(true);
    std::cout << "Checkpoint 3\n";
}

void VoiceOpsWindow::refresh_server_list(const std::string& pServerName, const std::string& pPassword, const std::string& pUsername, const std::string& pServerURL, const std::string& pServerPort) {
    bool firstEntry = mServers.empty();
    ServerInfo newServer = {pServerName, pPassword, pUsername, pServerURL, pServerPort};
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
            auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);
            box->set_name("server-holding-box");
            auto removeBtn = Gtk::make_managed<Gtk::Button>("x");
            removeBtn->set_name("server-remove-button");
            removeBtn->signal_clicked().connect([this, newServerCard, box]() {
            auto dialog = Gtk::make_managed<OptionsDialog>(this, "Are you sure you want to remove this server?");
            dialog->signal_response().connect([this, dialog, box, newServerCard](int response_id) { 
                auto vBox = dynamic_cast<Gtk::Box*>(mServerListBox->get_last_child());
                switch (response_id) {
                    case Gtk::ResponseType::YES:
                        database_functions::remove_server(mDBHandle, newServerCard.info.name);
                        vBox->remove(*box);
                        if (!vBox->get_last_child()) {
                            server_list_panel(true);
                        }
                        dialog->hide();
                    case Gtk::ResponseType::CANCEL:
                        dialog->hide();
                        return;
                    default:
                        dialog->hide();
                        std::cerr << "Error occurred in remove button callback\n";
                }
             });
             dialog->show();
        });
            box->append(*removeBtn);
            box->append(*newServerCard.button);
            serverListVBox->append(*box);

            // serverListVBox->append(*newServerCard.button);
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

    auto nameLabel = Gtk::make_managed<Gtk::Label>("Your messages start here.");
    nameLabel->set_selectable(true);
    nameLabel->set_name("server-start-message");
    nameLabel->set_hexpand(true);
    nameLabel->set_margin_bottom(10);
    nameLabel->set_halign(Gtk::Align::START);
    
    messageBox->append(*nameLabel);
    
    mChatList->append(*messageBox);
    mChatList->set_expand(true);
    
    scrollWindow->set_child(*mChatList);

    auto textBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);

    auto textMessagePortion = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);

    mVoiceCallButton = Gtk::make_managed<Gtk::Button>();
    auto pixbuf = Gdk::Pixbuf::create_from_file("./css/mic_disconnected.png");
    auto voiceImage = Gtk::make_managed<Gtk::Image>(pixbuf);
    mVoiceCallButton->set_child(*voiceImage);
    mVoiceCallButton->signal_clicked().connect(sigc::mem_fun(*this, &VoiceOpsWindow::on_voice_join));
    
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

    auto fileNameBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);
    auto fileNameLabel = Gtk::make_managed<Gtk::Label>();
    auto removeBtn = Gtk::make_managed<Gtk::Button>("x");
    removeBtn->signal_clicked().connect([this, removeBtn, fileNameLabel]() {
        this->mSelectedServer->selectedFilePath = "";
        fileNameLabel->set_text("");
        removeBtn->set_visible(false);
    });
    removeBtn->set_visible(false);
    fileNameBox->append(*fileNameLabel);
    fileNameBox->append(*removeBtn);

    fileNameBox->set_halign(Gtk::Align::CENTER);

    textBox->append(*fileNameBox);
    
    textMessagePortion->append(*mVoiceCallButton);
    textMessagePortion->append(*mMessageEntry);
    textMessagePortion->append(*choosePhotoButton);
    textMessagePortion->append(*sendButton);

    textBox->append(*textMessagePortion);

    textBox->set_name("message-text-btn-box");

    innerWrap->append(*scrollWindow);
    innerWrap->append(*textBox);
}

void VoiceOpsWindow::on_voice_join() {
    Voice::toggle();
    if (Voice::getVoiceStatus()) {
        auto pixbuf = Gdk::Pixbuf::create_from_file("./css/mic.png");
        auto voiceImage = Gtk::make_managed<Gtk::Image>(pixbuf);
        mVoiceCallButton->set_child(*voiceImage);
    } else {
        auto pixbuf = Gdk::Pixbuf::create_from_file("./css/mic_disconnected.png");
        auto voiceImage = Gtk::make_managed<Gtk::Image>(pixbuf);
        mVoiceCallButton->set_child(*voiceImage);
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

        mSelectedServer->selectedFilePath = filePath;

        if (textBox) {
            auto fileNameBox = dynamic_cast<Gtk::Box*>(textBox->get_first_child());
            auto btn = dynamic_cast<Gtk::Button*>(fileNameBox->get_last_child());
            if (btn) btn->set_visible(true);
            auto label = dynamic_cast<Gtk::Label*>(fileNameBox->get_first_child());
            if (label) label->set_text("Selected file: " + filePath.substr(filePath.find_last_of('\\') + 1, filePath.length()));
            else std::cerr << "Error accessing label for the file name.\n";
            Glib::signal_timeout().connect_once(sigc::mem_fun(*this, &VoiceOpsWindow::scroll_to_latest_message), 50);

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
            adjustment->set_value(adjustment->get_upper());
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

    if (!message.empty() && !(message.find_first_not_of(' ') == std::string::npos)) {
        std::cout << "Send button clicked. Message: " << message.c_str() << "\n";

        Packet packet;
        packet.packetType = PACKET_TYPE_MSG_TO_SERVER;
        packet.length = message.bytes();
        memcpy(packet.data.message_to_server, message.c_str(), message.bytes());

        int byteCount = send(mClientTCPSocket, reinterpret_cast<const char*>(&packet), sizeof(Packet), 0);
        if (byteCount == SOCKET_ERROR) {
            std::cout << "Error in sending data to server: " << WSAGetLastError() << std::endl;
            return;
        }

        add_new_message(mSelectedServer->info.username, message.c_str());
    }

    if (mSelectedServer->selectedFilePath != "") {
        sendPicture(mSelectedServer->selectedFilePath, mClientTCPSocket);
        auto pixbuf = Gdk::Pixbuf::create_from_file(mSelectedServer->selectedFilePath);
        add_new_message(mSelectedServer->info.username, pixbuf);
        mSelectedServer->selectedFilePath = "";
    }

    mMessageEntry->set_text("");
    mMessageEntry->grab_focus();

    auto innerWrap = mServerContentBox->get_first_child();
    auto textBox = innerWrap->get_last_child();

    if (textBox) {
        auto fileNameBox = dynamic_cast<Gtk::Box*>(textBox->get_first_child());
        auto btn = dynamic_cast<Gtk::Button*>(fileNameBox->get_last_child());
        if (btn) btn->set_visible(false);
        auto label = dynamic_cast<Gtk::Label*>(fileNameBox->get_first_child());
        if (label) label->set_text("");
        else std::cerr << "Error accessing label for the file name.\n";
    } else {
        std::cerr << "Error accessing parent of the label for the file name.\n";
    }
}

void VoiceOpsWindow::add_new_message(std::string pUsername, const char* pMessage) {
    auto messageBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    auto nameAndTimeLabel = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);

    auto now = std::chrono::system_clock::now();
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_time_t);
    
    std::string timeString = "[" + std::to_string(now_tm->tm_hour) + ":" +
                                     std::to_string(now_tm->tm_min) +
                                     "]";
    
    std::string nameString = pUsername;
    
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

    Glib::signal_timeout().connect_once(sigc::mem_fun(*this, &VoiceOpsWindow::scroll_to_latest_message), 50);
}

void VoiceOpsWindow::add_new_message(std::string pUsername, Glib::RefPtr<Gdk::Pixbuf> pImage) {
    if (!pImage) {
        std::cerr << "Error: Invalid image pointer\n";
        return;
    }

    if (!mSelectedServer || !mChatList) {
        std::cerr << "Error: Invalid server or chat list\n";
        return;
    }

    auto messageBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    messageBox->set_halign(Gtk::Align::FILL);
    messageBox->set_valign(Gtk::Align::FILL);
    messageBox->set_expand(true);
    messageBox->set_name("chat-image-box");
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

    // auto labelpixbuf = Gdk::Pixbuf::create_from_file("C:/opengl-coding/image.png");
    std::cout << "Image height: " << pImage->get_height() << '\n';
    std::cout << "Image width: " << pImage->get_width() << '\n';
    // auto labelpixbuf = pImage->scale_simple(5000, 5000, Gdk::InterpType::BILINEAR);
    auto pictureButton = Gtk::make_managed<Gtk::Button>();
    auto messageLabel = Gtk::make_managed<Gtk::Picture>(pImage);
    messageLabel->set_name("chat-image-label");
    messageLabel->set_halign(Gtk::Align::START);
    messageLabel->set_valign(Gtk::Align::FILL);
    int width = (pImage->get_width() < 200) ? pImage->get_width() : 200;
    int height = (pImage->get_height() < 200) ? pImage->get_height() : 200; 
    messageLabel->set_size_request(width, height);
    messageLabel->set_can_shrink();
    messageLabel->set_expand(false);
    pictureButton->set_child(*messageLabel);

    pictureButton->signal_clicked().connect([this, pImage]() {
        auto dialog = Gtk::make_managed<PhotoDialog>(this, pImage);
        dialog->show();
    });

    if (mSelectedServer->previousSender != pUsername) messageBox->append(*nameAndTimeLabel);
    messageBox->append(*messageLabel);
    messageBox->append(*pictureButton);

    mChatList->append(*messageBox);

    mSelectedServer->previousSender = pUsername;

    Glib::signal_timeout().connect_once(sigc::mem_fun(*this, &VoiceOpsWindow::scroll_to_latest_message), 50);
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
            std::cout << pDialog.get_server_pass() << '\n';
            std::cout << pDialog.get_server_username() << '\n';
            std::cout << pDialog.get_server_url() << '\n';
            std::cout << pDialog.get_server_port() << '\n';
            std::cout << std::flush;
            insert_server_to_database(pDialog.get_server_name(), pDialog.get_server_pass(), pDialog.get_server_username(), pDialog.get_server_url(), pDialog.get_server_port());
            refresh_server_list(pDialog.get_server_name(), pDialog.get_server_pass(), pDialog.get_server_username(), pDialog.get_server_url(), pDialog.get_server_port());
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

void VoiceOpsWindow::reset_content_panel(bool pDisconnected) {
    Gtk::Box* innerWrap = dynamic_cast<Gtk::Box*>(mServerContentBox->get_first_child());
    Gtk::Widget* widget = innerWrap->get_first_child();
    while (widget) {
        innerWrap->remove(*widget);
        widget = innerWrap->get_first_child();
    }

    mSelectedServer->previousSender = "";
    mSelectedServer->selectedFilePath = "";
    mSelectedServer->button->get_parent()->set_name("wrong-name");
    mSelectedServer = nullptr;

    Gtk::Label* emptyLabel = nullptr;
    emptyLabel = Gtk::make_managed<Gtk::Label>("Select a server from the left to view its contents.");
    innerWrap->append(*emptyLabel);
    innerWrap->set_valign(Gtk::Align::CENTER);
    innerWrap->set_halign(Gtk::Align::CENTER);

    if (pDisconnected) {
        auto dialog = Gtk::AlertDialog::create("You were disconnected from the server.");
        dialog->show(*this);
    }
}

void VoiceOpsWindow::disconnect() {
    Voice::forceStop();
    closesocket(mClientTCPSocket);
    closesocket(mClientUDPSocket);

    Glib::signal_idle().connect_once([this] {
        if (mListenThreadTCP.joinable()) {
            mListenThreadTCP.join();
        }

        Voice::joinThread();
        
        this->reset_content_panel(true);
    });
}
