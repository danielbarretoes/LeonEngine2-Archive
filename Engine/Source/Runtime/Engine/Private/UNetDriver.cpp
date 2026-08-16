#include "Engine/UNetDriver.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/ADefaultPawn.hpp"
#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerState.hpp"
#include "Gameplay/UClassRegistry.hpp"

#include <cstring>

namespace Leon {

    namespace {

        constexpr uint32_t kSnapshotMagic = 0x4E455431; // "NET1"

        void WriteU32(std::vector<uint8_t>& Out, uint32_t InValue) {
            Out.push_back(static_cast<uint8_t>(InValue));
            Out.push_back(static_cast<uint8_t>(InValue >> 8));
            Out.push_back(static_cast<uint8_t>(InValue >> 16));
            Out.push_back(static_cast<uint8_t>(InValue >> 24));
        }

        void WriteF32(std::vector<uint8_t>& Out, float InValue) {
            uint32_t bits = 0;
            std::memcpy(&bits, &InValue, sizeof(bits));
            WriteU32(Out, bits);
        }

        void WriteU64(std::vector<uint8_t>& Out, uint64_t InValue) {
            WriteU32(Out, static_cast<uint32_t>(InValue));
            WriteU32(Out, static_cast<uint32_t>(InValue >> 32));
        }

        void WriteString(std::vector<uint8_t>& Out, const std::string& InStr) {
            WriteU32(Out, static_cast<uint32_t>(InStr.size()));
            Out.insert(Out.end(), InStr.begin(), InStr.end());
        }

        bool ReadU32(const std::vector<uint8_t>& In, size_t& Offset, uint32_t& OutValue) {
            if (Offset + 4 > In.size())
                return false;
            OutValue = static_cast<uint32_t>(In[Offset]) | (static_cast<uint32_t>(In[Offset + 1]) << 8) |
                       (static_cast<uint32_t>(In[Offset + 2]) << 16) | (static_cast<uint32_t>(In[Offset + 3]) << 24);
            Offset += 4;
            return true;
        }

        bool ReadF32(const std::vector<uint8_t>& In, size_t& Offset, float& OutValue) {
            uint32_t bits = 0;
            if (!ReadU32(In, Offset, bits))
                return false;
            std::memcpy(&OutValue, &bits, sizeof(bits));
            return true;
        }

        bool ReadU64(const std::vector<uint8_t>& In, size_t& Offset, uint64_t& OutValue) {
            uint32_t lo = 0, hi = 0;
            if (!ReadU32(In, Offset, lo) || !ReadU32(In, Offset, hi))
                return false;
            OutValue = static_cast<uint64_t>(lo) | (static_cast<uint64_t>(hi) << 32);
            return true;
        }

        bool ReadString(const std::vector<uint8_t>& In, size_t& Offset, std::string& OutStr) {
            uint32_t len = 0;
            if (!ReadU32(In, Offset, len) || Offset + len > In.size())
                return false;
            OutStr.assign(reinterpret_cast<const char*>(In.data() + Offset), len);
            Offset += len;
            return true;
        }

    } // namespace

    UNetConnection* UNetDriver::AddConnection() {
        auto conn = std::make_shared<UNetConnection>();
        Connections.push_back(conn);
        return conn.get();
    }

    void UNetDriver::BuildSnapshot(std::vector<uint8_t>& OutBytes) const {
        OutBytes.clear();
        if (!World)
            return;
        WriteU32(OutBytes, kSnapshotMagic);

        AGameStateBase* gs = World->GetGameState();
        WriteF32(OutBytes, gs ? gs->GetElapsedTime() : 0.0f);

        std::vector<APlayerState*> players;
        if (gs)
            players = gs->GetPlayerArray();
        WriteU32(OutBytes, static_cast<uint32_t>(players.size()));
        for (APlayerState* ps : players) {
            if (!ps)
                continue;
            WriteU64(OutBytes, ps->GetActorGuid().High);
            WriteU64(OutBytes, ps->GetActorGuid().Low);
            WriteU32(OutBytes, static_cast<uint32_t>(ps->GetPlayerId()));
            WriteF32(OutBytes, ps->GetScore());
            WriteString(OutBytes, ps->GetPlayerName());
        }

        std::vector<APawn*> pawns;
        for (APlayerController* pc : World->GetPlayerControllers()) {
            if (pc && pc->GetPawn())
                pawns.push_back(pc->GetPawn());
        }
        WriteU32(OutBytes, static_cast<uint32_t>(pawns.size()));
        for (APawn* pawn : pawns) {
            WriteU64(OutBytes, pawn->GetActorGuid().High);
            WriteU64(OutBytes, pawn->GetActorGuid().Low);
            glm::vec3 loc = pawn->GetActorLocation();
            glm::vec3 rot = pawn->GetActorRotation();
            WriteF32(OutBytes, loc.x);
            WriteF32(OutBytes, loc.y);
            WriteF32(OutBytes, loc.z);
            WriteF32(OutBytes, rot.x);
            WriteF32(OutBytes, rot.y);
            WriteF32(OutBytes, rot.z);
        }
    }

