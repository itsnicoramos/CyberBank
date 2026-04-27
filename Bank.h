#ifndef BANK_H
#define BANK_H

#include <vector>
#include <string>
#include <memory>
#include "Account.h"
#include "Storage.h"

class Bank {
private:
    std::vector<Account> accounts;
    int nextAccountNumber;
    std::unique_ptr<Storage> storage;

    Account* findAccount(int accountNumber);

public:
    explicit Bank(std::unique_ptr<Storage> s);

    void createAccount();
    Account* login();

    void deposit(Account* acct);
    void withdraw(Account* acct);
    void checkBalance(Account* acct);
    void viewHistory(Account* acct);

    void transfer(Account* acct);
    void managePayees(Account* acct);
    void manageInterest(Account* acct);
    void exportCsv(Account* acct);

    bool save();
    bool load();

    void runMenu();
};

#endif
