// #include <gtkmm.h>
// #include <thread>
// #include <mutex>
// #include <condition_variable>
// #include <windows.h>


// class HotkeyHandler {
//     public:
//         HotkeyHandler(Gtk::Window& window);


//         ~HotkeyHandler();

//         sigc::signal<void> hotkeyPressed();

//     private:
//         static const int HOTKEY_ID = 1;
//         static const int HOTKEY_MOD = MOD_CONTROL;
//         static const int HOTKEY_KEY = VK_SNAPSHOT;

//         Gtk::Window& mWindow;
//         std::thread mHKThread;
//         std::mutex mHKMutex;
//         std::condition_variable mCV;

//         void HotkeyHandler::message_loop() {
//             HHOOK hhook = SetWindowsHookEx(WH_GETMESSAGE, HotkeyMessageHandler, NULL, GetCurrentThreadId());

//             MSG msg;
//             while (GetMessage(&msg, NULL, 0, 0)) {
//                 TranslateMessage(&msg);
//                 DispatchMessage(&msg);
//             }   

//             UnhookWindowsHookEx(hhook);
//         }

//         static LRESULT CALLBACK HotkeyMessageHandler(int nCode, WPARAM wParam, LPARAM lParam) {
//             if (nCode == HC_ACTION && wParam == WM_HOTKEY) {
//                 int hotkey_id = static_cast<int>(lParam);
//                 if (hotkey_id == HOTKEY_ID) {
//                     std::unique_lock<std::mutex> lock(mHKMutex);
//                     mCV.notify_one();
//                 }
//             }

//             return CallNextHookEx(NULL, nCode, wParam, lParam);
//         }
// };