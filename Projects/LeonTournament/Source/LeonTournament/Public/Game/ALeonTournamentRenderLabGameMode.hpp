#pragma once

#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentCharacter.hpp"

namespace Leon {

    /**
     * Fixed-camera graphics lab: material spheres, lights, idle character, HUD knobs.
     * Used to A/B Low/Medium/High and individual renderer categories.
     */
    class ALeonTournamentRenderLabGameMode : public ALeonTournamentGameMode {
    public:
        ALeonTournamentRenderLabGameMode() = default;
        ALeonTournamentRenderLabGameMode(entt::entity InHandle, UWorld* InWorld,
                                         const std::string& InName = "LeonTournamentRenderLabGameMode");

        void InitGame() override;
        void StartPlay() override;
        void Tick(float DeltaSeconds) override;
        void RestartPlayer(AController* NewPlayer) override;
        bool WantsUICursor() const override { return true; }
        void CycleLabCamera();
        int32_t GetLabCameraIndex() const { return LabCameraIndex; }
        static constexpr int32_t LabCameraCount = 3;
        void PunchLabPreview();

    private:
        void ApplyLabClasses();
        void SpawnLabProps();
        void SpawnIdleCharacter();
        void SetupFixedCamera();
        void BindLabCamera();
        void EnsurePlanarPlane();

        ALeonTournamentCharacter* IdleCharacter = nullptr;
        int32_t LabCameraIndex = 0;
    };

} // namespace Leon
