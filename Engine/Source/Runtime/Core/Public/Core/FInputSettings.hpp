#pragma once

#include "Core/FConfigFile.hpp"
#include "Core/FInput.hpp"

#include <string>
#include <unordered_map>

namespace Leon {

    /**
     * @brief Parses Unreal-style key names from DefaultInput.ini ("W", "LeftShift", …).
     */
    struct FKeyName {
        static int FromString(const std::string& InName, int InFallback) {
            static const std::unordered_map<std::string, int> kMap = {
                {"Space", Key::Space},
                {"Escape", Key::Escape},
                {"Enter", Key::Enter},
                {"Tab", Key::Tab},
                {"Backspace", Key::Backspace},
                {"Insert", Key::Insert},
                {"Delete", Key::Delete},
                {"Right", Key::Right},
                {"Left", Key::Left},
                {"Down", Key::Down},
                {"Up", Key::Up},
                {"LeftShift", Key::LeftShift},
                {"RightShift", Key::RightShift},
                {"LeftControl", Key::LeftControl},
                {"RightControl", Key::RightControl},
                {"LeftAlt", Key::LeftAlt},
                {"RightAlt", Key::RightAlt},
                {"A", Key::A},
                {"B", Key::B},
                {"C", Key::C},
                {"D", Key::D},
                {"E", Key::E},
                {"F", Key::F},
                {"G", Key::G},
                {"H", Key::H},
                {"I", Key::I},
                {"J", Key::J},
                {"K", Key::K},
                {"L", Key::L},
                {"M", Key::M},
                {"N", Key::N},
                {"O", Key::O},
                {"P", Key::P},
                {"Q", Key::Q},
                {"R", Key::R},
                {"S", Key::S},
                {"T", Key::T},
                {"U", Key::U},
                {"V", Key::V},
                {"W", Key::W},
                {"X", Key::X},
                {"Y", Key::Y},
                {"Z", Key::Z},
                {"F1", Key::F1},
                {"F2", Key::F2},
                {"F3", Key::F3},
                {"F4", Key::F4},
                {"F5", Key::F5},
                {"F6", Key::F6},
                {"F7", Key::F7},
                {"F8", Key::F8},
                {"F9", Key::F9},
                {"F10", Key::F10},
                {"F11", Key::F11},
                {"F12", Key::F12},
            };
            auto it = kMap.find(InName);
            return it != kMap.end() ? it->second : InFallback;
        }
    };

    /**
     * @brief Movement / look bindings from Config/DefaultInput.ini.
     */
    struct FInputSettings {
        bool bEnableMouseLook = true;
        int MoveForwardKey = Key::W;
        int MoveBackwardKey = Key::S;
        int MoveLeftKey = Key::A;
        int MoveRightKey = Key::D;
        int MoveUpKey = Key::Space;
        int MoveDownKey = Key::LeftControl;
        int SprintKey = Key::LeftShift;

        static constexpr const char* kSection = "/Script/Engine.InputSettings";

        void LoadFromConfig(const FConfigFile& InConfig) {
            bEnableMouseLook = InConfig.GetBool(kSection, "bEnableMouseLook", bEnableMouseLook);
            MoveForwardKey = FKeyName::FromString(InConfig.GetString(kSection, "MoveForwardKey", "W"), MoveForwardKey);
            MoveBackwardKey =
                FKeyName::FromString(InConfig.GetString(kSection, "MoveBackwardKey", "S"), MoveBackwardKey);
            MoveLeftKey = FKeyName::FromString(InConfig.GetString(kSection, "MoveLeftKey", "A"), MoveLeftKey);
            MoveRightKey = FKeyName::FromString(InConfig.GetString(kSection, "MoveRightKey", "D"), MoveRightKey);
            MoveUpKey = FKeyName::FromString(InConfig.GetString(kSection, "MoveUpKey", "Space"), MoveUpKey);
            MoveDownKey = FKeyName::FromString(InConfig.GetString(kSection, "MoveDownKey", "LeftControl"), MoveDownKey);
            SprintKey = FKeyName::FromString(InConfig.GetString(kSection, "SprintKey", "LeftShift"), SprintKey);
        }

        static FInputSettings& GetMutable() {
            static FInputSettings Settings;
            return Settings;
        }

        static const FInputSettings& Get() { return GetMutable(); }

        static void Set(const FInputSettings& InSettings) { GetMutable() = InSettings; }
    };

} // namespace Leon
