#pragma once

#include "Gameplay/UObject.hpp"

namespace Leon {

    /**
     * Surface properties applied to rigid bodies. Unreal: UPhysicalMaterial.
     */
    class UPhysicalMaterial : public UObject {
    public:
        UPhysicalMaterial(const std::string& InName = "PhysicalMaterial");

        float Friction = 0.7f;
        float Restitution = 0.0f;
        float Density = 1000.0f;
    };

} // namespace Leon
