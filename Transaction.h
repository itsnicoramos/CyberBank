#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>

struct Transaction {
    std::string type;
    double amount;
    std::string timestamp;
    std::string note;

    Transaction();
    Transaction(const std::string& t, double a);
    Transaction(const std::string& t, double a, const std::string& n);

    std::string serialize() const;
    static Transaction deserialize(const std::string& s);
};

#endif
