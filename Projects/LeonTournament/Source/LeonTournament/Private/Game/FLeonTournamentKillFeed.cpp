#include "FLeonTournamentKillFeed.hpp"

#include <algorithm>

namespace Leon {

    void FLeonTournamentKillFeed::Push(const FLeonTournamentKillFeedEntry& InEntry) {
        if (Count < kMaxEntries) {
            Entries[Count++] = InEntry;
            return;
        }
        for (size_t i = 1; i < kMaxEntries; ++i)
            Entries[i - 1] = Entries[i];
        Entries[kMaxEntries - 1] = InEntry;
    }

    void FLeonTournamentKillFeed::Tick(float InDeltaSeconds) {
        size_t write = 0;
        for (size_t i = 0; i < Count; ++i) {
            Entries[i].TimeRemaining -= InDeltaSeconds;
            if (Entries[i].TimeRemaining > 0.0f)
                Entries[write++] = Entries[i];
        }
        Count = write;
    }

} // namespace Leon
