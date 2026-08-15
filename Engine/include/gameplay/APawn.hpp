#pragma once

#include "gameplay/AActor.hpp"

namespace Leon {

    class APlayerController;

    /**
     * @brief Base class for any AActor that can be possessed and controlled by an APlayerController.
     */
    class APawn : public AActor {
    public:
        APawn() = default;
        APawn(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "Pawn");
        ~APawn() override = default;

        virtual void PossessedBy(APlayerController* InController);
        virtual void UnPossessed();

        APlayerController* GetController() const { return m_Controller; }
        bool IsControlled() const { return m_Controller != nullptr; }

        virtual void SetupPlayerInputComponent(float DeltaSeconds) {}

    protected:
        APlayerController* m_Controller = nullptr;
    };

} // namespace Leon
