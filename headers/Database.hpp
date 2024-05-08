#pragma once

#include <sqlite3.h>
#include <vector>

#include "Utils.hpp"

namespace database_functions {
    void retrieve_servers(sqlite3* pDBHandle, std::vector<ServerInfo>& pServers);
}