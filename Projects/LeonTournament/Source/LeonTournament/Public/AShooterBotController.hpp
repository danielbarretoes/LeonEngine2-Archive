#pragma once

#include "Gameplay/AAIController.hpp"
#include "FShooterTypes.hpp"
#include "Gameplay/FDamageInfo.hpp"

#include <glm/glm.hpp>
#include <vector>

namespace Leon {

    class AShooterCharacter;

    class AShooterBotController : public AAIController {
    public:
        AShooterBotController() = default;
        AShooterBotController(entt::entity InHandle, UWorld* InWorld,
                              const std::string& InName = "ShooterBotController");

        void PostInitializeComponents() override;
        void Possess(APawn* InPawn) override;
        void Tick(float DeltaSeconds) override;
        void SetWaypoints(const std::vector<glm::vec3>& InPoints) { Waypoints = InPoints; }
        void SetCoverPoints(const std::vector<glm::vec3>& InPoints) { CoverPoints = InPoints; }

        EShooterBotState GetBotState() const;
        AShooterCharacter* GetCurrentTarget() const;
        bool HasLineOfSight(AShooterCharacter& InTarget) const;
        const FShooterBotPersonality& GetPersonality() const { return Personality; }

        void NotifyDamaged(const FDamageInfo& InInfo);
        void NotifyRespawned();
        bool IsBehaviorTreeRunning() const;

    private:
        AShooterCharacter* GetShooterPawn() const;
        void BuildBehaviorTree();
        void AssignPersonality();
        void TickPerception();
        void TickAim(float DeltaSeconds, const glm::vec3& InWorldPoint);
        bool IsAimAligned(const glm::vec3& InWorldPoint, float InDegrees) const;
        glm::vec3 PickApproachLocation(const glm::vec3& InFrom, const glm::vec3& InTarget, float InRange) const;
        glm::vec3 PickCoverLocation(AShooterCharacter& InSelf, AShooterCharacter* InTarget) const;
        glm::vec3 PickPatrolLocation(AShooterCharacter& InSelf) const;
        void DrawDebug() const;

        TRef<UBehaviorTree> Tree;
        TRef<UBlackboardData> BoardAsset;
        std::vector<glm::vec3> Waypoints;
        std::vector<glm::vec3> CoverPoints;
        FShooterBotPersonality Personality;
        glm::vec3 LastKnownLocation{0.0f};
        float LastKnownAge = 0.0f;
        float AcquireTime = 0.0f;
        float StrafeTimer = 0.0f;
        float StrafeSign = 1.0f;
        float LookAroundTimer = 0.0f;
        bool bDamageBound = false;
        EShooterBotState CachedState = EShooterBotState::Idle;
    };

} // namespace Leon
