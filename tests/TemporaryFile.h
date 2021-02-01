#pragma once

#include "logpp/core/config.h"
#include "logpp/sinks/file/FileSink.h"

#include <fmt/format.h>

#if defined(LOGPP_PLATFORM_WINDOWS)
  #include <windows.h>
  #include <fileapi.h>
  #include <rpc.h>
  #pragma comment(lib, "Rpcrt4.lib")
#endif

namespace logpp
{

    class TemporaryFile : public sink::File
    {
    public:
        TemporaryFile(std::ios_base::openmode openMode, std::string_view prefix, std::string_view suffix)
            : m_directory(createTemporaryDirectory())
            , m_name(createTemporaryFileName(prefix, suffix))
        {
            auto path = fmt::format("{}/{}", m_directory, m_name);
            open(path, openMode);
        }

        std::string_view directory() const
        {
            return m_directory;
        }

        std::string_view name() const
        {
            return m_name;
        }

    private:
        std::string m_directory;
        std::string m_name;

        static std::string createTemporaryFileName(std::string_view prefix, std::string_view suffix)
        {
            // We are not guaranteed that file names generated by this kind of algorithm will be unique
            // However, we are generating a unique temporary directory, so full path of temporary files
            // will be unique in that unique directory
            static size_t idx = 0;
            return fmt::format("{}-{}{}", prefix, idx++, suffix);
        }

        static std::string createTemporaryDirectory()
        {
        #if defined(LOGPP_PLATFORM_LINUX) 
            char templateName[] = "logpptmpXXXXXX";
            auto res = mkdtemp(templateName);
            if (!res)
                throw std::runtime_error("failed to create temporary directory");
            return std::string(templateName);
        #elif defined(LOGPP_PLATFORM_WINDOWS)
            UUID uuid;
            if (UuidCreate(&uuid) != RPC_S_OK)
                throw std::runtime_error("failed to create temporary directory: could not generate uuid-based unique name");

            unsigned char* str;
            if (UuidToString(&uuid, &str) != RPC_S_OK)
                throw std::runtime_error("failed to create temporary directory: could not generate uuid-based unique name");

            auto dir = std::string("logpptmp-") + std::string(reinterpret_cast<char *>(str));
            RpcStringFree(&str);

            if (!CreateDirectoryA(dir.c_str(), NULL))
                throw std::runtime_error("failed to create temporary directory");

            return dir;
        #else
            #error "Unsupported platform"
        #endif
        }

    };

}