    void UNetDriver::ApplySnapshot(const std::vector<uint8_t>& InBytes) {
        if (!World || InBytes.empty())
            return;
        size_t offset = 0;
        uint32_t magic = 0;
        if (!ReadU32(InBytes, offset, magic) || magic != kSnapshotMagic)
            return;

        float elapsed = 0.0f;
        if (!ReadF32(InBytes, offset, elapsed))
            return;

        AGameStateBase* gs = World->GetGameState();
        if (!gs) {
            gs = World->SpawnActor<AGameStateBase>("GameState");
            World->SetGameState(gs);
        }
        if (gs) {
            gs->SetLocalRole(ENetRole::SimulatedProxy);
            gs->SetElapsedTime(elapsed);
        }

        uint32_t playerCount = 0;
        if (!ReadU32(InBytes, offset, playerCount))
            return;
        for (uint32_t i = 0; i < playerCount; ++i) {
            uint64_t hi = 0, lo = 0;
            uint32_t id = 0;
            float score = 0.0f;
            std::string name;
            if (!ReadU64(InBytes, offset, hi) || !ReadU64(InBytes, offset, lo) || !ReadU32(InBytes, offset, id) ||
                !ReadF32(InBytes, offset, score) || !ReadString(InBytes, offset, name))
                return;
            FUUID guid(hi, lo);
            AActor* found = World->FindActorByGuid(guid);
            APlayerState* ps = dynamic_cast<APlayerState*>(found);
            if (!ps) {
                ps = World->SpawnActor<APlayerState>("PlayerState");
                ps->SetActorGuid(guid);
            }
            ps->SetLocalRole(ENetRole::SimulatedProxy);
            ps->SetPlayerId(static_cast<int32_t>(id));
            ps->SetScore(score);
            ps->SetPlayerName(name);
            if (gs)
                gs->AddPlayerState(ps);
        }

        uint32_t pawnCount = 0;
        if (!ReadU32(InBytes, offset, pawnCount))
            return;
        for (uint32_t i = 0; i < pawnCount; ++i) {
            uint64_t hi = 0, lo = 0;
            float x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0;
            if (!ReadU64(InBytes, offset, hi) || !ReadU64(InBytes, offset, lo) || !ReadF32(InBytes, offset, x) ||
                !ReadF32(InBytes, offset, y) || !ReadF32(InBytes, offset, z) || !ReadF32(InBytes, offset, rx) ||
                !ReadF32(InBytes, offset, ry) || !ReadF32(InBytes, offset, rz))
                return;
            FUUID guid(hi, lo);
            AActor* found = World->FindActorByGuid(guid);
            APawn* pawn = dynamic_cast<APawn*>(found);
            if (!pawn) {
                pawn = World->SpawnActor<ADefaultPawn>("DefaultPawn");
                pawn->SetActorGuid(guid);
            }
            pawn->SetLocalRole(ENetRole::SimulatedProxy);
            pawn->SetActorLocation({x, y, z});
            pawn->SetActorRotation({rx, ry, rz});
        }
    }

    void UNetDriver::Tick(float InDeltaSeconds) {
        (void)InDeltaSeconds;
        if (!World)
            return;

        if (World->GetNetMode() == ENetMode::ListenServer) {
            std::vector<uint8_t> snapshot;
            BuildSnapshot(snapshot);
            for (auto& conn : Connections) {
                if (conn)
                    conn->Outgoing = snapshot;
            }
            return;
        }

        if (World->GetNetMode() == ENetMode::Client) {
            for (auto& conn : Connections) {
                if (conn && !conn->Incoming.empty()) {
                    ApplySnapshot(conn->Incoming);
                    conn->Incoming.clear();
                }
            }
        }
    }

} // namespace Leon
