#pragma once

#include <gtkmm.h>
#include <string>

struct ServerInfo {
    std::string name;
    std::string username;
    std::string url;
    std::string port;
};

struct ServerCard {
    ServerInfo info;
    Gtk::Button* button;
    std::string previousSender;
    std::string selectedFilePath;
};

struct DataStore {
    char buffer[4096];
    size_t bytes;
};