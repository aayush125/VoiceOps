#include "Dialogs.hpp"

AddServerDialog::AddServerDialog(Gtk::Window* window)
{
    set_transient_for(*window);
    set_title("Add Server");
    set_size_request(400, 150);

    auto label = Gtk::make_managed<Gtk::Label>("Here's where you add the server.");
    label->set_margin(20);
    label->set_halign(Gtk::Align::CENTER);
    label->set_valign(Gtk::Align::CENTER);

    get_content_area()->append(*label);

    add_button("OK", Gtk::ResponseType::OK);

    auto ok_button = get_widget_for_response(Gtk::ResponseType::OK);
    ok_button->set_margin_end(5);
}