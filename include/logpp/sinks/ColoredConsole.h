#include "logpp/sinks/FormatSink.h"

#include "logpp/format/PatternFormatter.h"
#include "logpp/format/flag/LevelFormatter.h"

#include <cstdio>

namespace logpp::sink
{
    class ColoredConsole : public FormatSink
    {
    public:
        explicit ColoredConsole(FILE* fp)
            : ColoredConsole(fp, std::make_shared<PatternFormatter>("%+"))
        { }

        explicit ColoredConsole(FILE* fp, const std::shared_ptr<Formatter>& formatter)
            : FormatSink(formatter)
            , m_file(fp)
        {
            setColor(LogLevel::Trace, FgWhite);
            setColor(LogLevel::Debug, FgGreen);
            setColor(LogLevel::Info, FgCyan);
            setColor(LogLevel::Warning, FgYellow);
            setColor(LogLevel::Error, FgRed);

            configureFormatter(formatter);
        }

        void setColor(LogLevel level, std::string_view code)
        {
            m_colors[static_cast<size_t>(level)] = code;
        }

        std::string_view getColor(LogLevel level) const
        {
            return m_colors[static_cast<size_t>(level)];
        }

        void sink(std::string_view name, LogLevel level, const EventLogBuffer& buffer)
        {
            fmt::memory_buffer formatBuf;
            format(name, level, buffer, formatBuf);

            ::fwrite(formatBuf.data(), 1, formatBuf.size(), m_file);
            ::fputc('\n', m_file);
        }
    private:
        FILE *m_file;

        std::array<std::string_view, 5> m_colors;

        static constexpr std::string_view Bold = "\033[1m";
        static constexpr std::string_view Reset = "\033[m";

        static constexpr std::string_view FgBlack = "\033[30m";
        static constexpr std::string_view FgRed = "\033[31m";
        static constexpr std::string_view FgGreen = "\033[32m";
        static constexpr std::string_view FgYellow = "\033[33m";
        static constexpr std::string_view FgBlue = "\033[34m";
        static constexpr std::string_view FgMagenta = "\033[35m";
        static constexpr std::string_view FgCyan = "\033[36m";
        static constexpr std::string_view FgWhite = "\033[37m";

        void configureFormatter(const std::shared_ptr<Formatter>& formatter)
        {
            formatter->setPreFormat([=](std::string_view, LogLevel level, const EventLogBuffer&, fmt::memory_buffer& out) {
                auto color = getColor(level);
                fmt::format_to(out, "{}", color);
            });
            formatter->setPostFormat([=](std::string_view, LogLevel, const EventLogBuffer&, fmt::memory_buffer& out) {
                out.append(Reset.data(), Reset.data() + Reset.size());
            });
        }
    };

    class ColoredOutputConsole : public ColoredConsole
    {
    public:
        static constexpr std::string_view Name = "ColoredOutputConsole";

        ColoredOutputConsole()
            : ColoredConsole(stdout)
        {}

        explicit ColoredOutputConsole(std::shared_ptr<Formatter> formatter)
            : ColoredConsole(stdout, std::move(formatter))
        {}
    };

    class ColoredErrorConsole : public ColoredConsole
    {
    public:
        static constexpr std::string_view Name = "ColoredErrorConsole";

        ColoredErrorConsole()
            : ColoredConsole(stderr)
        {}

        ColoredErrorConsole(std::shared_ptr<Formatter> formatter)
            : ColoredConsole(stderr, std::move(formatter))
        {}
    };
}