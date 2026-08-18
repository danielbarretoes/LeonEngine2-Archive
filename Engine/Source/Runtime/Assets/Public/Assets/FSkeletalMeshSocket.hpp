#pragma once

#include <glm/glm.hpp>
#include <string>

namespace Leon {

    /**
     * Named offset from a skeleton bone (Unreal FSkeletalMeshSocket lite).
     */
    struct FSkeletalMeshSocket {
        std::string SocketName;
        std::string BoneName;
        glm::vec3 RelativeLocation{0.0f};
        glm::vec3 RelativeRotation{0.0f};
    };

} // namespace Leon
