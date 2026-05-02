#include "UserManager.h"
#include "TimeUtils.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <random>
#include <iomanip>
#include <functional>

UserManager::UserManager(const std::string& usersFile,
                         const std::string& walletsFile,
                         const std::string& transactionsFile,
                         const std::string& ordersFile)
: usersFile(usersFile), walletsFile(walletsFile), transactionsFile(transactionsFile), ordersFile(ordersFile)
{
    loadUsers();
}

void UserManager::ensureFiles()
{
    // Create files with headers if they don't exist
    {
        std::ifstream in(usersFile);
        if (!in.good())
        {
            std::ofstream out(usersFile);
            out << "username,fullName,email,passwordHash\n";
        }
    }
    {
        std::ifstream in(walletsFile);
        if (!in.good())
        {
            std::ofstream out(walletsFile);
            out << "username,currency,amount\n";
        }
    }
    {
        std::ifstream in(transactionsFile);
        if (!in.good())
        {
            std::ofstream out(transactionsFile);
            out << "timestamp,username,type,product,price,amount,note\n";
        }
    }
    {
        std::ifstream in(ordersFile);
        if (!in.good())
        {
            std::ofstream out(ordersFile);
            out << "timestamp,username,product,orderType,price,amount\n";
        }
    }
}

void UserManager::loadUsers()
{
    ensureFiles();
    users.clear();
    std::ifstream in(usersFile);
    std::string line;
    // header
    std::getline(in, line);
    while (std::getline(in, line))
    {
        if (line.empty()) continue;
        auto cols = splitCSVLine(line);
        if (cols.size() < 4) continue;
        UserRecord u;
        u.username = cols[0];
        u.fullName = cols[1];
        u.email = cols[2];
        try { u.passwordHash = static_cast<size_t>(std::stoull(cols[3])); } catch (...) { u.passwordHash = 0; }
        users[u.username] = u;
    }
}

void UserManager::saveUsers()
{
    std::ofstream out(usersFile);
    out << "username,fullName,email,passwordHash\n";
    for (auto const& kv : users)
    {
        auto const& u = kv.second;
        out << csvEscape(u.username) << "," << csvEscape(u.fullName) << "," << csvEscape(u.email) << "," << u.passwordHash << "\n";
    }
}

std::string UserManager::generateUsername()
{
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<long long> dist(0, 9999999999LL);
    for (int tries = 0; tries < 50; ++tries)
    {
        long long n = dist(rng);
        std::ostringstream ss;
        ss << std::setw(10) << std::setfill('0') << n;
        std::string uname = ss.str();
        if (users.find(uname) == users.end())
            return uname;
    }
    // fallback (extremely unlikely)
    return "0000000000";
}

bool UserManager::registerUser(const std::string& fullName, const std::string& email, const std::string& password, std::string& outUsername, std::string& outError)
{
    outError.clear();
    // Prevent duplicates by same fullName+email
    for (auto const& kv : users)
    {
        if (kv.second.fullName == fullName && kv.second.email == email)
        {
            outError = "User already registered with same full name and email.";
            return false;
        }
    }

    if (fullName.empty() || email.empty() || password.empty())
    {
        outError = "Full name, email, and password cannot be empty.";
        return false;
    }

    UserRecord u;
    u.username = generateUsername();
    u.fullName = fullName;
    u.email = email;
    u.passwordHash = std::hash<std::string>{}(password);

    users[u.username] = u;
    saveUsers();

    // Create a starter wallet linked to user
    Wallet w;
    w.insertCurrency("USDT", 10000.0);
    w.insertCurrency("BTC", 1.0);
    w.insertCurrency("ETH", 10.0);
    saveWallet(u.username, w);

    logTransaction(TimeUtils::nowTimestamp(), u.username, "REGISTER", "", 0.0, 0.0, "Account created");

    outUsername = u.username;
    return true;
}

