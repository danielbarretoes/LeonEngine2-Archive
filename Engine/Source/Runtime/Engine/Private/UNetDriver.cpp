#include "Engine/UNetDriver.hpp"
#include "Engine/FNetBlob.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/ADefaultPawn.hpp"
#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerState.hpp"
#include "Gameplay/AAIController.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "Core/FFrameProfiler.hpp"

#include <cstring>

namespace Leon {

    namespace {
        constexpr uint32_t kSnapshotMagic = 0x4E455432; // "NET2"
    }

    UNetConnection* UNetDriver::AddConnection() {
        auto conn = std::make_shared<UNetConnection>();
        Connections.push_back(conn);
        return conn.get();
    }

    void UNetDriver::BuildSnapshot(std::vector<uint8_t>& OutBytes) const {
        OutBytes.clear();
        if (!World)
            return;
        FNetBlob::WriteU32(OutBytes, kSnapshotMagic);

        AGameStateBase* gs = World->GetGameState();
        FNetBlob::WriteF32(OutBytes, gs ? gs->GetElapsedTime() : 0.0f);
        FNetBlob::WriteString(OutBytes, gs ? gs->GetClass() : std::string("AGameStateBase"));
        std::vector<uint8_t> gsBlob;
        if (gs)
            gs->SerializeReplication(gsBlob);
        FNetBlob::WriteBlob(OutBytes, gsBlob);

        std::vector<APlayerState*> players;
        if (gs)
            players = gs->GetPlayerArray();
        FNetBlob::WriteU32(OutBytes, static_cast<uint32_t>(players.size()));
        for (APlayerState* ps : players) {
            if (!ps)
                continue;
            FNetBlob::WriteU64(OutBytes, ps->GetActorGuid().High);
            FNetBlob::WriteU64(OutBytes, ps->GetActorGuid().Low);
            FNetBlob::WriteU32(OutBytes, static_cast<uint32_t>(ps->GetPlayerId()));
            FNetBlob::WriteF32(OutBytes, ps->GetScore());
            FNetBlob::WriteString(OutBytes, ps->GetPlayerName());
            FNetBlob::WriteString(OutBytes, ps->GetClass());
            std::vector<uint8_t> blob;
            ps->SerializeReplication(blob);
            FNetBlob::WriteBlob(OutBytes, blob);
        }

        std::vector<APawn*> pawns;
        for (APlayerController* pc : World->GetPlayerControllers()) {
            if (pc && pc->GetPawn())
                pawns.push_back(pc->GetPawn());
        }
        for (AAIController* ai : World->GetAIControllers()) {
            if (ai && ai->GetPawn()) {
                APawn* pawn = ai->GetPawn();
                bool bDup = false;
                for (APawn* existing : pawns) {
                    if (existing == pawn) {
                        bDup = true;
                        break;
                    }
                }
                if (!bDup)
                    pawns.push_back(pawn);
            }
        }
        FNetBlob::WriteU32(OutBytes, static_cast<uint32_t>(pawns.size()));
        for (APawn* pawn : pawns) {
            FNetBlob::WriteU64(OutBytes, pawn->GetActorGuid().High);
            FNetBlob::WriteU64(OutBytes, pawn->GetActorGuid().Low);
            FNetBlob::WriteString(OutBytes, pawn->GetClass());
            int32_t ownerId = -1;
            if (AController* pc = pawn->GetController()) {
                if (APlayerState* ps = pc->GetPlayerState())
                    ownerId = ps->GetPlayerId();
            }
            FNetBlob::WriteI32(OutBytes, ownerId);
            glm::vec3 loc = pawn->GetActorLocation();
            glm::vec3 rot = pawn->GetActorRotation();
            FNetBlob::WriteF32(OutBytes, loc.x);
            FNetBlob::WriteF32(OutBytes, loc.y);
            FNetBlob::WriteF32(OutBytes, loc.z);
            FNetBlob::WriteF32(OutBytes, rot.x);
            FNetBlob::WriteF32(OutBytes, rot.y);
            FNetBlob::WriteF32(OutBytes, rot.z);
            FAnimRepState anim;
            if (auto* character = dynamic_cast<ACharacter*>(pawn))
                anim = character->GetAnimRepState();
            FNetBlob::WriteF32(OutBytes, anim.Speed);
            FNetBlob::WriteF32(OutBytes, anim.Direction);
            FNetBlob::WriteF32(OutBytes, anim.AimPitch);
            FNetBlob::WriteU32(OutBytes, anim.Flags);
            std::vector<uint8_t> blob;
            pawn->SerializeReplication(blob);
            FNetBlob::WriteBlob(OutBytes, blob);
        }
        FFrameProfiler::Working().ReplicatedActors = static_cast<int32_t>(pawns.size() + players.size());
    }

