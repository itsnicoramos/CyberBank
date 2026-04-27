#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <string>
#include <vector>
#include <map>
#include "Transaction.h"

class Account {
private:
    int accountNumber;
    std::string ownerName;
    std::string pinHash;
    std::string pinSalt;
    double balance;
    double interestRate;
    long lastInterestEpoch;
    std::map<int, std::string> payees;
    std::vector<Transaction> history;

public:
    Account();
    Account(int num, const std::string& name, const std::string& pin);

    int getAccountNumber() const;
    std::string getOwnerName() const;
    double getBalance() const;
    double getInterestRate() const;
    long getLastInterestEpoch() const;
    const std::vector<Transaction>& getHistory() const;
    const std::map<int, std::string>& getPayees() const;

    bool verifyPin(const std::string& p) const;
    void setPin(const std::string& pin);

    bool deposit(double amount);
    bool withdraw(double amount);
    bool recordTransferOut(double amount, int toAcctNumber);
    bool recordTransferIn(double amount, int fromAcctNumber);

    void setInterestRate(double annualRate);
    double accrueInterest(long nowEpoch);

    void addPayee(int acctNumber, const std::string& nickname);
    bool removePayee(int acctNumber);

    void printHistory() const;
    bool exportHistoryCsv(const std::string& path) const;

    std::string serialize() const;
    static Account deserialize(const std::string& line);
};

#endif
