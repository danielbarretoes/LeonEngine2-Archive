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
        void CopySelectedToClipboard();
        void CopyAllVisibleToClipboard();

        void Draw(bool* bInOutOpen = nullptr);

    private:
        std::string FormatEntry(const FLogEntry& InEntry) const;
        bool PassesFilters(const FLogEntry& InEntry, const std::string& InFilterLower) const;
        std::string MakeFilterLower() const;
        void RebuildVisibleText();
        void CacheSelectionFromActiveLog();

        std::vector<FLogEntry> Entries;
        std::string VisibleLogText;
        std::string CachedSelection;
        char FilterBuffer[128] = "";
        bool bAutoScroll = true;
        bool bWasAtBottom = true;
        bool bShowInfo = true;
        bool bShowWarnings = true;
        bool bShowErrors = true;
    };

} // namespace Leon::Editor
