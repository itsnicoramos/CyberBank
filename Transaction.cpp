#include "Transaction.h"
#include <ctime>
#include <sstream>

static std::string nowStamp() {
    time_t now = time(0);
    char buf[80];
    struct tm* tstruct = localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tstruct);
    return std::string(buf);
}

Transaction::Transaction() : type(""), amount(0.0), timestamp(""), note("") {}

Transaction::Transaction(const std::string& t, double a)
    : type(t), amount(a), timestamp(nowStamp()), note("") {}

Transaction::Transaction(const std::string& t, double a, const std::string& n)
    : type(t), amount(a), timestamp(nowStamp()), note(n) {}

std::string Transaction::serialize() const {
    std::ostringstream oss;
    oss << type << "~" << amount << "~" << timestamp << "~" << note;
    return oss.str();
}

Transaction Transaction::deserialize(const std::string& s) {
    Transaction t;
    size_t p1 = s.find('~');
    if (p1 == std::string::npos) return t;
    size_t p2 = s.find('~', p1 + 1);
    if (p2 == std::string::npos) return t;
    size_t p3 = s.find('~', p2 + 1);

    t.type = s.substr(0, p1);
    try { t.amount = std::stod(s.substr(p1 + 1, p2 - p1 - 1)); }
    catch (...) { t.amount = 0.0; }
    if (p3 == std::string::npos) {
        t.timestamp = s.substr(p2 + 1);
        t.note = "";
    } else {
        t.timestamp = s.substr(p2 + 1, p3 - p2 - 1);
        t.note = s.substr(p3 + 1);
    }
    return t;
}
