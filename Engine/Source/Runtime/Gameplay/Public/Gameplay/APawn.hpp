#pragma once

#include "Gameplay/AActor.hpp"

namespace Leon {

    class AController;

    /**
     * @brief Base class for any AActor that can be possessed by an AController.
     */
    class APawn : public AActor {
    public:
        APawn() = default;
        APawn(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "Pawn");
        ~APawn() override = default;

        virtual void PossessedBy(AController* InController);
        virtual void UnPossessed();

        AController* GetController() const { return Controller; }
        bool IsControlled() const { return Controller != nullptr; }
        bool IsLocallyControlled() const;

        virtual void SetupPlayerInputComponent(float DeltaSeconds) {}

    protected:
        AController* Controller = nullptr;
    };

} // namespace Leon
