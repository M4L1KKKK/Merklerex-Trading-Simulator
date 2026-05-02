#include "TimeUtils.h"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace TimeUtils
{
    bool parseTimestamp(const std::string& ts, std::tm& out, long& micro)
    {
        // Expected: YYYY/MM/DD HH:MM:SS.micro
        if (ts.size() < 19) return false;

        std::istringstream ss(ts);
        ss >> std::get_time(&out, "%Y/%m/%d %H:%M:%S");
        if (ss.fail()) return false;

        micro = 0;
        if (ss.peek() == '.')
        {
            ss.get();
            std::string micros;
            ss >> micros;
            // micros might include trailing spaces/newlines - but should be digits
            try {
                micro = std::stol(micros);
            } catch (...) {
                micro = 0;
            }
        }
        return true;
    }

    std::string nowTimestamp()
    {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto secs = time_point_cast<seconds>(now);
        auto micros = duration_cast<microseconds>(now - secs).count();

        std::time_t t = system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif

        std::ostringstream out;
        out << std::put_time(&tm, "%Y/%m/%d %H:%M:%S");
        out << "." << std::setw(6) << std::setfill('0') << micros;
        return out.str();
    }

    std::string bucketLabel(const std::string& ts, char granularity)
    {
        if (ts.size() < 10) return ts;
        if (granularity == 'Y')
        {
            return ts.substr(0,4);
        }
        if (granularity == 'M')
        {
            return ts.substr(0,7);
        }
        // 'D'
        return ts.substr(0,10);
    }

    bool inRange(const std::string& ts, const std::string& start, const std::string& end)
    {
        if (!start.empty() && ts < start) return false;
        if (!end.empty() && ts > end) return false;
        return true;
    }
}
