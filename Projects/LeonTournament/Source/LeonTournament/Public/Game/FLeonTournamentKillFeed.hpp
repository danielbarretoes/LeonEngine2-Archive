#pragma once

#include "FLeonTournamentTypes.hpp"

#include <array>
#include <cstddef>

namespace Leon {

    class FLeonTournamentKillFeed {
    public:
        static constexpr size_t kMaxEntries = 8;

        void Push(const FLeonTournamentKillFeedEntry& InEntry);
        void Tick(float InDeltaSeconds);
        const std::array<FLeonTournamentKillFeedEntry, kMaxEntries>& GetEntries() const { return Entries; }
        size_t GetCount() const { return Count; }

    private:
        std::array<FLeonTournamentKillFeedEntry, kMaxEntries> Entries{};
        size_t Count = 0;
    };

} // namespace Leon
