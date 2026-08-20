#pragma once

#include "Gameplay/APickup.hpp"
#include "Gameplay/ALaunchPad.hpp"
#include "FLeonTournamentTypes.hpp"

namespace Leon {

    class ALeonTournamentCharacter;

    class ALeonTournamentPickup : public APickup {
    public:
        ALeonTournamentPickup() = default;
        ALeonTournamentPickup(entt::entity InHandle, UWorld* InWorld,
                              const std::string& InName = "LeonTournamentPickup");

        void SetPickupActive(bool bInActive) override;

    protected:
        bool CanBePickedUp(APawn* InPawn) const override;
        bool GiveTo(APawn* InPawn) override;
        virtual bool TryGiveTo(ALeonTournamentCharacter& InCharacter) = 0;

        glm::vec3 VisualColor{0.9f, 0.9f, 0.2f};
    };

    class ALeonTournamentWeaponPickup : public ALeonTournamentPickup {
    public:
        ALeonTournamentWeaponPickup() = default;
        ALeonTournamentWeaponPickup(entt::entity InHandle, UWorld* InWorld,
                                    const std::string& InName = "LeonTournamentWeaponPickup");

        void SetWeaponId(ELeonTournamentWeaponId InId);
        ELeonTournamentWeaponId GetWeaponId() const { return WeaponId; }

    protected:
        bool TryGiveTo(ALeonTournamentCharacter& InCharacter) override;
        void BuildVisual() override;

        ELeonTournamentWeaponId WeaponId = ELeonTournamentWeaponId::Shotgun;
    };

    class ALeonTournamentHealthPickup : public ALeonTournamentPickup {
    public:
        ALeonTournamentHealthPickup() = default;
        ALeonTournamentHealthPickup(entt::entity InHandle, UWorld* InWorld,
                                    const std::string& InName = "LeonTournamentHealthPickup");

    protected:
        bool TryGiveTo(ALeonTournamentCharacter& InCharacter) override;
        void BuildVisual() override;
    };

    class ALeonTournamentJumpPad : public ALaunchPad {
    public:
        ALeonTournamentJumpPad() = default;
        ALeonTournamentJumpPad(entt::entity InHandle, UWorld* InWorld,
                               const std::string& InName = "LeonTournamentJumpPad");

        void SetPadVelocity(const glm::vec3& InVelocity) { SetLaunchVelocity(InVelocity); }

    protected:
        void BuildVisual() override;
        void OnLaunched(ACharacter* InCharacter) override;
    };

} // namespace Leon
