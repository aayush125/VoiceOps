#pragma once

#include <iostream>

#include "Utils.hpp"
#include "Window.hpp"
#include <ws2tcpip.h>

bool createSocket(ServerInfo& server_info, SOCKET* tcpSocket, SOCKET* udpSocket);

void ReceiveMessages(SOCKET clientSocket, GMutex& mutex, DataStore& data, VoiceOpsWindow& windowref);
