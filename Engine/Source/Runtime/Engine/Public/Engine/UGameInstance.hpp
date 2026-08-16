#pragma once

#include "Gameplay/UObject.hpp"
#include "Engine/UWorld.hpp"

namespace Leon {

    /**
     * @brief Unreal Engine aligned GameInstance surviving across map loads.
     */
    class UGameInstance : public UObject {
    public:
        UGameInstance() = default;
        explicit UGameInstance(const std::string& InName) : UObject(InName) {}
        ~UGameInstance() override = default;

        virtual void Init() {}
        virtual void Shutdown() {}

        void SetWorld(const TRef<UWorld>& InWorld) { World = InWorld; }
        TRef<UWorld> GetWorld() const { return World; }

    protected:
        TRef<UWorld> World;
    };

} // namespace Leon
