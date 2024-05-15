#pragma once
#include <gtkmm.h>
#include <sqlite3.h>
#include <thread>

#define NOGDI
#include <winsock2.h>

#include "Database.hpp"
#include "Dialogs.hpp"
#include "Utils.hpp"

class VoiceOpsWindow : public Gtk::Window {
    public:
        VoiceOpsWindow();

        ~VoiceOpsWindow();
    
    protected:
        void on_add_button_clicked();
        void on_add_server_response(AddServerDialog& pDialog, int pResponseID);
        void on_server_button_clicked(ServerCard& pServer);
        void on_send_button_clicked();

        void server_list_panel();
        void server_content_panel(bool);
        Gtk::Box* top_bar();

        Gtk::Button mButton;

        sqlite3 *mDBHandle = nullptr;
        Gtk::Box* mServerListBox;
        Gtk::Box* mServerContentBox = nullptr;
        ServerCard* mSelectedServer;
        Glib::RefPtr<Gtk::Menu> mPopupMenu;
        std::vector<ServerCard> mServerCards;
        std::vector<ServerInfo> mServers;

        Gtk::Entry* chatInput;
        
        // SOCKET clientUDPSocket;
        SOCKET clientTCPSocket;        

        std::thread listenThreadTCP;
        std::thread listenThreadUDP;
        
    private:
        void setup_database();
        void refresh_server_list(const std::string& pServerName, const std::string& pServerURL, const std::string& pServerPort);

        void insert_server_to_database(const std::string& pName, const std::string& pURL, const std::string& pPort);
};