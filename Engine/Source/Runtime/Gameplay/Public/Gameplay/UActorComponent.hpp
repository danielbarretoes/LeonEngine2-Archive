#pragma once

#include "Gameplay/UObject.hpp"

namespace Leon {

    class AActor;

    /**
     * @brief Lite tickable component owned by an AActor (logic only — not EnTT POD render comps).
     */
    class UActorComponent : public UObject {
    public:
        UActorComponent(const std::string& InName = "ActorComponent");
        ~UActorComponent() override = default;

        virtual void BeginPlay() {}
        virtual void Tick(float DeltaSeconds) { (void)DeltaSeconds; }
        virtual void EndPlay() {}

        /** Avoids dynamic_cast on the world overlap gather hot path. */
        virtual class UPrimitiveComponent* AsPrimitiveComponent() { return nullptr; }

        AActor* GetOwner() const { return Owner; }
        void SetOwner(AActor* InOwner) { Owner = InOwner; }

        bool IsComponentTickEnabled() const { return bTickEnabled; }
        void SetComponentTickEnabled(bool bEnabled) { bTickEnabled = bEnabled; }

        bool HasBegunPlay() const { return bHasBegunPlay; }
        void MarkBegunPlay() { bHasBegunPlay = true; }

    protected:
        AActor* Owner = nullptr;
        bool bTickEnabled = true;
        bool bHasBegunPlay = false;
    };

} // namespace Leon
