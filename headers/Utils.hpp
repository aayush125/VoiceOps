#pragma once

#include <gtkmm.h>
#include <string>

struct ServerInfo {
    std::string name;
    std::string url;
    std::string port;
};

struct ServerCard {
    ServerInfo info;
    Gtk::Button* button;
};