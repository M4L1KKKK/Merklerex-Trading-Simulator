#include "MerkelMain.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <random>

#include "OrderBookEntry.h"
#include "CSVReader.h"
#include "Candlestick.h"
#include "TimeUtils.h"

MerkelMain::MerkelMain()
{
}

void MerkelMain::init()
{
    userMgr.ensureFiles();

    // Require login/register first
    authFlow();

    currentTime = orderBook.getEarliestTime();

    // Load wallet from persistence for logged-in user
    if (isLoggedIn)
    {
        userMgr.loadWallet(loggedInUsername, wallet);
    }

    while (true)
    {
        if (!isLoggedIn)
        {
            authFlow();
            // After successful login, reload wallet
            if (isLoggedIn)
            {
                userMgr.loadWallet(loggedInUsername, wallet);
            }
            continue;
        }

        printMenu();
        int input = getUserOption();
        processUserOption(input);
    }
}


// Authentication Flow


void MerkelMain::printAuthMenu()
{
    std::cout << "\n============== AUTH ==============\n";
    std::cout << "1: Register\n";
    std::cout << "2: Login\n";
    std::cout << "3: Reset Password\n";
    std::cout << "4: Exit\n";
    std::cout << "==================================\n";
}

std::string MerkelMain::promptLine(const std::string& prompt)
{
    std::cout << prompt;
    std::string s;
    std::getline(std::cin, s);
    return s;
}

int MerkelMain::getMenuOption(const std::string& prompt, int min, int max)
{
    while (true)
    {
        std::string line = promptLine(prompt);
        try
        {
            int v = std::stoi(line);
            if (v >= min && v <= max)
                return v;
        }
        catch (...) {}
        std::cout << "Invalid input. Please enter a number between " << min << " and " << max << ".\n";
    }
}

void MerkelMain::authFlow()
{
    while (!isLoggedIn)
    {
        printAuthMenu();
        int choice = getMenuOption("Choose 1-4: ", 1, 4);
        if (choice == 1) handleRegister();
        if (choice == 2) handleLogin();
        if (choice == 3) handleResetPassword();
        if (choice == 4)
        {
            std::cout << "Exiting...\n";
            exit(0);
        }
    }
}

void MerkelMain::handleRegister()
{
    std::string fullName = promptLine("Full name: ");
    std::string email = promptLine("Email: ");
    std::string password = promptLine("Password: ");

    std::string username;
    std::string err;
    if (userMgr.registerUser(fullName, email, password, username, err))
    {
        std::cout << "Registered successfully! Your 10-digit username is: " << username << "\n";
        std::cout << "Now login using your username.\n";
    }
    else
    {
        std::cout << "Registration failed: " << err << "\n";
    }
}

void MerkelMain::handleLogin()
{
    std::string username = promptLine("Username (10 digits): ");
    std::string password = promptLine("Password: ");

    std::string err;
    auto session = userMgr.login(username, password, err);
    if (!session.has_value())
    {
        std::cout << "Login failed: " << err << "\n";
        return;
    }

    isLoggedIn = true;
    loggedInUsername = username;
    wallet = session->wallet;

    std::cout << "Login successful. Welcome!\n";
}

void MerkelMain::handleResetPassword()
{
    std::string username = promptLine("Username: ");
    std::string email = promptLine("Email used at registration: ");
    std::string newPassword = promptLine("New password: ");

    std::string err;
    if (userMgr.resetPassword(username, email, newPassword, err))
    {
        std::cout << "Password updated. You can now login.\n";
    }
    else
    {
        std::cout << "Reset failed: " << err << "\n";
    }
}

void MerkelMain::logout()
{
    if (isLoggedIn)
    {
        userMgr.saveWallet(loggedInUsername, wallet);
        userMgr.logTransaction(TimeUtils::nowTimestamp(), loggedInUsername, "LOGOUT", "", 0.0, 0.0, "User logged out");
    }
    isLoggedIn = false;
    loggedInUsername.clear();
    wallet.clear();
    std::cout << "Logged out.\n";
}


