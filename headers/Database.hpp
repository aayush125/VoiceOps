#pragma once

#include <sqlite3.h>
#include <vector>

#include "Utils.hpp"

namespace database_functions {
    void retrieve_servers(sqlite3* pDBHandle, std::vector<ServerInfo>& pServers);
    void remove_server(sqlite3* pDBHandle, const std::string& serverName);
    std::string retrieve_password(sqlite3* pDBHandle, const std::string& serverName);
}