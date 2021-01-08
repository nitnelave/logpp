#include "logpp/logpp.h"

#include "logpp/core/Logger.h"
#include "logpp/sinks/LogFmt.h"

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
        logpp::data("username", userName),
        logpp::data("password", password)
    );

    return true;
}

bool grantUser(const std::string& userName, AccessRight access)
{
    if (userName == "admin")
        logpp::warn("Attempting to grant access to admin user");

    logpp::debug(logpp::format("Granting access to user {}", userName),
        logpp::data("access_right", accessRightString(access))
    );

    return true;
}

int main(int argc, const char *argv[])
{
    if (argc < 3)
        return 0;
    
    if (authorizeUser(argv[1], argv[2]))
    {
        if (grantUser(argv[1], AccessRight::Read))
        {
            logpp::info("User had been granted",
                logpp::data("access_right", accessRightString(AccessRight::Read))
            );
        }
    }
}