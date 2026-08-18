#include "Gameplay/FControlInput.hpp"
#include "Core/FInput.hpp"
#include "Core/FInputSettings.hpp"
#include "Engine/FNetBlob.hpp"

#include <cmath>

namespace Leon {

    FControlInput FControlInput::SampleFromHardware() {
        FControlInput input;
        const FInputSettings& settings = FInputSettings::Get();

        if (FInput::IsKeyPressed(settings.MoveForwardKey))
            input.MoveY += 1.0f;
        if (FInput::IsKeyPressed(settings.MoveBackwardKey))
            input.MoveY -= 1.0f;
        if (FInput::IsKeyPressed(settings.MoveLeftKey))
            input.MoveX -= 1.0f;
        if (FInput::IsKeyPressed(settings.MoveRightKey))
            input.MoveX += 1.0f;

        if (settings.bEnableGamepad && FInput::IsGamepadConnected(settings.GamepadId)) {
            auto [lx, ly] = FInput::GetGamepadLeftStick(settings.GamepadId, settings.GamepadDeadzone);
            input.MoveX += lx;
            input.MoveY += -ly;
            if (FInput::IsGamepadButtonPressed(GamepadButton::DPadUp, settings.GamepadId))
                input.MoveY += 1.0f;
            if (FInput::IsGamepadButtonPressed(GamepadButton::DPadDown, settings.GamepadId))
                input.MoveY -= 1.0f;
            if (FInput::IsGamepadButtonPressed(GamepadButton::DPadLeft, settings.GamepadId))
                input.MoveX -= 1.0f;
            if (FInput::IsGamepadButtonPressed(GamepadButton::DPadRight, settings.GamepadId))
                input.MoveX += 1.0f;
        }

        const float len = std::sqrt(input.MoveX * input.MoveX + input.MoveY * input.MoveY);
        if (len > 1.0f) {
            input.MoveX /= len;
            input.MoveY /= len;
        }

        bool bJump = FInput::IsKeyPressed(settings.JumpKey);
        bool bCrouch = FInput::IsKeyPressed(settings.MoveDownKey);
        bool bSprint = FInput::IsKeyPressed(settings.SprintKey);
        if (settings.bEnableGamepad && FInput::IsGamepadConnected(settings.GamepadId)) {
            if (FInput::IsGamepadButtonPressed(GamepadButton::A, settings.GamepadId))
                bJump = true;
            if (FInput::IsGamepadButtonPressed(GamepadButton::LeftBumper, settings.GamepadId))
                bSprint = true;
        }
        input.SetAction(JumpBit, bJump);
        input.SetAction(CrouchBit, bCrouch);
        input.SetAction(SprintBit, bSprint);
        return input;
    }

    void FControlInput::Serialize(std::vector<uint8_t>& OutBytes) const {
        FNetBlob::WriteF32(OutBytes, LookYaw);
        FNetBlob::WriteF32(OutBytes, LookPitch);
        FNetBlob::WriteF32(OutBytes, MoveX);
        FNetBlob::WriteF32(OutBytes, MoveY);
        FNetBlob::WriteU16(OutBytes, ActionBits);
    }

    bool FControlInput::Deserialize(const uint8_t* InData, size_t InSize) {
        if (!InData && InSize > 0)
            return false;
        std::vector<uint8_t> bytes(InData, InData + InSize);
        size_t offset = 0;
        return FNetBlob::ReadF32(bytes, offset, LookYaw) && FNetBlob::ReadF32(bytes, offset, LookPitch) &&
               FNetBlob::ReadF32(bytes, offset, MoveX) && FNetBlob::ReadF32(bytes, offset, MoveY) &&
               FNetBlob::ReadU16(bytes, offset, ActionBits);
    }

} // namespace Leon
