#include "Storage.h"
#include <fstream>
#include <sstream>
#include <iomanip>

#ifdef CYBERBANK_HAVE_SQLITE
#include <sqlite3.h>
#endif

static const char* FILE_FORMAT_TAG = "CYBERBANK_V2";

FileStorage::FileStorage(const std::string& f) : filename(f) {}

std::string FileStorage::describe() const {
    return "file:" + filename;
}

bool FileStorage::save(const std::vector<Account>& accounts, int nextAccountNumber) {
    std::ofstream out(filename.c_str());
    if (!out) return false;
    out << FILE_FORMAT_TAG << "\n";
    out << nextAccountNumber << "\n";
    for (size_t i = 0; i < accounts.size(); i++) {
        out << accounts[i].serialize() << "\n";
    }
    return true;
}

bool FileStorage::load(std::vector<Account>& accounts, int& nextAccountNumber) {
    std::ifstream in(filename.c_str());
    if (!in) return false;
    std::string line;
    if (!std::getline(in, line)) return true;

    if (line == FILE_FORMAT_TAG) {
        if (std::getline(in, line)) {
            try { nextAccountNumber = std::stoi(line); } catch (...) {}
        }
    } else {
        try { nextAccountNumber = std::stoi(line); } catch (...) {}
    }

    while (std::getline(in, line)) {
        if (!line.empty()) accounts.push_back(Account::deserialize(line));
    }
    return true;
}

#ifdef CYBERBANK_HAVE_SQLITE

SqliteStorage::SqliteStorage(const std::string& path) : dbPath(path) {}

std::string SqliteStorage::describe() const {
    return "sqlite:" + dbPath;
}

static bool execSql(sqlite3* db, const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK;
}

bool SqliteStorage::save(const std::vector<Account>& accounts, int nextAccountNumber) {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }

    execSql(db,
        "CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT);"
        "CREATE TABLE IF NOT EXISTS accounts (line TEXT NOT NULL);");

    if (!execSql(db, "BEGIN TRANSACTION;")) { sqlite3_close(db); return false; }
    execSql(db, "DELETE FROM accounts;");

    {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db,
            "INSERT OR REPLACE INTO meta(key,value) VALUES('next_account_number', ?);",
            -1, &stmt, nullptr);
        std::ostringstream oss; oss << nextAccountNumber;
        std::string v = oss.str();
        sqlite3_bind_text(stmt, 1, v.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db,
            "INSERT INTO accounts(line) VALUES (?);", -1, &stmt, nullptr);
        for (size_t i = 0; i < accounts.size(); i++) {
            std::string s = accounts[i].serialize();
            sqlite3_bind_text(stmt, 1, s.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
    }

    bool ok = execSql(db, "COMMIT;");
    sqlite3_close(db);
    return ok;
}

bool SqliteStorage::load(std::vector<Account>& accounts, int& nextAccountNumber) {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }
    execSql(db,
        "CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT);"
        "CREATE TABLE IF NOT EXISTS accounts (line TEXT NOT NULL);");

    {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db,
            "SELECT value FROM meta WHERE key='next_account_number';",
            -1, &stmt, nullptr);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* v = sqlite3_column_text(stmt, 0);
            if (v) {
                try { nextAccountNumber = std::stoi(reinterpret_cast<const char*>(v)); }
                catch (...) {}
            }
        }
        sqlite3_finalize(stmt);
    }

    {
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, "SELECT line FROM accounts;", -1, &stmt, nullptr);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* v = sqlite3_column_text(stmt, 0);
            if (v) accounts.push_back(Account::deserialize(
                reinterpret_cast<const char*>(v)));
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return true;
}

#endif
