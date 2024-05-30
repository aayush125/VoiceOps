#pragma once
#include <gtkmm.h>

class AddServerDialog : public Gtk::Dialog {
    public:
        AddServerDialog(Gtk::Window*);
        const std::string& get_server_name();
        const std::string& get_server_username();
        const std::string& get_server_url();
        const std::string& get_server_port();

    private:
        Gtk::Entry* mServerNameEntry;
        Gtk::Entry* mUserNameEntry;
        Gtk::Entry* mServerURLEntry;
        Gtk::Entry* mServerPortEntry;

        std::string mServerName;
        std::string mUserName;
        std::string mServerURL;
        std::string mServerPort;

        void on_entry_changed();
};