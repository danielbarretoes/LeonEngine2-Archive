#include "core/Log.hpp"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Leon {

    void FLog::Init() {
#ifdef _WIN32
        // Enable ANSI color escape sequences on Windows terminal
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
#endif
    }

    void FLog::Print(ELogLevel InLevel, std::string_view InTag, std::string_view InMessage) {
        const char* colorCode = "\033[0m";
        const char* levelStr = "INFO";

        switch (InLevel) {
        case ELogLevel::Trace:
            colorCode = "\033[37m"; // White
            levelStr = "TRACE";
            break;
        case ELogLevel::Info:
            colorCode = "\033[32m"; // Green
            levelStr = "INFO";
            break;
        case ELogLevel::Warn:
            colorCode = "\033[33m"; // Yellow
            levelStr = "WARN";
            break;
        case ELogLevel::Error:
        case ELogLevel::Fatal:
            colorCode = "\033[31m"; // Red
            levelStr = "ERROR";
            break;
        }

        std::cout << colorCode << "[" << InTag << "] [" << levelStr << "]: " << InMessage << "\033[0m" << std::endl;
    }

} // namespace Leon
