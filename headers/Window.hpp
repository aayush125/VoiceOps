#pragma once
#include <gtkmm.h>

#include "Dialogs.hpp"

class VoiceOpsWindow : public Gtk::Window {
    public:
        VoiceOpsWindow();
    
    protected:
        void on_add_button_clicked();
        void on_add_server_response(AddServerDialog& pDialog, int pResponseID);
        // server-list-panel()
        // server-content-panel()
        Gtk::Box* server_list_panel(void*);
        Gtk::Box* server_content_panel(void*);
        Gtk::Box* top_bar();

        Gtk::Button m_button;
};