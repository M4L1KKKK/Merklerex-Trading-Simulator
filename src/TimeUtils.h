#pragma once

#include <string>
#include <ctime>

namespace TimeUtils
{
    /** Parse MerkelRex timestamp format: YYYY/MM/DD HH:MM:SS.micro */
    bool parseTimestamp(const std::string& ts, std::tm& out, long& micro);

    /** Format current system time to MerkelRex timestamp format */
    std::string nowTimestamp();

    /**
     * Return a bucket label for the timestamp based on granularity:
     *  'D' => YYYY/MM/DD
     *  'M' => YYYY/MM
     *  'Y' => YYYY
     */
    std::string bucketLabel(const std::string& ts, char granularity);

    /** Compare timestamps lexicographically (works with this format) */
    inline bool lessThan(const std::string& a, const std::string& b)
    {
        return a < b;
    }

    /**
     * Check if ts is within [start,end] (inclusive). Empty start/end => unbounded.
     */
    bool inRange(const std::string& ts, const std::string& start, const std::string& end);
}
