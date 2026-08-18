#pragma once

#include "Gameplay/AAIController.hpp"
#include "FLeonTournamentTypes.hpp"
#include "Gameplay/FDamageInfo.hpp"

#include <glm/glm.hpp>
#include <vector>

namespace Leon {

    class ALeonTournamentCharacter;

    class ALeonTournamentBotController : public AAIController {
    public:
        ALeonTournamentBotController() = default;
        ALeonTournamentBotController(entt::entity InHandle, UWorld* InWorld,
                                     const std::string& InName = "LeonTournamentBotController");

        void PostInitializeComponents() override;
        void Possess(APawn* InPawn) override;
        void Tick(float DeltaSeconds) override;
        void SetWaypoints(const std::vector<glm::vec3>& InPoints) { Waypoints = InPoints; }
        void SetCoverPoints(const std::vector<glm::vec3>& InPoints) { CoverPoints = InPoints; }

        ELeonTournamentBotState GetBotState() const;
        ALeonTournamentCharacter* GetCurrentTarget() const;
        bool HasLineOfSight(ALeonTournamentCharacter& InTarget) const;
        const FLeonTournamentBotPersonality& GetPersonality() const { return Personality; }

        void NotifyDamaged(const FDamageInfo& InInfo);
        void NotifyRespawned();
        bool IsBehaviorTreeRunning() const;

    private:
        void BuildBehaviorTree();
        void AssignPersonality();
        void TickPerception();
        void TickAim(float DeltaSeconds, const glm::vec3& InWorldPoint);
        bool IsAimAligned(const glm::vec3& InWorldPoint, float InDegrees) const;
        glm::vec3 PickApproachLocation(const glm::vec3& InFrom, const glm::vec3& InTarget, float InRange) const;
        glm::vec3 PickCoverLocation(ALeonTournamentCharacter& InSelf, ALeonTournamentCharacter* InTarget) const;
        glm::vec3 PickPatrolLocation(ALeonTournamentCharacter& InSelf) const;
        void DrawDebug() const;

        TRef<UBehaviorTree> Tree;
        TRef<UBlackboardData> BoardAsset;
        std::vector<glm::vec3> Waypoints;
        std::vector<glm::vec3> CoverPoints;
        FLeonTournamentBotPersonality Personality;
        float AcquireTime = 0.0f;
        float StrafeTimer = 0.0f;
        float StrafeSign = 1.0f;
        float LookAroundTimer = 0.0f;
        mutable glm::vec3 LastPatrolGoal{0.0f};
        mutable bool bHasLastPatrolGoal = false;
        bool bDamageBound = false;
        ELeonTournamentBotState CachedState = ELeonTournamentBotState::Idle;
    };

} // namespace Leon
