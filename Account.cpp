#include "Account.h"
#include "Crypto.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cmath>

Account::Account()
    : accountNumber(0), ownerName(""), pinHash(""), pinSalt(""),
      balance(0.0), interestRate(0.0), lastInterestEpoch(0) {}

Account::Account(int num, const std::string& name, const std::string& pin)
    : accountNumber(num), ownerName(name),
      balance(0.0), interestRate(0.0), lastInterestEpoch(time(0)) {
    setPin(pin);
}

int Account::getAccountNumber() const { return accountNumber; }
std::string Account::getOwnerName() const { return ownerName; }
double Account::getBalance() const { return balance; }
double Account::getInterestRate() const { return interestRate; }
long Account::getLastInterestEpoch() const { return lastInterestEpoch; }
const std::vector<Transaction>& Account::getHistory() const { return history; }
const std::map<int, std::string>& Account::getPayees() const { return payees; }

bool Account::verifyPin(const std::string& p) const {
    if (pinHash.empty() || pinSalt.empty()) return false;
    return Crypto::hashPin(p, pinSalt) == pinHash;
}

void Account::setPin(const std::string& pin) {
    pinSalt = Crypto::randomSaltHex();
    pinHash = Crypto::hashPin(pin, pinSalt);
}

bool Account::deposit(double amount) {
    if (amount <= 0) return false;
    balance += amount;
    history.push_back(Transaction("DEPOSIT", amount));
    return true;
}

bool Account::withdraw(double amount) {
    if (amount <= 0 || amount > balance) return false;
    balance -= amount;
    history.push_back(Transaction("WITHDRAW", amount));
    return true;
}

bool Account::recordTransferOut(double amount, int toAcctNumber) {
    if (amount <= 0 || amount > balance) return false;
    balance -= amount;
    std::ostringstream oss; oss << "to=" << toAcctNumber;
    history.push_back(Transaction("TRANSFER_OUT", amount, oss.str()));
    return true;
}

bool Account::recordTransferIn(double amount, int fromAcctNumber) {
    if (amount <= 0) return false;
    balance += amount;
    std::ostringstream oss; oss << "from=" << fromAcctNumber;
    history.push_back(Transaction("TRANSFER_IN", amount, oss.str()));
    return true;
}

void Account::setInterestRate(double annualRate) {
    if (annualRate < 0) annualRate = 0;
    interestRate = annualRate;
}

double Account::accrueInterest(long nowEpoch) {
    if (interestRate <= 0 || balance <= 0 || lastInterestEpoch <= 0) {
        lastInterestEpoch = nowEpoch;
        return 0.0;
    }
    long elapsed = nowEpoch - lastInterestEpoch;
    if (elapsed < 86400) return 0.0;
    double days = double(elapsed) / 86400.0;
    double interest = balance * interestRate * (days / 365.0);
    interest = std::floor(interest * 100.0) / 100.0;
    if (interest <= 0) {
        lastInterestEpoch = nowEpoch;
        return 0.0;
    }
    balance += interest;
    lastInterestEpoch = nowEpoch;
    std::ostringstream oss;
    oss << "rate=" << std::fixed << std::setprecision(4) << interestRate
        << " days=" << std::fixed << std::setprecision(2) << days;
    history.push_back(Transaction("INTEREST", interest, oss.str()));
    return interest;
}

void Account::addPayee(int acctNumber, const std::string& nickname) {
    payees[acctNumber] = nickname;
}

bool Account::removePayee(int acctNumber) {
    return payees.erase(acctNumber) > 0;
}

void Account::printHistory() const {
    if (history.empty()) {
        std::cout << "No transactions yet.\n";
        return;
    }
    std::cout << "\n--- Transaction History ---\n";
    for (size_t i = 0; i < history.size(); i++) {
        const Transaction& t = history[i];
        std::cout << (i + 1) << ". [" << t.timestamp << "] "
                  << t.type << " $"
                  << std::fixed << std::setprecision(2) << t.amount;
        if (!t.note.empty()) std::cout << " (" << t.note << ")";
        std::cout << "\n";
    }
}