    void UNetDriver::ApplySnapshot(const std::vector<uint8_t>& InBytes) {
        if (!World || InBytes.empty())
            return;
        size_t offset = 0;
        uint32_t magic = 0;
        if (!FNetBlob::ReadU32(InBytes, offset, magic) || magic != kSnapshotMagic)
            return;

        float elapsed = 0.0f;
        if (!FNetBlob::ReadF32(InBytes, offset, elapsed))
            return;
        std::string gsClass;
        if (!FNetBlob::ReadString(InBytes, offset, gsClass))
            return;
        std::vector<uint8_t> gsBlob;
        if (!FNetBlob::ReadBlob(InBytes, offset, gsBlob))
            return;

        AGameStateBase* gs = World->GetGameState();
        if (!gs) {
            if (!gsClass.empty() && UClassRegistry::Get().HasClass(gsClass)) {
                gs = dynamic_cast<AGameStateBase*>(
                    UClassRegistry::Get().CreateActorOfClass(gsClass, World, "GameState"));
            }
            if (!gs)
                gs = World->SpawnActor<AGameStateBase>("GameState");
            World->SetGameState(gs);
        }
        if (gs) {
            gs->SetLocalRole(ENetRole::SimulatedProxy);
            gs->SetElapsedTime(elapsed);
            if (!gsBlob.empty())
                gs->DeserializeReplication(gsBlob.data(), gsBlob.size());
            if (World->GetNetMode() == ENetMode::Client)
                gs->ClearPlayerArray();
        }

        uint32_t playerCount = 0;
        if (!FNetBlob::ReadU32(InBytes, offset, playerCount))
            return;
        for (uint32_t i = 0; i < playerCount; ++i) {
            uint64_t hi = 0, lo = 0;
            uint32_t id = 0;
            float score = 0.0f;
            std::string name;
            std::string className;
            std::vector<uint8_t> blob;
            if (!FNetBlob::ReadU64(InBytes, offset, hi) || !FNetBlob::ReadU64(InBytes, offset, lo) ||
                !FNetBlob::ReadU32(InBytes, offset, id) || !FNetBlob::ReadF32(InBytes, offset, score) ||
                !FNetBlob::ReadString(InBytes, offset, name) || !FNetBlob::ReadString(InBytes, offset, className) ||
                !FNetBlob::ReadBlob(InBytes, offset, blob))
                return;
            FUUID guid(hi, lo);
            AActor* found = World->FindActorByGuid(guid);
            APlayerState* ps = dynamic_cast<APlayerState*>(found);
            if (!ps) {
                if (!className.empty() && UClassRegistry::Get().HasClass(className)) {
                    ps = dynamic_cast<APlayerState*>(
                        UClassRegistry::Get().CreateActorOfClass(className, World, "PlayerState"));
                }
                if (!ps)
                    ps = World->SpawnActor<APlayerState>("PlayerState");
                ps->SetActorGuid(guid);
            }
            ps->SetLocalRole(ENetRole::SimulatedProxy);
            ps->SetPlayerId(static_cast<int32_t>(id));
            ps->SetScore(score);
            ps->SetPlayerName(name);
            if (!blob.empty())
                ps->DeserializeReplication(blob.data(), blob.size());
            if (gs)
                gs->AddPlayerState(ps);
            if (World->GetNetMode() == ENetMode::Client && LocalPlayerId >= 0 &&
                static_cast<int32_t>(id) == LocalPlayerId) {
                if (APlayerController* pc = World->GetFirstPlayerController())
                    pc->SetPlayerState(ps);
            }
        }

        uint32_t pawnCount = 0;
        if (!FNetBlob::ReadU32(InBytes, offset, pawnCount))
            return;
        for (uint32_t i = 0; i < pawnCount; ++i) {
            uint64_t hi = 0, lo = 0;
            std::string className;
            float x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0, speed = 0, dir = 0, pitch = 0;
            uint32_t flags = 0;
            int32_t ownerId = -1;
            std::vector<uint8_t> blob;
            if (!FNetBlob::ReadU64(InBytes, offset, hi) || !FNetBlob::ReadU64(InBytes, offset, lo) ||
                !FNetBlob::ReadString(InBytes, offset, className) || !FNetBlob::ReadI32(InBytes, offset, ownerId) ||
                !FNetBlob::ReadF32(InBytes, offset, x) || !FNetBlob::ReadF32(InBytes, offset, y) ||
                !FNetBlob::ReadF32(InBytes, offset, z) || !FNetBlob::ReadF32(InBytes, offset, rx) ||
                !FNetBlob::ReadF32(InBytes, offset, ry) || !FNetBlob::ReadF32(InBytes, offset, rz) ||
                !FNetBlob::ReadF32(InBytes, offset, speed) || !FNetBlob::ReadF32(InBytes, offset, dir) ||
                !FNetBlob::ReadF32(InBytes, offset, pitch) || !FNetBlob::ReadU32(InBytes, offset, flags) ||
                !FNetBlob::ReadBlob(InBytes, offset, blob))
                return;
            FUUID guid(hi, lo);
            AActor* found = World->FindActorByGuid(guid);
            APawn* pawn = dynamic_cast<APawn*>(found);
            if (!pawn) {
                if (!className.empty() && UClassRegistry::Get().HasClass(className)) {
                    pawn = dynamic_cast<APawn*>(UClassRegistry::Get().CreateActorOfClass(className, World, "Pawn"));
                }
                if (!pawn)
                    pawn = World->SpawnActor<ADefaultPawn>("DefaultPawn");
                pawn->SetActorGuid(guid);
            }
            const bool bOwnPawn =
                World->GetNetMode() == ENetMode::Client && LocalPlayerId >= 0 && ownerId == LocalPlayerId;
            if (bOwnPawn) {
                if (APlayerController* pc = World->GetFirstPlayerController())
                    pc->Possess(pawn);
            }
            pawn->SetLocalRole(bOwnPawn ? ENetRole::AutonomousProxy : ENetRole::SimulatedProxy);
            pawn->SetActorLocation({x, y, z});
            pawn->SetActorRotation({rx, ry, rz});
            if (auto* character = dynamic_cast<ACharacter*>(pawn)) {
                FAnimRepState anim;
                anim.Speed = speed;
                anim.Direction = dir;
                anim.AimPitch = pitch;
                anim.Flags = flags;
                character->SetAnimRepState(anim);
            }
            if (!blob.empty())
                pawn->DeserializeReplication(blob.data(), blob.size());
        }
    }

