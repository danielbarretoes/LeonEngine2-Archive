#pragma once

#include "Core/Base.hpp"
#include <format>
#include <string_view>
#include <utility>

namespace Leon {

    enum class ELogLevel { Trace = 0, Info, Warn, Error, Fatal };

    class FLog {
    public:
        static void Init();

        static void Print(ELogLevel InLevel, std::string_view InTag, std::string_view InMessage);

        template <typename... TArgs> static void CoreTrace(std::format_string<TArgs...> InFmt, TArgs&&... InArgs) {
            Print(ELogLevel::Trace, "CORE", std::format(InFmt, std::forward<TArgs>(InArgs)...));
        }

        template <typename... TArgs> static void CoreInfo(std::format_string<TArgs...> InFmt, TArgs&&... InArgs) {
            Print(ELogLevel::Info, "CORE", std::format(InFmt, std::forward<TArgs>(InArgs)...));
        }

        template <typename... TArgs> static void CoreWarn(std::format_string<TArgs...> InFmt, TArgs&&... InArgs) {
            Print(ELogLevel::Warn, "CORE", std::format(InFmt, std::forward<TArgs>(InArgs)...));
        }

        template <typename... TArgs> static void CoreError(std::format_string<TArgs...> InFmt, TArgs&&... InArgs) {
            Print(ELogLevel::Error, "CORE", std::format(InFmt, std::forward<TArgs>(InArgs)...));
        }

        template <typename... TArgs> static void Info(std::format_string<TArgs...> InFmt, TArgs&&... InArgs) {
            Print(ELogLevel::Info, "APP", std::format(InFmt, std::forward<TArgs>(InArgs)...));
        }

        template <typename... TArgs> static void Warn(std::format_string<TArgs...> InFmt, TArgs&&... InArgs) {
            Print(ELogLevel::Warn, "APP", std::format(InFmt, std::forward<TArgs>(InArgs)...));
        }

        template <typename... TArgs> static void Error(std::format_string<TArgs...> InFmt, TArgs&&... InArgs) {
            Print(ELogLevel::Error, "APP", std::format(InFmt, std::forward<TArgs>(InArgs)...));
        }
    };

} // namespace Leon

#define LE_CORE_TRACE(...) ::Leon::FLog::CoreTrace(__VA_ARGS__)
#define LE_CORE_INFO(...) ::Leon::FLog::CoreInfo(__VA_ARGS__)
#define LE_CORE_WARN(...) ::Leon::FLog::CoreWarn(__VA_ARGS__)
#define LE_CORE_ERROR(...) ::Leon::FLog::CoreError(__VA_ARGS__)

#define LE_INFO(...) ::Leon::FLog::Info(__VA_ARGS__)
#define LE_WARN(...) ::Leon::FLog::Warn(__VA_ARGS__)
#define LE_ERROR(...) ::Leon::FLog::Error(__VA_ARGS__)
