#include <iostream>

#include "Dialogs.hpp"

AddServerDialog::AddServerDialog(Gtk::Window* window) {
    set_transient_for(*window);
    set_title("Add Server");
    set_size_request(400, 150);

    auto dialogBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    dialogBox->set_name("dialog-inner-wrapper");

    auto label = Gtk::make_managed<Gtk::Label>("Enter the server's URL and port number.");
    label->set_margin(20);
    label->set_halign(Gtk::Align::CENTER);
    label->set_valign(Gtk::Align::CENTER);
    dialogBox->append(*label);
    // get_content_area()->append(*label);

    mServerNameEntry = Gtk::make_managed<Gtk::Entry>();
    mServerNameEntry->set_placeholder_text("Server Name");
    mServerNameEntry->set_margin_start(20);
    mServerNameEntry->set_margin_end(20);
    mServerNameEntry->set_margin_bottom(10);
    mServerNameEntry->signal_changed().connect(sigc::mem_fun(*this, &AddServerDialog::on_entry_changed));
    dialogBox->append(*mServerNameEntry);
    // get_content_area()->append(*mServerNameEntry);

    mServerURLEntry = Gtk::make_managed<Gtk::Entry>();
    mServerURLEntry->set_placeholder_text("Server URL");
    mServerURLEntry->set_margin_start(20);
    mServerURLEntry->set_margin_end(20);
    mServerURLEntry->set_margin_bottom(10);
    mServerURLEntry->signal_changed().connect(sigc::mem_fun(*this, &AddServerDialog::on_entry_changed));
    dialogBox->append(*mServerURLEntry);
    // get_content_area()->append(*mServerURLEntry);

    mServerPortEntry = Gtk::make_managed<Gtk::Entry>();
    mServerPortEntry->set_placeholder_text("Server Port");
    mServerPortEntry->set_margin_start(20);
    mServerPortEntry->set_margin_end(20);
    mServerPortEntry->set_margin_bottom(10);
    mServerPortEntry->signal_changed().connect(sigc::mem_fun(*this, &AddServerDialog::on_entry_changed));
    dialogBox->append(*mServerPortEntry);
    // get_content_area()->append(*mServerPortEntry);

    get_content_area()->append(*dialogBox);

    add_button("OK", Gtk::ResponseType::OK);
    auto ok_button = get_widget_for_response(Gtk::ResponseType::OK);
    ok_button->set_margin_end(5);
    ok_button->set_margin_bottom(5);
    ok_button->set_sensitive(false);
    ok_button->set_name("dialog-ok-button");
}

void AddServerDialog::on_entry_changed() {
    auto ok_button = get_widget_for_response(Gtk::ResponseType::OK);
    bool all_filled = !mServerNameEntry->get_text().empty() &&
                        !mServerURLEntry->get_text().empty() &&
                        !mServerPortEntry->get_text().empty();
    ok_button->set_sensitive(all_filled);
}

const std::string& AddServerDialog::get_server_name() {
    mServerName = mServerNameEntry->get_text();
    return mServerName;
}


const std::string& AddServerDialog::get_server_url() {
    mServerURL = mServerURLEntry->get_text();
    return mServerURL;
}

const std::string& AddServerDialog::get_server_port() {
    mServerPort = mServerPortEntry->get_text();
    return mServerPort;
}