std::optional<Session> UserManager::login(const std::string& username, const std::string& password, std::string& outError)
{
    outError.clear();
    auto it = users.find(username);
    if (it == users.end())
    {
        outError = "Username not found.";
        return std::nullopt;
    }
    size_t ph = std::hash<std::string>{}(password);
    if (ph != it->second.passwordHash)
    {
        outError = "Invalid password.";
        return std::nullopt;
    }
    Session s;
    s.user = it->second;
    loadWallet(username, s.wallet);
    logTransaction(TimeUtils::nowTimestamp(), username, "LOGIN", "", 0.0, 0.0, "Login success");
    return s;
}

bool UserManager::resetPassword(const std::string& username, const std::string& email, const std::string& newPassword, std::string& outError)
{
    outError.clear();
    auto it = users.find(username);
    if (it == users.end())
    {
        outError = "Username not found.";
        return false;
    }
    if (it->second.email != email)
    {
        outError = "Email does not match our records.";
        return false;
    }
    if (newPassword.empty())
    {
        outError = "New password cannot be empty.";
        return false;
    }
    it->second.passwordHash = std::hash<std::string>{}(newPassword);
    saveUsers();
    logTransaction(TimeUtils::nowTimestamp(), username, "RESET_PASSWORD", "", 0.0, 0.0, "Password reset");
    return true;
}

void UserManager::loadWallet(const std::string& username, Wallet& wallet)
{
    wallet.clear();
    std::ifstream in(walletsFile);
    std::string line;
    std::getline(in, line); // header
    bool found = false;
    while (std::getline(in, line))
    {
        if (line.empty()) continue;
        auto cols = splitCSVLine(line);
        if (cols.size() < 3) continue;
        if (cols[0] != username) continue;
        found = true;
        try {
            wallet.insertCurrency(cols[1], std::stod(cols[2]));
        } catch (...) {
            // ignore
        }
    }
    if (!found)
    {
        // Ensure wallet exists (starter balances)
        wallet.insertCurrency("USDT", 10000.0);
        wallet.insertCurrency("BTC", 1.0);
        wallet.insertCurrency("ETH", 10.0);
        saveWallet(username, wallet);
    }
}

void UserManager::saveWallet(const std::string& username, const Wallet& wallet)
{
    // Read all existing rows, replace for this user
    std::ifstream in(walletsFile);
    std::vector<std::string> rows;
    std::string line;
    if (in.good())
    {
        while (std::getline(in, line))
        {
            if (line.empty()) continue;
            rows.push_back(line);
        }
    }
    if (rows.empty())
    {
        rows.push_back("username,currency,amount");
    }

    std::vector<std::string> outRows;
    outRows.push_back(rows[0]);
    // keep others except this user
    for (size_t i = 1; i < rows.size(); ++i)
    {
        auto cols = splitCSVLine(rows[i]);
        if (cols.size() >= 1 && cols[0] == username)
            continue;
        outRows.push_back(rows[i]);
    }

    // append new balances
    auto balances = wallet.getBalances();
    for (auto const& kv : balances)
    {
        outRows.push_back(csvEscape(username) + "," + csvEscape(kv.first) + "," + std::to_string(kv.second));
    }

    std::ofstream out(walletsFile);
    for (auto const& r : outRows)
        out << r << "\n";
}

void UserManager::logTransaction(const std::string& timestamp,
                                const std::string& username,
                                const std::string& type,
                                const std::string& product,
                                double price,
                                double amount,
                                const std::string& note)
{
    ensureFiles();
    std::ofstream out(transactionsFile, std::ios::app);
    out << csvEscape(timestamp) << "," << csvEscape(username) << "," << csvEscape(type) << "," << csvEscape(product)
        << "," << price << "," << amount << "," << csvEscape(note) << "\n";
}

void UserManager::logOrder(const OrderBookEntry& order)
{
    ensureFiles();
    std::ofstream out(ordersFile, std::ios::app);
    std::string typeStr = "unknown";
    if (order.orderType == OrderBookType::ask) typeStr = "ask";
    else if (order.orderType == OrderBookType::bid) typeStr = "bid";
    else if (order.orderType == OrderBookType::asksale) typeStr = "asksale";
    else if (order.orderType == OrderBookType::bidsale) typeStr = "bidsale";

    out << csvEscape(order.timestamp) << "," << csvEscape(order.username) << "," << csvEscape(order.product) << "," << typeStr
        << "," << order.price << "," << order.amount << "\n";
}