static std::string csvEscape(const std::string& s) {
    bool needsQuote = false;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == ',' || c == '"' || c == '\n' || c == '\r') { needsQuote = true; break; }
    }
    if (!needsQuote) return s;
    std::string out = "\"";
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '"') out += "\"\"";
        else out += s[i];
    }
    out += "\"";
    return out;
}

bool Account::exportHistoryCsv(const std::string& path) const {
    std::ofstream out(path.c_str());
    if (!out) return false;
    out << "timestamp,type,amount,note\n";
    for (size_t i = 0; i < history.size(); i++) {
        const Transaction& t = history[i];
        out << csvEscape(t.timestamp) << ","
            << csvEscape(t.type) << ","
            << std::fixed << std::setprecision(2) << t.amount << ","
            << csvEscape(t.note) << "\n";
    }
    return true;
}

static std::string encodePayees(const std::map<int, std::string>& payees) {
    std::ostringstream oss;
    bool first = true;
    for (std::map<int, std::string>::const_iterator it = payees.begin();
         it != payees.end(); ++it) {
        if (!first) oss << ",";
        first = false;
        oss << it->first << ":" << it->second;
    }
    return oss.str();
}

static void decodePayees(const std::string& s, std::map<int, std::string>& payees) {
    payees.clear();
    if (s.empty()) return;
    std::string cur;
    for (size_t i = 0; i <= s.size(); i++) {
        if (i == s.size() || s[i] == ',') {
            size_t colon = cur.find(':');
            if (colon != std::string::npos) {
                try {
                    int num = std::stoi(cur.substr(0, colon));
                    payees[num] = cur.substr(colon + 1);
                } catch (...) {}
            }
            cur.clear();
        } else {
            cur += s[i];
        }
    }
}

std::string Account::serialize() const {
    std::ostringstream oss;
    oss << accountNumber << "|" << ownerName << "|"
        << pinHash << "|" << pinSalt << "|"
        << std::fixed << std::setprecision(2) << balance << "|"
        << std::fixed << std::setprecision(6) << interestRate << "|"
        << lastInterestEpoch << "|"
        << encodePayees(payees) << "|";
    for (size_t i = 0; i < history.size(); i++) {
        oss << history[i].serialize();
        if (i + 1 < history.size()) oss << ";";
    }
    return oss.str();
}

Account Account::deserialize(const std::string& line) {
    Account a;
    std::vector<std::string> fields;
    std::string current;
    for (size_t i = 0; i < line.size(); i++) {
        if (line[i] == '|') {
            fields.push_back(current);
            current.clear();
        } else {
            current += line[i];
        }
    }
    fields.push_back(current);

    if (fields.size() < 8) return a;

    try { a.accountNumber = std::stoi(fields[0]); } catch (...) { a.accountNumber = 0; }
    a.ownerName = fields[1];
    a.pinHash = fields[2];
    a.pinSalt = fields[3];
    try { a.balance = std::stod(fields[4]); } catch (...) { a.balance = 0.0; }
    try { a.interestRate = std::stod(fields[5]); } catch (...) { a.interestRate = 0.0; }
    try { a.lastInterestEpoch = std::stol(fields[6]); } catch (...) { a.lastInterestEpoch = 0; }
    decodePayees(fields[7], a.payees);

    if (fields.size() >= 9 && !fields[8].empty()) {
        std::string txns = fields[8];
        std::string txn;
        for (size_t i = 0; i < txns.size(); i++) {
            if (txns[i] == ';') {
                if (!txn.empty()) a.history.push_back(Transaction::deserialize(txn));
                txn.clear();
            } else {
                txn += txns[i];
            }
        }
        if (!txn.empty()) a.history.push_back(Transaction::deserialize(txn));
    }

    return a;
}
