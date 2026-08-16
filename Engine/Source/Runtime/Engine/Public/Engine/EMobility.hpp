#pragma once

#include <cstdint>
#include <string>

namespace Leon {

    /**
     * @brief Light contribution mode for static lighting (Unreal-style mobility).
     * Stationary: baked indirect only; direct + shadows remain dynamic at runtime.
     */
    enum class ELightMobility : uint8_t {
        Static = 0,     ///< Full Lightmass bake (direct + indirect); excluded from dynamic UBO
        Stationary = 1, ///< Lightmass indirect only; dynamic direct + shadows at runtime
        Movable = 2     ///< Fully dynamic; ignored by Lightmass
    };

    inline bool IsLightmassBakeLight(ELightMobility InMobility) {
        return InMobility == ELightMobility::Static || InMobility == ELightMobility::Stationary;
    }

    inline bool DoesLightmassBakeDirect(ELightMobility InMobility) {
        return InMobility == ELightMobility::Static;
    }

    /**
     * @brief Component / geometry mobility for static lighting receivers.
     */
    enum class EComponentMobility : uint8_t {
        Static = 0,
        Stationary = 1,
        Movable = 2
    };

    inline const char* LightMobilityToString(ELightMobility InValue) {
        switch (InValue) {
        case ELightMobility::Static:
            return "Static";
        case ELightMobility::Stationary:
            return "Stationary";
        case ELightMobility::Movable:
            return "Movable";
        default:
            return "Movable";
        }
    }

    inline ELightMobility StringToLightMobility(const std::string& InStr) {
        if (InStr == "Static")
            return ELightMobility::Static;
        if (InStr == "Stationary")
            return ELightMobility::Stationary;
        return ELightMobility::Movable;
    }

    inline const char* ComponentMobilityToString(EComponentMobility InValue) {
        switch (InValue) {
        case EComponentMobility::Static:
            return "Static";
        case EComponentMobility::Stationary:
            return "Stationary";
        case EComponentMobility::Movable:
            return "Movable";
        default:
            return "Movable";
        }
    }

    inline EComponentMobility StringToComponentMobility(const std::string& InStr) {
        if (InStr == "Static")
            return EComponentMobility::Static;
        if (InStr == "Stationary")
            return EComponentMobility::Stationary;
        return EComponentMobility::Movable;
    }

} // namespace Leon
