#include <iostream>

#include "Database.hpp"

void database_functions::retrieve_servers(sqlite3* pDBHandle, std::vector<ServerInfo>& pServer) {
    sqlite3_stmt* stmt;
    std::string sql = "SELECT SERVER_NAME, SERVER_USERNAME, SERVER_URL, SERVER_PORT FROM SERVER_LIST;";
    int rc = sqlite3_prepare_v2(pDBHandle, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cout << "SQLite error: " << sqlite3_errmsg(pDBHandle) << std::endl;
        sqlite3_close(pDBHandle);
        return;
    }

    pServer.clear(); // Clear the vector before populating it

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        ServerInfo server;
        server.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        server.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        server.url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        server.port = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        pServer.push_back(server);
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cout << "SQLite error: " << sqlite3_errmsg(pDBHandle) << std::endl;
    }
}