#include "Bank.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <sstream>

Bank::Bank(std::unique_ptr<Storage> s)
    : nextAccountNumber(1001), storage(std::move(s)) {}

Account* Bank::findAccount(int accountNumber) {
    for (size_t i = 0; i < accounts.size(); i++) {
        if (accounts[i].getAccountNumber() == accountNumber) return &accounts[i];
    }
    return nullptr;
}

bool Bank::save() {
    if (!storage) return false;
    return storage->save(accounts, nextAccountNumber);
}

bool Bank::load() {
    if (!storage) return false;
    return storage->load(accounts, nextAccountNumber);
}

static bool readPin(std::string& pin, const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        if (!std::getline(std::cin, pin)) return false;
        if (pin.size() != 4) {
            std::cout << "PIN must be exactly 4 digits.\n";
            continue;
        }
        bool digits = true;
        for (size_t i = 0; i < pin.size(); i++) {
            if (pin[i] < '0' || pin[i] > '9') { digits = false; break; }
        }
        if (!digits) {
            std::cout << "PIN must contain only digits.\n";
            continue;
        }
        return true;
    }
}

static bool readDouble(double& out, const std::string& prompt) {
    std::cout << prompt;
    std::string input;
    if (!std::getline(std::cin, input)) return false;
    try { out = std::stod(input); return true; }
    catch (...) { std::cout << "Invalid number.\n"; return false; }
}

static bool readInt(int& out, const std::string& prompt) {
    std::cout << prompt;
    std::string input;
    if (!std::getline(std::cin, input)) return false;
    try { out = std::stoi(input); return true; }
    catch (...) { std::cout << "Invalid number.\n"; return false; }
}

void Bank::createAccount() {
    std::string name, pin;
    std::cout << "\n--- Create New Account ---\n";
    std::cout << "Enter your full name: ";
    std::getline(std::cin, name);
    if (name.empty()) {
        std::cout << "Name cannot be empty.\n";
        return;
    }
    if (name.find('|') != std::string::npos) {
        std::cout << "Name cannot contain '|'.\n";
        return;
    }

    if (!readPin(pin, "Set a 4-digit PIN: ")) return;

    Account a(nextAccountNumber++, name, pin);
    accounts.push_back(a);
    save();
    std::cout << "Account created. Your account number is "
              << a.getAccountNumber() << "\n";
}

Account* Bank::login() {
    int num;
    std::cout << "\n--- Login ---\n";
    if (!readInt(num, "Account number: ")) return nullptr;

    Account* a = findAccount(num);
    if (!a) { std::cout << "Account not found.\n"; return nullptr; }

    std::cout << "PIN: ";
    std::string pin;
    std::getline(std::cin, pin);
    if (!a->verifyPin(pin)) {
        std::cout << "Incorrect PIN.\n";
        return nullptr;
    }

    double accrued = a->accrueInterest(time(0));
    if (accrued > 0) {
        std::cout << "Accrued $" << std::fixed << std::setprecision(2)
                  << accrued << " in interest since last login.\n";
        save();
    }

    std::cout << "Welcome, " << a->getOwnerName() << "!\n";
    return a;
}

void Bank::deposit(Account* acct) {
    if (!acct) { std::cout << "Please log in first.\n"; return; }
    double amount;
    if (!readDouble(amount, "Amount to deposit: $")) return;

    if (acct->deposit(amount)) {
        std::cout << "Deposited $" << std::fixed << std::setprecision(2)
                  << amount << "\n";
        std::cout << "New balance: $" << acct->getBalance() << "\n";
        save();
    } else {
        std::cout << "Deposit failed. Amount must be positive.\n";
    }
}

void Bank::withdraw(Account* acct) {
    if (!acct) { std::cout << "Please log in first.\n"; return; }
    double amount;
    if (!readDouble(amount, "Amount to withdraw: $")) return;

    if (acct->withdraw(amount)) {
        std::cout << "Withdrew $" << std::fixed << std::setprecision(2)
                  << amount << "\n";
        std::cout << "New balance: $" << acct->getBalance() << "\n";
        save();
    } else {
        std::cout << "Withdrawal failed. Check amount and balance.\n";
    }
}

