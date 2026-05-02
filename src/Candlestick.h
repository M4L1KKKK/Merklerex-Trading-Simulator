#pragma once

#include <string>

class Candlestick
{
public:
    Candlestick();
    Candlestick(const std::string& bucketLabel,
                const std::string& startTimestamp,
                const std::string& endTimestamp,
                double open,
                double close,
                double high,
                double low,
                int trades);

    std::string bucketLabel;
    std::string startTimestamp;
    std::string endTimestamp;

    double open = 0.0;
    double close = 0.0;
    double high = 0.0;
    double low = 0.0;

    int trades = 0;
};