// Main Menu


void MerkelMain::printMenu()
{
    std::cout << "\n============== MENU ==============\n";
    std::cout << "1: Print help\n";
    std::cout << "2: Candlestick summary (asks & bids)\n";
    std::cout << "3: Print exchange stats (current timestamp)\n";
    std::cout << "4: Make an ask (sell)\n";
    std::cout << "5: Make a bid (buy)\n";
    std::cout << "6: Wallet & transactions\n";
    std::cout << "7: Simulate NEW trading activity (5 asks + 5 bids per product)\n";
    std::cout << "8: Continue to next timeframe (match orders)\n";
    std::cout << "9: Logout\n";
    std::cout << "==================================\n";
    std::cout << "Logged in as: " << loggedInUsername << "\n";
    std::cout << "Current time is: " << currentTime << "\n";
}

void MerkelMain::printHelp()
{
    std::cout << "Help - analyse the market, place bids/asks, and track your wallet + transaction history.\n";
    std::cout << "Tip: Use candlesticks to spot trends across day/month/year buckets.\n";
}

void MerkelMain::printMarketStats()
{
    for (std::string const& p : orderBook.getKnownProducts())
    {
        std::cout << "Product: " << p << "\n";
        std::vector<OrderBookEntry> asks = orderBook.getOrders(OrderBookType::ask, p, currentTime);
        std::vector<OrderBookEntry> bids = orderBook.getOrders(OrderBookType::bid, p, currentTime);

        std::cout << "  Asks seen: " << asks.size();
        if (!asks.empty())
        {
            std::cout << " | Min ask: " << OrderBook::getLowPrice(asks) << " | Max ask: " << OrderBook::getHighPrice(asks);
        }
        std::cout << "\n";

        std::cout << "  Bids seen: " << bids.size();
        if (!bids.empty())
        {
            std::cout << " | Min bid: " << OrderBook::getLowPrice(bids) << " | Max bid: " << OrderBook::getHighPrice(bids);
        }
        std::cout << "\n";
    }
}


// Candlesticks


static std::vector<Candlestick> buildCandles(const std::vector<OrderBookEntry>& orders,
                                            char granularity,
                                            const std::string& startTs,
                                            const std::string& endTs)
{
    // orders must be sorted by timestamp
    std::vector<OrderBookEntry> sorted = orders;
    std::sort(sorted.begin(), sorted.end(), OrderBookEntry::compareByTimestamp);

    std::vector<Candlestick> candles;
    std::string curBucket;

    bool firstInBucket = true;
    double open = 0, close = 0, high = 0, low = 0;
    int trades = 0;
    std::string bucketStartTs;
    std::string bucketEndTs;

    for (auto const& e : sorted)
    {
        if (!TimeUtils::inRange(e.timestamp, startTs, endTs)) continue;

        std::string bucket = TimeUtils::bucketLabel(e.timestamp, granularity);
        if (curBucket.empty())
        {
            curBucket = bucket;
        }

        if (bucket != curBucket)
        {
            // flush previous
            candles.emplace_back(curBucket, bucketStartTs, bucketEndTs, open, close, high, low, trades);

            // reset
            curBucket = bucket;
            firstInBucket = true;
            trades = 0;
        }

        if (firstInBucket)
        {
            open = e.price;
            high = e.price;
            low = e.price;
            firstInBucket = false;
            bucketStartTs = e.timestamp;
        }

        // update running
        close = e.price;
        if (e.price > high) high = e.price;
        if (e.price < low) low = e.price;
        trades++;
        bucketEndTs = e.timestamp;
    }

    if (!curBucket.empty() && trades > 0)
    {
        candles.emplace_back(curBucket, bucketStartTs, bucketEndTs, open, close, high, low, trades);
    }

    return candles;
}