void Bank::checkBalance(Account* acct) {
    if (!acct) { std::cout << "Please log in first.\n"; return; }
    std::cout << "Current balance: $" << std::fixed << std::setprecision(2)
              << acct->getBalance() << "\n";
}

void Bank::viewHistory(Account* acct) {
    if (!acct) { std::cout << "Please log in first.\n"; return; }
    acct->printHistory();
}

void Bank::transfer(Account* acct) {
    if (!acct) { std::cout << "Please log in first.\n"; return; }

    std::cout << "\n--- Transfer ---\n";
    const std::map<int, std::string>& payees = acct->getPayees();
    if (!payees.empty()) {
        std::cout << "Saved payees:\n";
        for (std::map<int, std::string>::const_iterator it = payees.begin();
             it != payees.end(); ++it) {
            std::cout << "  " << it->first << " - " << it->second << "\n";
        }
    } else {
        std::cout << "(No saved payees yet.)\n";
    }

    int toNum;
    if (!readInt(toNum, "Recipient account number: ")) return;
    if (toNum == acct->getAccountNumber()) {
        std::cout << "Cannot transfer to your own account.\n";
        return;
    }
    Account* dest = findAccount(toNum);
    if (!dest) { std::cout << "Recipient account not found.\n"; return; }

    double amount;
    if (!readDouble(amount, "Amount to transfer: $")) return;
    if (amount <= 0) { std::cout << "Amount must be positive.\n"; return; }
    if (amount > acct->getBalance()) {
        std::cout << "Insufficient funds.\n"; return;
    }

    if (!acct->recordTransferOut(amount, toNum)) {
        std::cout << "Transfer failed.\n"; return;
    }
    dest->recordTransferIn(amount, acct->getAccountNumber());

    std::cout << "Transferred $" << std::fixed << std::setprecision(2)
              << amount << " to account " << toNum
              << " (" << dest->getOwnerName() << ").\n";
    std::cout << "New balance: $" << acct->getBalance() << "\n";

    if (payees.find(toNum) == payees.end()) {
        std::cout << "Save '" << dest->getOwnerName()
                  << "' as a payee? (y/n): ";
        std::string yn; std::getline(std::cin, yn);
        if (!yn.empty() && (yn[0] == 'y' || yn[0] == 'Y')) {
            std::cout << "Nickname (no '|', ',', or ':'): ";
            std::string nick; std::getline(std::cin, nick);
            if (nick.find('|') == std::string::npos &&
                nick.find(',') == std::string::npos &&
                nick.find(':') == std::string::npos &&
                !nick.empty()) {
                acct->addPayee(toNum, nick);
            } else {
                std::cout << "Invalid nickname; payee not saved.\n";
            }
        }
    }
    save();
}

void Bank::managePayees(Account* acct) {
    if (!acct) { std::cout << "Please log in first.\n"; return; }
    while (true) {
        std::cout << "\n--- Payee Directory ---\n";
        const std::map<int, std::string>& payees = acct->getPayees();
        if (payees.empty()) {
            std::cout << "(No payees yet.)\n";
        } else {
            for (std::map<int, std::string>::const_iterator it = payees.begin();
                 it != payees.end(); ++it) {
                std::cout << "  " << it->first << " - " << it->second << "\n";
            }
        }
        std::cout << "1. Add payee\n2. Remove payee\n3. Back\nChoice: ";
        std::string choice; if (!std::getline(std::cin, choice)) return;
        if (choice == "1") {
            int num;
            if (!readInt(num, "Payee account number: ")) continue;
            Account* dest = findAccount(num);
            if (!dest) { std::cout << "Account not found.\n"; continue; }
            std::cout << "Nickname (no '|', ',', or ':'): ";
            std::string nick; std::getline(std::cin, nick);
            if (nick.empty() ||
                nick.find('|') != std::string::npos ||
                nick.find(',') != std::string::npos ||
                nick.find(':') != std::string::npos) {
                std::cout << "Invalid nickname.\n"; continue;
            }
            acct->addPayee(num, nick);
            save();
            std::cout << "Payee saved.\n";
        } else if (choice == "2") {
            int num;
            if (!readInt(num, "Account number to remove: ")) continue;
            if (acct->removePayee(num)) {
                save();
                std::cout << "Payee removed.\n";
            } else {
                std::cout << "Payee not found.\n";
            }
        } else if (choice == "3") {
            return;
        } else {
            std::cout << "Invalid choice.\n";
        }
    }
}

