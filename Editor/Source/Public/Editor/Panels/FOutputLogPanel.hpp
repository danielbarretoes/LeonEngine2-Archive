#pragma once

#include "Core/Base.hpp"
#include <string>
#include <unordered_set>
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
        void CopySelectedToClipboard();
        void CopyAllVisibleToClipboard();

        void Draw(bool* bInOutOpen = nullptr);

    private:
        std::string FormatEntry(const FLogEntry& InEntry) const;

        std::vector<FLogEntry> Entries;
        std::unordered_set<size_t> SelectedIndices;
        char FilterBuffer[128] = "";
        bool bAutoScroll = true;
        bool bShowInfo = true;
        bool bShowWarnings = true;
        bool bShowErrors = true;
    };

} // namespace Leon::Editor
