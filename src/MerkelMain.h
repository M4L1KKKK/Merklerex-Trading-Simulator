#pragma once

#include <vector>
#include "OrderBookEntry.h"
#include "OrderBook.h"
#include "Wallet.h"
#include "UserManager.h"


class MerkelMain
{
    public:
        MerkelMain();
        /** Call this to start the sim */
        void init();
    private: 
        // Authentication / session
        void authFlow();
        void printAuthMenu();
        int getMenuOption(const std::string& prompt, int min, int max);
        std::string promptLine(const std::string& prompt);

        void handleRegister();
        void handleLogin();
        void handleResetPassword();
        void logout();

        void printMenu();
        void printHelp();
        void printMarketStats();
        void printCandlestickMenu();
        void enterAsk();
        void enterBid();
        void printWallet();
        void walletMenu();
        void deposit();
        void withdraw();
        void printRecentTransactions();
        void printUserOrderStats();
        void printTotalSpent();
        void simulateTradingActivity();
        void gotoNextTimeframe();
        int getUserOption();
        void processUserOption(int userOption);

        std::string currentTime;

        bool isLoggedIn = false;
        std::string loggedInUsername;

//        OrderBook orderBook{"20200317.csv"};
	OrderBook orderBook{"20200601.csv"};
        Wallet wallet;

        UserManager userMgr{"users.csv", "wallets.csv", "transactions.csv", "orders.csv"};

};
