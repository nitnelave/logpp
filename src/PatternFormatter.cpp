#include "logpp/format/PatternFormatter.h"

#include "logpp/format/flag/DateFormatter.h"
#include "logpp/format/flag/FullFormatter.h"
#include "logpp/format/flag/LevelFormatter.h"
#include "logpp/format/flag/LiteralFormatter.h"
#include "logpp/format/flag/NameFormatter.h"
#include "logpp/format/flag/TextFormatter.h"
#include "logpp/format/flag/TimeFormatter.h"

namespace logpp
{
    PatternFormatter::PatternFormatter()
        : PatternFormatter("%+")
    {}

    PatternFormatter::PatternFormatter(std::string pattern)
        : m_pattern(std::move(pattern))
    {
        parsePattern(m_pattern);
    }

    void PatternFormatter::format(std::string_view name, LogLevel level, const EventLogBuffer& buffer, fmt::memory_buffer& out) const
    {
        auto time = buffer.time();
        auto cTime = Clock::to_time_t(time);
        auto tm = std::localtime(&cTime);

        for (const auto& formatter: m_formatters)
        {
            formatter->format(name, level, tm, buffer, out);
        }
    }

    void PatternFormatter::parsePattern(const std::string& pattern)
    {
        auto it = pattern.begin(), end = pattern.end();

        std::string literalStr;

        m_formatters.clear();
        while (it != end)
        {
            if (*it == '%')
            {
                if (!literalStr.empty())
                    m_formatters.push_back(std::make_unique<LiteralFormatter>(std::move(literalStr)));

                ++it;
                if (it != end)
                {
                    std::unique_ptr<FlagFormatter> formatter;
                    std::tie(it, formatter) = parseFlag(it);
                    m_formatters.push_back(std::move(formatter));
                }
            }
            else
            {
                literalStr.push_back(*it);
                ++it;
            }
        }

        if (!literalStr.empty())
            m_formatters.push_back(std::make_unique<LiteralFormatter>(std::move(literalStr)));
    }

    std::pair<std::string::const_iterator, std::unique_ptr<FlagFormatter>>
    PatternFormatter::parseFlag(std::string::const_iterator it)
    {
        std::unique_ptr<FlagFormatter> formatter;
        switch (*it)
        {
            // Full format
            case '+':
            {
                ++it;
                formatter = std::make_unique<FullFormatter>();
                break;
            }

            // -----------------------------
            // Date
            // -----------------------------

            // Writes year as a decimal number, e.g. 2017
            case 'Y':
            {
                ++it;
                formatter = std::make_unique<YearFormatter>();
                break;
            }
            // Writes month as a decimal number (range [01,12]) 
            case 'm':
            {
                ++it;
                formatter = std::make_unique<MonthDecimalFormatter>();
                break;
            }
            // writes day of the month as a decimal number (range [01,31])
            case 'd':
            {
                ++it;
                formatter = std::make_unique<DayDecimalFormatter>();
                break;
            }

            // -----------------------------
            // Time
            // -----------------------------

            // Writes hour as a decimal number, 24 hour clock (range [00-23])
            case 'H':
            {
                ++it;
                formatter = std::make_unique<HoursFormatter>();
                break;
            }
            // Writes minute as a decimal number (range [00,59])
            case 'M':
            {
                ++it;
                formatter = std::make_unique<MinutesFormatter>();
                break;
            }
            // Writes second as a decimal number (range [00,60])
            case 'S':
            {
                ++it;
                formatter = std::make_unique<SecondsFormatter>();
                break;
            }

            // Text
            case 'v':
            {
                ++it;
                formatter = std::make_unique<TextFormatter>();
                break;
            }
            // Level
            case 'l':
            {
                ++it;
                formatter = std::make_unique<LevelFormatter>();
                break;
            }
            // Logger name
            case 'n':
            {
                ++it;
                formatter = std::make_unique<NameFormatter>();
                break;
            }
        }


        return std::make_pair(it, std::move(formatter));
    }
}