void MerkelMain::printCandlestickMenu()
{
    std::cout << "\nCandlestick summary\n";
    std::cout << "Enter product (example ETH/BTC). Known products:\n";
    for (auto const& p : orderBook.getKnownProducts())
        std::cout << "  - " << p << "\n";

    std::string product = promptLine("Product: ");

    std::cout << "Granularity: 1=Daily, 2=Monthly, 3=Yearly (default)\n";
    int g = 3;
    {
        std::string gline = promptLine("Choose 1-3 (press Enter for default=3): ");
        if (!gline.empty())
        {
            try { g = std::stoi(gline); } catch (...) { g = 3; }
            if (g < 1 || g > 3) g = 3;
        }
    }
    char gran = (g == 1 ? 'D' : (g == 2 ? 'M' : 'Y'));

    std::cout << "Optional date range. Leave empty for full range.\n";
    std::string startTs = promptLine("Start timestamp (YYYY/MM/DD HH:MM:SS.micro) or empty: ");
    std::string endTs = promptLine("End timestamp (YYYY/MM/DD HH:MM:SS.micro) or empty: ");

    // ASKS
    auto asksAll = orderBook.getOrders(OrderBookType::ask, product);
    auto bidsAll = orderBook.getOrders(OrderBookType::bid, product);

    auto askCandles = buildCandles(asksAll, gran, startTs, endTs);
    auto bidCandles = buildCandles(bidsAll, gran, startTs, endTs);

    auto printTable = [](const std::string& title, const std::vector<Candlestick>& candles)
    {
        std::cout << "\n--- " << title << " ---\n";
        if (candles.empty())
        {
            std::cout << "No data for this selection.\n";
            return;
        }
        std::cout << "Bucket\tOpen\tClose\tHigh\tLow\tTrades\tStart\tEnd\n";
        for (auto const& c : candles)
        {
            std::cout << c.bucketLabel << "\t" << c.open << "\t" << c.close << "\t" << c.high << "\t" << c.low
                      << "\t" << c.trades << "\t" << c.startTimestamp << "\t" << c.endTimestamp << "\n";
        }
    };

    printTable("ASK candlesticks", askCandles);
    printTable("BID candlesticks", bidCandles);
}

// Order placement


void MerkelMain::enterAsk()
{
    std::cout << "Make an ask - enter: product,price,amount (eg ETH/BTC,200,0.5)\n";
    std::string input;
    std::getline(std::cin, input);

    std::vector<std::string> tokens = CSVReader::tokenise(input, ',');
    if (tokens.size() != 3)
    {
        std::cout << "Bad input! " << input << "\n";
        return;
    }

    try
    {
        OrderBookEntry obe = CSVReader::stringsToOBE(
            tokens[1],
            tokens[2],
            currentTime,
            tokens[0],
            OrderBookType::ask);

        obe.username = loggedInUsername;

        if (wallet.canFulfillOrder(obe))
        {
            orderBook.insertOrder(obe);
            userMgr.logOrder(obe);
            userMgr.logTransaction(TimeUtils::nowTimestamp(), loggedInUsername, "PLACE_ORDER", obe.product, obe.price, obe.amount, "ASK placed");
            std::cout << "Ask added.\n";
        }
        else
        {
            std::cout << "Wallet has insufficient funds.\n";
        }
    }
    catch (...)
    {
        std::cout << "Bad input (price/amount).\n";
    }
}

void MerkelMain::enterBid()
{
    std::cout << "Make a bid - enter: product,price,amount (eg ETH/BTC,200,0.5)\n";
    std::string input;
    std::getline(std::cin, input);

    std::vector<std::string> tokens = CSVReader::tokenise(input, ',');
    if (tokens.size() != 3)
    {
        std::cout << "Bad input! " << input << "\n";
        return;
    }

    try
    {
        OrderBookEntry obe = CSVReader::stringsToOBE(
            tokens[1],
            tokens[2],
            currentTime,
            tokens[0],
            OrderBookType::bid);

        obe.username = loggedInUsername;

        if (wallet.canFulfillOrder(obe))
        {
            orderBook.insertOrder(obe);
            userMgr.logOrder(obe);
            userMgr.logTransaction(TimeUtils::nowTimestamp(), loggedInUsername, "PLACE_ORDER", obe.product, obe.price, obe.amount, "BID placed");
            std::cout << "Bid added.\n";
        }
        else
        {
            std::cout << "Wallet has insufficient funds.\n";
        }
    }
    catch (...)
    {
        std::cout << "Bad input (price/amount).\n";
    }
}