std::vector<std::string> UserManager::getRecentTransactions(const std::string& username, int n, const std::string& productFilter)
{
    std::vector<std::string> lines;
    std::ifstream in(transactionsFile);
    std::string line;
    std::getline(in, line); // header
    while (std::getline(in, line))
    {
        if (line.empty()) continue;
        auto cols = splitCSVLine(line);
        if (cols.size() < 7) continue;
        if (cols[1] != username) continue;
        if (!productFilter.empty() && cols[3] != productFilter) continue;
        lines.push_back(line);
    }
    // take last n
    std::vector<std::string> out;
    int start = (int)lines.size() - n;
    if (start < 0) start = 0;
    for (int i = start; i < (int)lines.size(); ++i)
        out.push_back(lines[i]);
    return out;
}

int UserManager::countOrders(const std::string& username, const std::string& typeFilter, const std::string& productFilter)
{
    int count = 0;
    std::ifstream in(ordersFile);
    std::string line;
    std::getline(in, line); // header
    while (std::getline(in, line))
    {
        if (line.empty()) continue;
        auto cols = splitCSVLine(line);
        if (cols.size() < 6) continue;
        if (cols[1] != username) continue;
        if (!typeFilter.empty() && cols[3] != typeFilter) continue;
        if (!productFilter.empty() && cols[2] != productFilter) continue;
        ++count;
    }
    return count;
}

double UserManager::totalSpent(const std::string& username, const std::string& startTs, const std::string& endTs)
{
    // Definition used here (and documented in report):
    // "spent" = sum(price*amount) for BID orders placed by the user in the given timeframe
    //            + sum(withdrawal amounts) (from transaction log)
    // This gives an intuitive measure of outflow.
    double total = 0.0;

    // From order log
    {
        std::ifstream in(ordersFile);
        std::string line;
        std::getline(in, line); // header
        while (std::getline(in, line))
        {
            if (line.empty()) continue;
            auto cols = splitCSVLine(line);
            if (cols.size() < 6) continue;
            // cols: ts, username, product, orderType, price, amount
            if (cols[1] != username) continue;
            if (!TimeUtils::inRange(cols[0], startTs, endTs)) continue;
            if (cols[3] == "bid")
            {
                try {
                    double price = std::stod(cols[4]);
                    double amt = std::stod(cols[5]);
                    total += price * amt;
                } catch (...) {}
            }
        }
    }

    // From withdrawals
    {
        std::ifstream in(transactionsFile);
        std::string line;
        std::getline(in, line); // header
        while (std::getline(in, line))
        {
            if (line.empty()) continue;
            auto cols = splitCSVLine(line);
            if (cols.size() < 7) continue;
            if (cols[1] != username) continue;
            if (!TimeUtils::inRange(cols[0], startTs, endTs)) continue;
            if (cols[2] == "WITHDRAW")
            {
                try {
                    double amt = std::stod(cols[5]);
                    total += amt;
                } catch (...) {}
            }
        }
    }

    return total;
}

std::string UserManager::csvEscape(const std::string& s)
{
    // Minimal CSV escaping (handles commas and quotes)
    bool needQuotes = false;
    for (char c : s)
    {
        if (c == ',' || c == '"' || c == '\n' || c == '\r')
        {
            needQuotes = true;
            break;
        }
    }
    if (!needQuotes) return s;

    std::string out = "\"";
    for (char c : s)
    {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += "\"";
    return out;
}

std::vector<std::string> UserManager::splitCSVLine(const std::string& line)
{
    std::vector<std::string> cols;
    std::string cur;
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); ++i)
    {
        char c = line[i];
        if (inQuotes)
        {
            if (c == '"')
            {
                if (i + 1 < line.size() && line[i + 1] == '"')
                {
                    cur.push_back('"');
                    ++i;
                }
                else
                {
                    inQuotes = false;
                }
            }
            else
            {
                cur.push_back(c);
            }
        }
        else
        {
            if (c == ',')
            {
                cols.push_back(cur);
                cur.clear();
            }
            else if (c == '"')
            {
                inQuotes = true;
            }
            else
            {
                cur.push_back(c);
            }
        }
    }
    cols.push_back(cur);
    return cols;
}
