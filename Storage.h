#ifndef STORAGE_H
#define STORAGE_H

#include <string>
#include <vector>
#include "Account.h"

class Storage {
public:
    virtual ~Storage() {}
    virtual bool save(const std::vector<Account>& accounts, int nextAccountNumber) = 0;
    virtual bool load(std::vector<Account>& accounts, int& nextAccountNumber) = 0;
    virtual std::string describe() const = 0;
};

class FileStorage : public Storage {
    std::string filename;
public:
    explicit FileStorage(const std::string& f);
    bool save(const std::vector<Account>& accounts, int nextAccountNumber) override;
    bool load(std::vector<Account>& accounts, int& nextAccountNumber) override;
    std::string describe() const override;
};

#ifdef CYBERBANK_HAVE_SQLITE
class SqliteStorage : public Storage {
    std::string dbPath;
public:
    explicit SqliteStorage(const std::string& path);
    bool save(const std::vector<Account>& accounts, int nextAccountNumber) override;
    bool load(std::vector<Account>& accounts, int& nextAccountNumber) override;
    std::string describe() const override;
};
#endif

#endif
