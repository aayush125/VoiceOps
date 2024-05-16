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

struct DataStore {
    char buffer[4096];
    size_t bytes;
};