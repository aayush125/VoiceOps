#pragma once

#define NOGDI
#include <winsock2.h>

#define MAX_CLIENTS 10

namespace Voice {
    void init();
    void toggle();
    void newThread(SOCKET UDPSocket);
    void joinThread();
    void forceStop();
    bool getVoiceStatus();
}
