#include "Candlestick.h"

Candlestick::Candlestick() = default;

Candlestick::Candlestick(const std::string& bucketLabel,
                         const std::string& startTimestamp,
                         const std::string& endTimestamp,
                         double open,
                         double close,
                         double high,
                         double low,
                         int trades)
{
    this->bucketLabel = bucketLabel;
    this->startTimestamp = startTimestamp;
    this->endTimestamp = endTimestamp;
    this->open = open;
    this->close = close;
    this->high = high;
    this->low = low;
    this->trades = trades;
}
