#pragma once

#include <cstdint>
#include <vector>

namespace Leon {

    /**
     * Standard listen-server control blob: analog move, look, and action bits.
     * Engine owns Jump/Crouch/Sprint. Games OR CustomBit0+ for held actions (e.g. fire).
     * Discrete actions (reload, weapon cycle) should use ServerRPC instead of bits.
     */
    struct FControlInput {
        static constexpr uint16_t JumpBit = 1u << 0;
        static constexpr uint16_t CrouchBit = 1u << 1;
        static constexpr uint16_t SprintBit = 1u << 2;
        static constexpr uint16_t CustomBit0 = 1u << 8;

        float MoveX = 0.0f;
        float MoveY = 0.0f;
        float LookYaw = 0.0f;
        float LookPitch = 0.0f;
        uint16_t ActionBits = 0;

        bool HasAction(uint16_t InBit) const { return (ActionBits & InBit) != 0; }
        void SetAction(uint16_t InBit, bool bOn) {
            if (bOn)
                ActionBits |= InBit;
            else
                ActionBits = static_cast<uint16_t>(ActionBits & ~InBit);
        }

        /** WASD / stick / jump / crouch / sprint from FInputSettings. Look is filled by the pawn. */
        static FControlInput SampleFromHardware();

        void Serialize(std::vector<uint8_t>& OutBytes) const;
        bool Deserialize(const uint8_t* InData, size_t InSize);
    };

} // namespace Leon
