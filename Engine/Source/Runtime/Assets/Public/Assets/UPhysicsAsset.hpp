#pragma once

#include "Core/Base.hpp"
#include "Gameplay/UObject.hpp"

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace Leon {

    enum class EPhysicsAssetBodyShape : uint8_t { Sphere = 0, Box = 1, Capsule = 2 };

    struct FPhysicsAssetBody {
        std::string BoneName;
        EPhysicsAssetBodyShape Shape = EPhysicsAssetBodyShape::Capsule;
        glm::vec3 Offset{0.0f};
        glm::vec3 BoxExtent{0.1f, 0.1f, 0.1f};
        float Radius = 0.08f;
        float CapsuleHalfHeight = 0.16f;
    };

    /**
     * Collision bodies associated with skeleton bones. Data model for ragdoll; no solver yet.
     */
    class UPhysicsAsset : public UObject {
    public:
        UPhysicsAsset(const std::string& InName = "PhysicsAsset");

        void AddBody(const FPhysicsAssetBody& InBody) { Bodies.push_back(InBody); }
        const std::vector<FPhysicsAssetBody>& GetBodies() const { return Bodies; }

        bool SaveToFile(const std::string& InPath) const;
        bool LoadFromFile(const std::string& InPath);

        static constexpr uint32_t Magic = 0x48535950; // 'PHYH' packed differently
        static constexpr uint32_t Version = 1;

    private:
        std::vector<FPhysicsAssetBody> Bodies;
    };

} // namespace Leon
