#include <gtkmm.h>
#include <iostream>
#include <sqlite3.h>

#include "Window.hpp"

int main(int argc, char* argv[]) {
    auto app = Gtk::Application::create("org.voiceops.examples.base");
    return app->make_window_and_run<VoiceOpsWindow>(argc, argv);
}