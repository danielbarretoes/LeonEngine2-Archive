#pragma once

#include "Gameplay/AActor.hpp"

namespace Leon {

    /**
     * Map sky / IBL actor. One FSkyboxComponent per world is the intended setup.
     */
    class ASkyLight : public AActor {
    public:
        ASkyLight() = default;
        ASkyLight(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "SkyLight");

        void PostInitializeComponents() override;
    };

} // namespace Leon
