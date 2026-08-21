#pragma once

#include "Core/FStringUtils.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/ACameraActor.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/ADefaultPawn.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/AHUD.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerStart.hpp"
#include "Gameplay/APlayerState.hpp"
#include "Gameplay/AWorldSettings.hpp"
#include "Engine/Components.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace Leon {

    inline void Indent(std::stringstream& ss, int level) {
        for (int i = 0; i < level; ++i)
            ss << "  ";
    }

    inline std::string StripQuotes(const std::string& str) {
        std::string s = FStringUtils::Trim(str);
        if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
            return s.substr(1, s.size() - 2);
        }
        return s;
    }

    inline std::string LightingQualityToString(ELightingBuildQuality InQuality) {
        switch (InQuality) {
        case ELightingBuildQuality::Preview:
            return "Preview";
        case ELightingBuildQuality::Production:
            return "Production";
        default:
            return "Draft";
        }
    }

    inline ELightingBuildQuality StringToLightingQuality(const std::string& InStr) {
        if (InStr == "Preview")
            return ELightingBuildQuality::Preview;
        if (InStr == "Production")
            return ELightingBuildQuality::Production;
        return ELightingBuildQuality::Draft;
    }

    inline bool IsRuntimeFrameworkActor(const AActor& InActor) {
        return dynamic_cast<const AGameModeBase*>(&InActor) || dynamic_cast<const AGameStateBase*>(&InActor) ||
               dynamic_cast<const APlayerController*>(&InActor) || dynamic_cast<const APlayerState*>(&InActor) ||
               dynamic_cast<const AHUD*>(&InActor) || dynamic_cast<const APlayerCameraManager*>(&InActor) ||
               dynamic_cast<const ADefaultPawn*>(&InActor) || dynamic_cast<const ACharacter*>(&InActor) ||
               dynamic_cast<const AWorldSettings*>(&InActor);
    }

    inline void WriteWorldSettings(std::stringstream& ss, const FWorldSettingsComponent& ws) {
        Indent(ss, 1);
        ss << "WorldSettings:\n";
        if (!ws.GameModeClass.empty()) {
            Indent(ss, 2);
            ss << "GameModeClass: \"" << ws.GameModeClass << "\"\n";
        }
        Indent(ss, 2);
        ss << "StaticLighting: " << (ws.bStaticLighting ? "true" : "false") << "\n";
        Indent(ss, 2);
        ss << "LightingBuildQuality: " << LightingQualityToString(ws.LightingBuildQuality) << "\n";
        Indent(ss, 2);
        ss << "LightmapResolution: " << ws.LightmapResolution << "\n";
        Indent(ss, 2);
        ss << "NumIndirectBounces: " << ws.NumIndirectBounces << "\n";
        Indent(ss, 2);
        ss << "SamplesPerTexel: " << ws.SamplesPerTexel << "\n";
        Indent(ss, 2);
        ss << "IndirectIntensity: " << ws.IndirectIntensity << "\n";
        Indent(ss, 2);
        ss << "AmbientOcclusion: " << (ws.bAmbientOcclusion ? "true" : "false") << "\n";
        Indent(ss, 2);
        ss << "AOIntensity: " << ws.AOIntensity << "\n";
        Indent(ss, 2);
        ss << "AORadius: " << ws.AORadius << "\n";
        Indent(ss, 2);
        ss << "TexelPadding: " << ws.TexelPadding << "\n";
        Indent(ss, 2);
        ss << "WorldScale: " << ws.WorldScale << "\n";
        if (!ws.LightmapAssetPath.empty()) {
            Indent(ss, 2);
            ss << "LightmapAsset: \"" << ws.LightmapAssetPath << "\"\n";
        }
        if (ws.LightmapBakeHash != 0) {
            Indent(ss, 2);
            ss << "LightmapBakeHash: \"" << std::hex << ws.LightmapBakeHash << std::dec << "\"\n";
        }
    }

    inline std::vector<float> ParseFloatArray(const std::string& valStr) {
        std::vector<float> result;
        std::string s = FStringUtils::Trim(valStr);
        if (!s.empty() && s.front() == '[' && s.back() == ']') {
            s = s.substr(1, s.size() - 2);
        }
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, ',')) {
            item = FStringUtils::Trim(item);
            if (!item.empty()) {
                try {
                    result.push_back(std::stof(item));
                } catch (...) {
                    result.push_back(0.0f);
                }
            }
        }
        return result;
    }

} // namespace Leon
