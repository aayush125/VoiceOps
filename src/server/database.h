#pragma once
#include <iostream>
#include <sqlite3.h>
#include <vector>

struct data {
    int id;
    std::string sender;
    std::string message;
    int channelId;

};

//Creates the database if not available
static int createDB(const char* databasePath) {
    sqlite3* DB;
    int exit = sqlite3_open(databasePath, &DB);
    sqlite3_close(DB);
    return 0;
}

static int callback(void* dataPtr, int argc, char** argv, char** azColName) {
    std::vector<data>* databaseContent = static_cast<std::vector<data>*>(dataPtr);
    data row;
    for (int i = 0; i < argc; i++) {
        if (std::string(azColName[i]) == "ID") {
            row.id = std::stoi(argv[i]);
        }
        else if (std::string(azColName[i]) == "SENDER") {
            row.sender = argv[i] ? argv[i] : "";
        }
        else if (std::string(azColName[i]) == "MESSAGE") {
            row.message = argv[i] ? argv[i] : "";
        }
        else if (std::string(azColName[i]) == "CHANNELID") {
            row.channelId = std::stoi(argv[i]);
        }
    }
    databaseContent->push_back(row);
    return 0;
}

static int createTable(const char* databasePath) {
    sqlite3* DB;
    std::string sqlCommand = "Create TABLE IF NOT EXISTS MESSAGES("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
        "SENDER		TEXT NOT NULL,"
        "MESSAGE	TEXT NOT NULL,"
        "CHANNELID	INTEGER NOT NULL);";

    try {
        int exit = 0;
        exit = sqlite3_open(databasePath, &DB);

        char* messageError;
        exit = sqlite3_exec(DB, sqlCommand.c_str(), NULL, 0, &messageError);

        if (exit != SQLITE_OK) {
            std::cerr << "Error Creating a table" << std::endl;
            sqlite3_free(messageError);
        }
        sqlite3_close(DB);
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}

static int insertData(const char* databasePath, std::string clientID, std::string message, int channelID) {
    sqlite3* DB;
    sqlite3_stmt* statement;
    int exit = sqlite3_open(databasePath, &DB);

    if (exit != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(DB) << std::endl;
        return exit;
    }

    std::string sql = "INSERT INTO MESSAGES (SENDER, MESSAGE, CHANNELID) VALUES (?, ?, ?);";
    exit = sqlite3_prepare_v2(DB, sql.c_str(), -1, &statement, NULL);
    if (exit != SQLITE_OK) {
        std::cerr << "Error preparing statement: " << sqlite3_errmsg(DB) << std::endl;
        sqlite3_close(DB);
        return exit;
    }
    
    sqlite3_bind_text(statement, 1, clientID.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, message.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(statement, 3, channelID);

    exit = sqlite3_step(statement);
    if (exit != SQLITE_DONE) {
        std::cerr << "Error inserting data: " << sqlite3_errmsg(DB) << std::endl;
        return exit;
    }

    sqlite3_finalize(statement);
    sqlite3_close(DB);

    return 0;
}

static std::vector<data> selectData(const char* databasePath, int queryLimit) {
    sqlite3* DB;
    int exit = sqlite3_open(databasePath, &DB);
    std::vector<data> databaseContent;
    std::string sql = "SELECT * FROM MESSAGES ORDER BY ID DESC LIMIT " + std::to_string(queryLimit);
    sqlite3_exec(DB, sql.c_str(), callback, &databaseContent, NULL);
    sqlite3_close(DB);
    return databaseContent;
}