// Wallet + transactions


void MerkelMain::printWallet()
{
    std::cout << "\nWallet balances:\n";
    std::cout << wallet.toString() << "\n";
}

void MerkelMain::deposit()
{
    std::string cur = promptLine("Currency (e.g., USDT): ");
    std::string amtStr = promptLine("Amount: ");
    try
    {
        double amt = std::stod(amtStr);
        wallet.insertCurrency(cur, amt);
        userMgr.saveWallet(loggedInUsername, wallet);
        userMgr.logTransaction(TimeUtils::nowTimestamp(), loggedInUsername, "DEPOSIT", cur, 0.0, amt, "Deposit");
        std::cout << "Deposit successful.\n";
    }
    catch (...)
    {
        std::cout << "Invalid amount.\n";
    }
}

void MerkelMain::withdraw()
{
    std::string cur = promptLine("Currency (e.g., USDT): ");
    std::string amtStr = promptLine("Amount: ");
    try
    {
        double amt = std::stod(amtStr);
        if (wallet.removeCurrency(cur, amt))
        {
            userMgr.saveWallet(loggedInUsername, wallet);
            userMgr.logTransaction(TimeUtils::nowTimestamp(), loggedInUsername, "WITHDRAW", cur, 0.0, amt, "Withdraw");
            std::cout << "Withdrawal successful.\n";
        }
        else
        {
            std::cout << "Withdrawal failed (insufficient funds or unknown currency).\n";
        }
    }
    catch (...)
    {
        std::cout << "Invalid amount.\n";
    }
}

void MerkelMain::printRecentTransactions()
{
    std::cout << "1: Last 5 transactions\n";
    std::cout << "2: Last 5 transactions filtered by product/currency\n";
    int c = getMenuOption("Choose 1-2: ", 1, 2);
    std::string filter;
    if (c == 2)
    {
        filter = promptLine("Enter product (e.g., ETH/BTC) OR currency (e.g., USDT): ");
    }
    auto lines = userMgr.getRecentTransactions(loggedInUsername, 5, filter);
    if (lines.empty())
    {
        std::cout << "No transactions found.\n";
        return;
    }
    std::cout << "timestamp,username,type,product,price,amount,note\n";
    for (auto const& l : lines)
        std::cout << l << "\n";
}

void MerkelMain::printUserOrderStats()
{
    std::cout << "Order stats (from orders.csv)\n";
    std::string product = promptLine("Product filter (empty for all): ");

    int asks = userMgr.countOrders(loggedInUsername, "ask", product);
    int bids = userMgr.countOrders(loggedInUsername, "bid", product);

    std::cout << "Asks placed: " << asks << "\n";
    std::cout << "Bids placed: " << bids << "\n";
}

void MerkelMain::printTotalSpent()
{
    std::cout << "Total spent = sum(price*amount) of your BID orders + withdrawals within timeframe.\n";
    std::string startTs = promptLine("Start timestamp (or empty): ");
    std::string endTs = promptLine("End timestamp (or empty): ");

    double total = userMgr.totalSpent(loggedInUsername, startTs, endTs);
    std::cout << "Total spent: " << total << "\n";
}

void MerkelMain::walletMenu()
{
    while (true)
    {
        std::cout << "\n=========== WALLET MENU ===========\n";
        std::cout << "1: View wallet\n";
        std::cout << "2: Deposit\n";
        std::cout << "3: Withdraw\n";
        std::cout << "4: View recent transactions\n";
        std::cout << "5: Order stats (asks/bids counts)\n";
        std::cout << "6: Total spent within timeframe\n";
        std::cout << "7: Back\n";
        std::cout << "==================================\n";

        int c = getMenuOption("Choose 1-7: ", 1, 7);
        if (c == 1) printWallet();
        if (c == 2) deposit();
        if (c == 3) withdraw();
        if (c == 4) printRecentTransactions();
        if (c == 5) printUserOrderStats();
        if (c == 6) printTotalSpent();
        if (c == 7) return;
    }
}


