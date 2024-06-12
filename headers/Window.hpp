#pragma once

#include "lodepng.h"

#include <gtkmm.h>
#include <sqlite3.h>
#include <thread>

#define NOGDI
#include <winsock2.h>
#include <windows.h>

#include "Database.hpp"
#include "Dialogs.hpp"
#include "Utils.hpp"

class VoiceOpsWindow : public Gtk::Window {
    public:
        VoiceOpsWindow();

        ~VoiceOpsWindow();

        // friend void ReceiveMessages(SOCKET& clientSocket, GMutex& mutex, DataStore& data, Gtk::Box* chatBox);
        void add_new_message(std::string pUsername = "", const char* pMessage = "");
        void add_new_message(std::string pUsername = "", Glib::RefPtr<Gdk::Pixbuf> pImage = nullptr);
        ServerCard* get_selected_server();
        SOCKET get_tcp_socket();

    protected:
        void on_add_button_clicked();
        void on_add_server_response(AddServerDialog& pDialog, int pResponseID);
        void on_server_button_clicked(ServerCard& pServer);
        void on_send_button_clicked();
        void on_photo_button_clicked();
        void on_photo_response(const Gtk::FileChooserNative& pFileChooser, int pResponseID);
        bool on_hotkey_press(GdkKeyEvent* pKeyEvent);
        void on_voice_join();

        void scroll_to_latest_message();
        void server_list_panel();
        void server_content_panel(bool);
        Gtk::Box* top_bar();

        Gtk::Button mButton;

        sqlite3 *mDBHandle = nullptr;
        Gtk::Box* mServerListBox;
        Gtk::Box* mServerContentBox = nullptr;
        Gtk::Entry* mMessageEntry;
        Gtk::Box* mChatList;
        Gtk::Button* mVoiceCallButton;
        ServerCard* mSelectedServer;
        std::vector<ServerCard> mServerCards;
        std::vector<ServerInfo> mServers;

        Glib::RefPtr<Gdk::Pixbuf> mScreenshotPixbuf;

        std::thread mListenThread;
        
        SOCKET mClientUDPSocket;
        SOCKET mClientTCPSocket;

        std::thread mListenThreadTCP;

        bool voiceConnected = false;
    private:

        std::thread mHKThread;

        void setup_database();
        void refresh_server_list(const std::string& pServerName, const std::string& pUsername, const std::string& pServerURL, const std::string& pServerPort);

        void insert_server_to_database(const std::string& pName, const std::string& pUsername, const std::string& pURL, const std::string& pPort);
};