    void UNetDriver::ConsumeIncomingInput() {
        if (!World || World->GetNetMode() != ENetMode::ListenServer)
            return;
        const auto& pcs = World->GetPlayerControllers();
        for (size_t i = 0; i < Connections.size(); ++i) {
            auto& conn = Connections[i];
            if (!conn || conn->IncomingInput.empty())
                continue;

            APlayerController* pc = nullptr;
            if (conn->BoundPlayerId >= 0) {
                for (APlayerController* candidate : pcs) {
                    if (candidate && candidate->GetPlayerState() &&
                        candidate->GetPlayerState()->GetPlayerId() == conn->BoundPlayerId) {
                        pc = candidate;
                        break;
                    }
                }
            } else {
                // Loopback / tests: first connection is the first remote controller (host is [0]).
                const size_t pcIndex = i + 1;
                if (pcIndex < pcs.size())
                    pc = pcs[pcIndex];
            }
            if (!pc || !pc->GetPawn()) {
                conn->IncomingInput.clear();
                continue;
            }
            pc->GetPawn()->ApplyControlInput(conn->IncomingInput.data(), conn->IncomingInput.size());
            conn->IncomingInput.clear();
        }
    }

    void UNetDriver::Tick(float InDeltaSeconds) {
        (void)InDeltaSeconds;
        if (!World)
            return;

        // Flow: listen-server snapshot
        // 1. Host builds one GameState + PlayerState + pawn blob.
        // 2. Every connection gets the same Outgoing copy (UDP layer sends it).
        // 3. Client Tick writes local control bits, then applies Incoming snapshot
        //    (possessing the pawn whose owner PlayerId matches LocalPlayerId).
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
                if (!conn)
                    continue;
                conn->OutgoingInput.clear();
                if (APlayerController* pc = World->GetFirstPlayerController()) {
                    if (APawn* pawn = pc->GetPawn())
                        pawn->SerializeControlInput(conn->OutgoingInput);
                }
                if (!conn->Incoming.empty()) {
                    ApplySnapshot(conn->Incoming);
                    conn->Incoming.clear();
                }
            }
        }
    }

} // namespace Leon