// Simulate activity


void MerkelMain::simulateTradingActivity()
{
    // 5 new asks and 5 new bids for ALL products.
    // We use the current system timestamp to represent "new" activity beyond the historical dataset.
    std::string ts = TimeUtils::nowTimestamp();

    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> smallAmt(0.1, 2.0);

    int totalCreated = 0;

    for (auto const& product : orderBook.getKnownProducts())
    {
        // Use currentTime snapshot for pricing logic
        std::vector<OrderBookEntry> asksNow = orderBook.getOrders(OrderBookType::ask, product, currentTime);
        std::vector<OrderBookEntry> bidsNow = orderBook.getOrders(OrderBookType::bid, product, currentTime);

        double refAsk = !asksNow.empty() ? OrderBook::getLowPrice(asksNow) : 1.0;
        double refBid = !bidsNow.empty() ? OrderBook::getHighPrice(bidsNow) : 1.0;

        // Simple pricing justification:
        // - Ask prices slightly above the current lowest ask
        // - Bid prices slightly below the current highest bid
        for (int i = 0; i < 5; ++i)
        {
            // ASK
            {
                double price = refAsk * (1.0 + 0.001 * (i + 1));
                double amt = smallAmt(rng);
                OrderBookEntry ask{price, amt, ts, product, OrderBookType::ask};
                ask.username = loggedInUsername;
                if (wallet.canFulfillOrder(ask))
                {
                    orderBook.insertOrder(ask);
                    userMgr.logOrder(ask);
                    userMgr.logTransaction(ts, loggedInUsername, "SIM_ORDER", product, price, amt, "Simulated ASK");
                    totalCreated++;
                }
            }

            // BID
            {
                double price = refBid * (1.0 - 0.001 * (i + 1));
                double amt = smallAmt(rng);
                OrderBookEntry bid{price, amt, ts, product, OrderBookType::bid};
                bid.username = loggedInUsername;
                if (wallet.canFulfillOrder(bid))
                {
                    orderBook.insertOrder(bid);
                    userMgr.logOrder(bid);
                    userMgr.logTransaction(ts, loggedInUsername, "SIM_ORDER", product, price, amt, "Simulated BID");
                    totalCreated++;
                }
            }
        }
    }

    std::cout << "Simulated orders created (if wallet allowed): " << totalCreated << "\n";
    userMgr.saveWallet(loggedInUsername, wallet);
}


// Timeframe step & matching


void MerkelMain::gotoNextTimeframe()
{
    std::cout << "Going to next time frame...\n";
    for (std::string p : orderBook.getKnownProducts())
    {
        std::vector<OrderBookEntry> sales = orderBook.matchAsksToBids(p, currentTime);
        for (OrderBookEntry& sale : sales)
        {
            if (sale.username == loggedInUsername)
            {
                wallet.processSale(sale);
                userMgr.logTransaction(TimeUtils::nowTimestamp(), loggedInUsername, "SALE", sale.product, sale.price, sale.amount, "Matched sale");
            }
        }
    }

    userMgr.saveWallet(loggedInUsername, wallet);

    currentTime = orderBook.getNextTime(currentTime);
}


// Input + dispatch


int MerkelMain::getUserOption()
{
    return getMenuOption("Choose 1-9: ", 1, 9);
}

void MerkelMain::processUserOption(int userOption)
{
    if (userOption == 1) printHelp();
    if (userOption == 2) printCandlestickMenu();
    if (userOption == 3) printMarketStats();
    if (userOption == 4) enterAsk();
    if (userOption == 5) enterBid();
    if (userOption == 6) walletMenu();
    if (userOption == 7) simulateTradingActivity();
    if (userOption == 8) gotoNextTimeframe();
    if (userOption == 9) logout();
}
