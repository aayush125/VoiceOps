#include <iostream>

#include "Dialogs.hpp"
#include "Window.hpp"

VoiceOpsWindow::VoiceOpsWindow() {
    set_title("VoiceOps");

    auto hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);
    auto server_list = server_list_panel(nullptr);
    auto server_content = server_content_panel(nullptr);

    hbox->append(*server_list);
    hbox->append(*server_content);

    set_child(*hbox);
    set_default_size(500, 500);
}

Gtk::Box* VoiceOpsWindow::server_list_panel(void* pServerList) {
    auto mainVBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    auto topBarVBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    auto serverListVBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    serverListVBox->set_vexpand(true);

    Gtk::Label* label = nullptr;

    auto topBar = top_bar();
    topBarVBox->append(*topBar);
    mainVBox->append(*topBarVBox);

    if (!pServerList) {
        label = Gtk::make_managed<Gtk::Label>("You have not joined a server yet.");
        serverListVBox->append(*label);
        serverListVBox->set_valign(Gtk::Align::CENTER);
        mainVBox->append(*serverListVBox);

        mainVBox->set_margin_start(10);
        return mainVBox;
    }
    
    mainVBox->append(*serverListVBox);

    mainVBox->set_margin_start(10);    
    return mainVBox;
}

Gtk::Box* VoiceOpsWindow::server_content_panel(void* pSelectedServer) {
    auto mainHBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);
    mainHBox->set_hexpand(true);
    Gtk::Label* emptyLabel = nullptr;

    if (!pSelectedServer) {
        emptyLabel = Gtk::make_managed<Gtk::Label>("Select a server from the left to view its contents.");
        mainHBox->append(*emptyLabel);
        mainHBox->set_valign(Gtk::Align::CENTER);
        mainHBox->set_halign(Gtk::Align::CENTER);
        mainHBox->set_margin_start(10);

        return mainHBox;
    }

    mainHBox->set_margin_start(10);
    return mainHBox;
}

Gtk::Box* VoiceOpsWindow::top_bar() {
    auto hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);
    auto add_button = Gtk::make_managed<Gtk::Button>("+");
    add_button->signal_clicked().connect(sigc::mem_fun(*this, &VoiceOpsWindow::on_add_button_clicked));

    add_button->set_margin(5);

    hbox->append(*add_button);

    hbox->set_halign(Gtk::Align::END);

    return hbox;
}

void VoiceOpsWindow::on_add_button_clicked() {
    std::cout << "Add server button was clicked\n";

    auto dialog = Gtk::make_managed<AddServerDialog>(this);
    dialog->signal_response().connect([this, dialog](int response_id) {on_add_server_response(*dialog, response_id);});
    dialog->show();
}

void VoiceOpsWindow::on_add_server_response(AddServerDialog& pDialog, int pResponseID) {
    switch (pResponseID) {
        case Gtk::ResponseType::OK:
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