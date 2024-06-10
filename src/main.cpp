#include <gtkmm.h>
#include <iostream>
#include <sqlite3.h>

#include "Window.hpp"

#include <fstream>

// bool register_hotkey() {
//     std::cout << "Register hotkey\n";
//     return RegisterHotKey(NULL, HOTKEY_ID, MOD_CONTROL, 'M');
// }

// void unregister_hotkey() {
//     std::cout << "Unregister hotkey\n";
//     UnregisterHotKey(NULL, HOTKEY_ID);
// }

// bool process_windows_messages() {
//     static bool registered = false;
//     if (!registered) {
//         register_hotkey();
//         registered = true;
//     }

//     MSG msg;
//     while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
//         if (msg.message == WM_HOTKEY && msg.wParam == HOTKEY_ID) {
//             std::cout << "Hotkey pressed" << std::endl;
//         }
//     }

//     return true;
// }

int main(int argc, char *argv[])
{
    // Loading the dll file
    WSADATA wsaData;
    int wsaerr;
    WORD wVersionRequested = MAKEWORD(2, 2);
    wsaerr = WSAStartup(wVersionRequested, &wsaData);
    if (wsaerr != 0)
    {
        std::cout << "The winsock dll not found!" << std::endl;
        return 0;
    }
    else
    {
        std::cout << "The winsock dll was found!" << std::endl;
        std::cout << "Status: " << wsaData.szSystemStatus << std::endl;
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

    return app->make_window_and_run<VoiceOpsWindow>(argc, argv);

    // unregister_hotkey();
    WSACleanup();
}