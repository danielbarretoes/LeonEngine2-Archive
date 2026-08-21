#pragma once

#include "Core/Base.hpp"
#include "Gameplay/UObject.hpp"
#include "Physics/IPhysicsScene.hpp"

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace Leon {

    class USkeleton;

    enum class EPhysicsAssetBodyShape : uint8_t { Sphere = 0, Box = 1, Capsule = 2 };

    struct FPhysicsAssetBody {
        std::string BoneName;
        EPhysicsAssetBodyShape Shape = EPhysicsAssetBodyShape::Capsule;
        glm::vec3 Offset{0.0f};
        glm::vec3 BoxExtent{0.1f, 0.1f, 0.1f};
        float Radius = 0.08f;
        float CapsuleHalfHeight = 0.16f;
        float Mass = 5.0f;
    };

    struct FPhysicsAssetConstraint {
        std::string BoneA;
        std::string BoneB;
        EPhysicsConstraintType Type = EPhysicsConstraintType::SwingTwist;
        float RestLength = 0.0f;
        glm::vec3 Axis{0.0f, 1.0f, 0.0f};
        float Swing1LimitRadians = 0.7f;
        float Swing2LimitRadians = 0.7f;
        float TwistLimitRadians = 0.5f;
    };

    /**
     * Collision bodies associated with skeleton bones. Data model for ragdoll.
     */
    class UPhysicsAsset : public UObject {
    public:
        UPhysicsAsset(const std::string& InName = "PhysicsAsset");

        void AddBody(const FPhysicsAssetBody& InBody) { Bodies.push_back(InBody); }
        const std::vector<FPhysicsAssetBody>& GetBodies() const { return Bodies; }

        void AddConstraint(const FPhysicsAssetConstraint& InConstraint) { Constraints.push_back(InConstraint); }
        const std::vector<FPhysicsAssetConstraint>& GetConstraints() const { return Constraints; }

        static TRef<UPhysicsAsset> CreateHumanoidFromSkeleton(const USkeleton& InSkeleton);

        bool SaveToFile(const std::string& InPath) const;
        bool LoadFromFile(const std::string& InPath);

        static constexpr uint32_t Magic = 0x4853504C; // on-disk ASCII 'LPSH' (LE uint32)
        static constexpr uint32_t Version = 3;

    private:
        std::vector<FPhysicsAssetBody> Bodies;
        std::vector<FPhysicsAssetConstraint> Constraints;
    };

} // namespace Leon
