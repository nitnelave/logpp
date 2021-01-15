#include "logpp/logpp.h"

#include "logpp/core/Logger.h"
#include "logpp/sinks/ColoredConsole.h"

enum class AccessRight
{
    Read,
    Write,
    All
};

std::string_view accessRightString(AccessRight access)
{
    using namespace std::string_view_literals;

    switch (access)
    {
        case AccessRight::Read:
            return "Read"sv;
        case AccessRight::Write:
            return "Write"sv;
        case AccessRight::All:
            return "All"sv;
    }

    return ""sv;
}

bool authorizeUser(const std::string& userName, const std::string& password)
{
    logpp::debug("Authorizing user",
        logpp::field("username", userName),
        logpp::field("password", password)
    );

    return true;
}

bool grantUser(const std::string& userName, AccessRight access)
{
    if (userName == "admin")
        logpp::warn("Attempting to grant access to admin user");

    logpp::debug(logpp::format("Granting access to user {}", userName),
        logpp::field("access_right", accessRightString(access))
    );

    return true;
}

int main(int argc, const char *argv[])
{
    // Set the global logger level to Debug
    logpp::setLevel(logpp::LogLevel::Debug);
    // Uncomment this line to log lines in logfmt format
    // logpp::setFormatter<logpp::LogFmtFormatter>();

    if (argc < 3)
    {
        logpp::error(logpp::format("Usage: {} [username] [password]", argv[0]));
        return 0;
    }

    // Set LogLevel above to Trace to show this message.
    logpp::trace(logpp::format("Running {}", argv[0]),
        logpp::field("user_name", argv[1]),
        logpp::field("password", argv[2])
    );

    if (authorizeUser(argv[1], argv[2]))
    {
        if (grantUser(argv[1], AccessRight::Read))
        {
            logpp::info("User had been granted",
                logpp::field("access_right", accessRightString(AccessRight::Read))
            );
        }
    }
}