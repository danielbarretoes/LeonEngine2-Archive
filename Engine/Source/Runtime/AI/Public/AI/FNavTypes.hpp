#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace Leon {

    enum class ENavPathStatus : uint8_t { Invalid = 0, Partial = 1, Valid = 2 };

    inline const char* NavPathStatusName(ENavPathStatus InStatus) {
        switch (InStatus) {
        case ENavPathStatus::Valid:
            return "PATH VALID";
        case ENavPathStatus::Partial:
            return "PATH PARTIAL";
        default:
            return "PATH INVALID";
        }
    }

    struct FNavPath {
        std::vector<glm::vec3> Points;
        ENavPathStatus Status = ENavPathStatus::Invalid;
        float Length = 0.0f;

        bool IsValid() const { return Status != ENavPathStatus::Invalid && Points.size() >= 2; }
        int32_t NumPoints() const { return static_cast<int32_t>(Points.size()); }
    };

    enum class EPathFollowingStatus : uint8_t { Idle = 0, Moving = 1, Success = 2, Failed = 3 };

    inline const char* PathFollowingStatusName(EPathFollowingStatus InStatus) {
        switch (InStatus) {
        case EPathFollowingStatus::Moving:
            return "Moving";
        case EPathFollowingStatus::Success:
            return "Success";
        case EPathFollowingStatus::Failed:
            return "Failed";
        default:
            return "Idle";
        }
    }

} // namespace Leon
