#pragma once

#include <iostream>

#include "Utils.hpp"
#include "Window.hpp"
#include <ws2tcpip.h>

SOCKET createSocket(ServerInfo &server_info);

void ReceiveMessages(SOCKET& clientSocket, GMutex& mutex, DataStore& data, VoiceOpsWindow& windowref);