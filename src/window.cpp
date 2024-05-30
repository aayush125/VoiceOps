#include <chrono>
#include <iostream>

#include "Dialogs.hpp"
#include "Window.hpp"
#include <ws2tcpip.h>

#include "Networking.hpp"

static DataStore data;
static GMutex mutex;

gboolean update_textbuffer(void*) {
    g_mutex_lock(&mutex);
    auto msg = std::string(data.buffer, data.bytes);
    g_mutex_unlock(&mutex);
    std::cout << "Incoming: " << msg.c_str() << '\n';

    return G_SOURCE_REMOVE;
}

VoiceOpsWindow::VoiceOpsWindow() {
    set_title("VoiceOps");
    set_name("main-window");

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

    if (mServerListBox) hbox->append(*mServerListBox);
    else std::cerr << "problem 1\n";
    if (mServerContentBox) hbox->append(*mServerContentBox);
    else std::cerr << "problem 2\n";

    set_child(*hbox);
    set_default_size(900, 900);

    // auto children = hbox->get_last_child();
    // mServerContentBox = dynamic_cast<Gtk::Box*>(children);

    mServerContentBox->set_name("content-box");
    mServerListBox->set_name("list-box");
}

void VoiceOpsWindow::setup_database() {
    char *zErrMsg = 0;
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
    SOCKET newSocket = createSocket(pServer.info);
    if (newSocket == 0) {
        auto dialog = Gtk::AlertDialog::create("Failed to connect to server.");
        dialog->show(*this);

        closesocket(newSocket);
        return;
    }

    closesocket(mClientSocket);
    mClientSocket = newSocket;

    if (mListenThread.joinable()) {
        mListenThread.join();
    }

    mListenThread = std::thread([this]() {
        ReceiveMessages(mClientSocket, std::ref(mutex), std::ref(data), *this);
    });

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
    nameLabel->set_selectable(true);\
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

    auto textBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);
    mMessageEntry = Gtk::make_managed<Gtk::Entry>();
    mMessageEntry->signal_activate().connect(sigc::mem_fun(*this, &VoiceOpsWindow::on_send_button_clicked));
    auto sendButton = Gtk::make_managed<Gtk::Button>("Send");
    sendButton->signal_clicked().connect(sigc::mem_fun(*this, &VoiceOpsWindow::on_send_button_clicked));

    mMessageEntry->set_hexpand(true);
    textBox->set_hexpand(true);

    textBox->append(*mMessageEntry);
    textBox->append(*sendButton);

    innerWrap->append(*scrollWindow);
    innerWrap->append(*textBox);
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

    add_new_message(message.c_str());

    scroll_to_latest_message();

    send(mClientSocket, message.c_str(), message.bytes(), 0);

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
                                     "]"; // + (pSelf ? mSelectedServer->info.username : "Client Name");
    
    std::string nameString = (pUsername == "") ? mSelectedServer->info.username : pUsername;
    static std::string senderName = (pUsername == "") ? mSelectedServer->info.username : "";
    
    auto timeLabel = Gtk::make_managed<Gtk::Label>(timeString.c_str());
    timeLabel->set_selectable(true);
    timeLabel->set_name("chat-time-label");
    timeLabel->set_halign(Gtk::Align::START);

    auto nameLabel = Gtk::make_managed<Gtk::Label>(nameString.c_str());
    nameLabel->set_selectable(true);
    (pUsername == "") ? nameLabel->set_name("chat-selfname-label") : nameLabel->set_name("chat-friendname-label");
    nameLabel->set_hexpand(true);
    nameLabel->set_halign(Gtk::Align::START);

    nameAndTimeLabel->append(*timeLabel);
    nameAndTimeLabel->append(*nameLabel);

    auto messageLabel = Gtk::make_managed<Gtk::Label>(pMessage);
    messageLabel->set_selectable(true);
    messageLabel->set_name("chat-msg-label");
    messageLabel->set_hexpand(true);
    messageLabel->set_halign(Gtk::Align::START);
    
    if (senderName != pUsername) messageBox->append(*nameAndTimeLabel);
    messageBox->append(*messageLabel);

    mChatList->append(*messageBox);

    senderName = pUsername;
}

void VoiceOpsWindow::on_add_button_clicked() {
    std::cout << "Add server button was clicked\n";

    auto dialog = Gtk::make_managed<AddServerDialog>(this);
    dialog->set_name("add-dialog");
    dialog->signal_response().connect([this, dialog](int response_id) {on_add_server_response(*dialog, response_id);});
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