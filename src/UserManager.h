#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <vector>
#include "Wallet.h"
#include "OrderBookEntry.h"

struct UserRecord
{
    std::string username; // 10 digits
    std::string fullName;
    std::string email;
    size_t passwordHash = 0;
};

struct Session
{
    UserRecord user;
    Wallet wallet;
};

class UserManager
{
public:
    UserManager(const std::string& usersFile,
                const std::string& walletsFile,
                const std::string& transactionsFile,
                const std::string& ordersFile);

    void ensureFiles();

    bool registerUser(const std::string& fullName, const std::string& email, const std::string& password, std::string& outUsername, std::string& outError);

    std::optional<Session> login(const std::string& username, const std::string& password, std::string& outError);

    bool resetPassword(const std::string& username, const std::string& email, const std::string& newPassword, std::string& outError);

    // Wallet persistence
    void loadWallet(const std::string& username, Wallet& wallet);
    void saveWallet(const std::string& username, const Wallet& wallet);

    // Transaction log: timestamp,username,type,product,price,amount,note
    void logTransaction(const std::string& timestamp,
                        const std::string& username,
                        const std::string& type,
                        const std::string& product,
                        double price,
                        double amount,
                        const std::string& note);

    // Order log: timestamp,username,product,orderType,price,amount
    void logOrder(const OrderBookEntry& order);

    // Reads last n transactions for user (optionally filter by product)
    std::vector<std::string> getRecentTransactions(const std::string& username, int n, const std::string& productFilter = "");

    // Stats from order logs: counts of asks/bids
    int countOrders(const std::string& username, const std::string& typeFilter, const std::string& productFilter = "");

    // Total money spent in timeframe (based on bids and withdrawals/deposits?)
    double totalSpent(const std::string& username, const std::string& startTs, const std::string& endTs);

private:
    std::string usersFile;
    std::string walletsFile;
    std::string transactionsFile;
    std::string ordersFile;

    std::unordered_map<std::string, UserRecord> users;

    void loadUsers();
    void saveUsers();

    std::string generateUsername();
    static std::string csvEscape(const std::string& s);
    static std::vector<std::string> splitCSVLine(const std::string& line);
};