void Bank::manageInterest(Account* acct) {
    if (!acct) { std::cout << "Please log in first.\n"; return; }
    std::cout << "\n--- Interest Settings ---\n";
    std::cout << "Current annual interest rate: "
              << std::fixed << std::setprecision(4)
              << (acct->getInterestRate() * 100.0) << "%\n";

    std::cout << "1. Set new rate\n2. Accrue interest now\n3. Back\nChoice: ";
    std::string choice; if (!std::getline(std::cin, choice)) return;
    if (choice == "1") {
        double pct;
        if (!readDouble(pct, "New annual rate (e.g. 2.5 for 2.5%): ")) return;
        if (pct < 0) { std::cout << "Rate cannot be negative.\n"; return; }
        acct->setInterestRate(pct / 100.0);
        save();
        std::cout << "Rate updated.\n";
    } else if (choice == "2") {
        double accrued = acct->accrueInterest(time(0));
        if (accrued > 0) {
            std::cout << "Credited $" << std::fixed << std::setprecision(2)
                      << accrued << ".\n";
            std::cout << "New balance: $" << acct->getBalance() << "\n";
            save();
        } else {
            std::cout << "No interest accrued (need positive balance, "
                         "rate, and at least 1 day elapsed).\n";
        }
    }
}

void Bank::exportCsv(Account* acct) {
    if (!acct) { std::cout << "Please log in first.\n"; return; }
    std::ostringstream oss;
    oss << "acct_" << acct->getAccountNumber() << "_history.csv";
    std::string path = oss.str();
    if (acct->exportHistoryCsv(path)) {
        std::cout << "Exported transaction history to " << path << "\n";
    } else {
        std::cout << "Export failed: could not write " << path << "\n";
    }
}

void Bank::runMenu() {
    Account* current = nullptr;
    if (storage) std::cout << "Storage: " << storage->describe() << "\n";

    while (true) {
        std::cout << "\n=== CyberBank CLI ===\n";
        if (current) {
            std::cout << "Logged in as: " << current->getOwnerName()
                      << " (Acct " << current->getAccountNumber() << ")\n";
        }
        std::cout << "1.  Create a new account\n";
        std::cout << "2.  Log in\n";
        std::cout << "3.  Deposit money\n";
        std::cout << "4.  Withdraw money\n";
        std::cout << "5.  Check balance\n";
        std::cout << "6.  View transaction history\n";
        std::cout << "7.  Transfer to another account\n";
        std::cout << "8.  Manage payees\n";
        std::cout << "9.  Interest settings\n";
        std::cout << "10. Export history to CSV\n";
        std::cout << "11. Save and exit\n";
        std::cout << "Choice: ";

        std::string choice;
        if (!std::getline(std::cin, choice)) break;

        if (choice == "1") createAccount();
        else if (choice == "2") { Account* a = login(); if (a) current = a; }
        else if (choice == "3") deposit(current);
        else if (choice == "4") withdraw(current);
        else if (choice == "5") checkBalance(current);
        else if (choice == "6") viewHistory(current);
        else if (choice == "7") transfer(current);
        else if (choice == "8") managePayees(current);
        else if (choice == "9") manageInterest(current);
        else if (choice == "10") exportCsv(current);
        else if (choice == "11") {
            if (save()) std::cout << "Saved. Goodbye!\n";
            else std::cout << "Save failed.\n";
            break;
        }
        else std::cout << "Invalid choice. Please pick 1-11.\n";
    }
}
