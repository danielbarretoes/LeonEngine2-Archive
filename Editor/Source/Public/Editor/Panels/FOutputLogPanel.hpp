#pragma once

#include "Core/Base.hpp"
#include <string>
#include <vector>

namespace Leon::Editor {

    enum class ELogLevel { Info, Warning, Error };

    struct FLogEntry {
        ELogLevel Level = ELogLevel::Info;
        std::string Category;
        std::string Message;
        std::string Timestamp;
    };

    /**
     * @brief Output Log console panel showing engine logs and diagnostics.
     */
    class FOutputLogPanel {
    public:
        FOutputLogPanel() = default;

        void AddLog(ELogLevel InLevel, const std::string& InCategory, const std::string& InMessage);
        void Clear();

        void Draw(bool* bInOutOpen = nullptr);

    private:
        std::vector<FLogEntry> Entries;
        char FilterBuffer[128] = "";
        bool bAutoScroll = true;
        bool bShowInfo = true;
        bool bShowWarnings = true;
        bool bShowErrors = true;
    };

} // namespace Leon::Editor
