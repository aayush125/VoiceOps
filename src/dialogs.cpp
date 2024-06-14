#include <iostream>

#include "Dialogs.hpp"

PhotoDialog::PhotoDialog(Gtk::Window* window, Glib::RefPtr<Gdk::Pixbuf> pImage) {
    set_transient_for(*window);
    set_title("Photo");
    int width = (pImage->get_width() < 800) ? pImage->get_width() : 800;
    int height = (pImage->get_height() < 800) ? pImage->get_height() : 800; 
    set_size_request(width, height);

    auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    auto photo = Gtk::make_managed<Gtk::Picture>(pImage);
    box->append(*photo);

    get_content_area()->append(*box);
}

OptionsDialog::OptionsDialog(Gtk::Window* window, std::string message) {
    set_transient_for(*window);
    set_modal(true);
    set_title("Alert");

    auto label = Gtk::make_managed<Gtk::Label>(message);
    label->set_margin(5);
    get_content_area()->append(*label);

    add_button("Yes", Gtk::ResponseType::YES);
    add_button("Cancel", Gtk::ResponseType::CANCEL);

    auto yes_button = get_widget_for_response(Gtk::ResponseType::YES);
    yes_button->set_margin(5);

    auto cancel_button = get_widget_for_response(Gtk::ResponseType::CANCEL);
    cancel_button->set_margin(5);
}

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

    mServerNameEntry = Gtk::make_managed<Gtk::Entry>();
    mServerNameEntry->set_placeholder_text("Server Name");
    mServerNameEntry->set_margin_start(20);
    mServerNameEntry->set_margin_end(20);
    mServerNameEntry->set_margin_bottom(10);
    mServerNameEntry->signal_changed().connect(sigc::mem_fun(*this, &AddServerDialog::on_entry_changed));
    dialogBox->append(*mServerNameEntry);

    mServerPasswordEntry = Gtk::make_managed<Gtk::Entry>();
    mServerPasswordEntry->set_placeholder_text("Server Password");
    mServerPasswordEntry->set_margin_start(20);
    mServerPasswordEntry->set_margin_end(20);
    mServerPasswordEntry->set_margin_bottom(10);
    mServerPasswordEntry->signal_changed().connect(sigc::mem_fun(*this, &AddServerDialog::on_entry_changed));
    dialogBox->append(*mServerPasswordEntry);

    mUserNameEntry = Gtk::make_managed<Gtk::Entry>();
    mUserNameEntry->set_placeholder_text("Username for this server");
    mUserNameEntry->set_margin_start(20);
    mUserNameEntry->set_margin_end(20);
    mUserNameEntry->set_margin_bottom(10);
    mServerNameEntry->signal_changed().connect(sigc::mem_fun(*this, &AddServerDialog::on_entry_changed));
    dialogBox->append(*mUserNameEntry);

    mServerURLEntry = Gtk::make_managed<Gtk::Entry>();
    mServerURLEntry->set_placeholder_text("Server URL");
    mServerURLEntry->set_margin_start(20);
    mServerURLEntry->set_margin_end(20);
    mServerURLEntry->set_margin_bottom(10);
    mServerURLEntry->signal_changed().connect(sigc::mem_fun(*this, &AddServerDialog::on_entry_changed));
    dialogBox->append(*mServerURLEntry);

    mServerPortEntry = Gtk::make_managed<Gtk::Entry>();
    mServerPortEntry->set_placeholder_text("Server Port");
    mServerPortEntry->set_margin_start(20);
    mServerPortEntry->set_margin_end(20);
    mServerPortEntry->set_margin_bottom(10);
    mServerPortEntry->signal_changed().connect(sigc::mem_fun(*this, &AddServerDialog::on_entry_changed));
    dialogBox->append(*mServerPortEntry);

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

const std::string& AddServerDialog::get_server_pass() {
    mServerPassword = mServerPasswordEntry->get_text();
    return mServerPassword;
}

const std::string& AddServerDialog::get_server_username() {
    mUserName = mUserNameEntry->get_text();
    return mUserName;
}


const std::string& AddServerDialog::get_server_url() {
    mServerURL = mServerURLEntry->get_text();
    return mServerURL;
}

const std::string& AddServerDialog::get_server_port() {
    mServerPort = mServerPortEntry->get_text();
    return mServerPort;
}