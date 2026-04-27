#include "Bank.h"
#include "Storage.h"
#include <iostream>
#include <cstdlib>
#include <memory>
#include <string>

int main() {
    std::unique_ptr<Storage> storage;
    const char* backend = std::getenv("CYBERBANK_STORAGE");
    std::string choice = backend ? backend : "file";

    if (choice == "sqlite") {
#ifdef CYBERBANK_HAVE_SQLITE
        const char* dbPath = std::getenv("CYBERBANK_DB");
        storage.reset(new SqliteStorage(dbPath ? dbPath : "cyberbank.db"));
#else
        std::cerr << "SQLite backend not compiled in; falling back to file.\n";
        storage.reset(new FileStorage("accounts.txt"));
#endif
    } else {
        const char* filePath = std::getenv("CYBERBANK_FILE");
        storage.reset(new FileStorage(filePath ? filePath : "accounts.txt"));
    }

    Bank bank(std::move(storage));
    bank.load();
    std::cout << "Welcome to CyberBank CLI\n";
    bank.runMenu();
    return 0;
}
