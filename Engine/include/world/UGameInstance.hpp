#pragma once

#include "gameplay/UObject.hpp"
#include "world/UWorld.hpp"

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

        void SetWorld(const TRef<UWorld>& InWorld) { m_World = InWorld; }
        TRef<UWorld> GetWorld() const { return m_World; }

    protected:
        TRef<UWorld> m_World;
    };

} // namespace Leon
