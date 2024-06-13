#include <gtkmm.h>
#include <iostream>
#include <sqlite3.h>
#include "Window.hpp"

#include <fstream>

int main(int argc, char* argv[]) {
    // Loading the dll file
    WSADATA wsaData;
    int wsaerr;
    WORD wVersionRequested = MAKEWORD(2, 2);
    wsaerr = WSAStartup(wVersionRequested, &wsaData);
    if (wsaerr != 0) {
        std::cout << "The winsock dll not found!" << std::endl;
        return 0;
    }

    Glib::setenv("GSK_RENDERER", "cairo", false);
    Glib::setenv("PANGOCAIRO_BACKEND", "fc", false);

    auto app = Gtk::Application::create("org.voiceops.examples.base");

    // Glib::signal_idle().connect(sigc::ptr_fun(&process_windows_messages));

    auto cssProvider = Gtk::CssProvider::create();
    std::ifstream cssFile("./css/style.css");
    std::string cssData((std::istreambuf_iterator<char>(cssFile)), std::istreambuf_iterator<char>());
    cssProvider->load_from_data(cssData);

    auto screen = Gdk::Display::get_default();

    Gtk::StyleContext::add_provider_for_display(screen, cssProvider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, NULL);

    int ret = app->make_window_and_run<VoiceOpsWindow>(argc, argv);

    WSACleanup();

    return ret;
}
