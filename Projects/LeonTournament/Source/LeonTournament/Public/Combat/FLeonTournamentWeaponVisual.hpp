#pragma once

#include "FLeonTournamentTypes.hpp"

#include <string>

namespace Leon {

    enum class ELeonTournamentWeaponMeshKind : uint8_t { Cylinder = 0, Cube = 1, Sphere = 2 };

    struct FLeonTournamentWeaponVisual {
        ELeonTournamentWeaponMeshKind MeshKind = ELeonTournamentWeaponMeshKind::Cylinder;
        float Radius = 0.045f;
        float TopRadius = 0.045f;
        float Length = 0.36f;
        float CubeSize = 0.22f;
    };

    /** Third-person / world silhouette (compact). */
    inline FLeonTournamentWeaponVisual LeonTournamentWeaponVisualPreset(ELeonTournamentWeaponId InId) {
        FLeonTournamentWeaponVisual visual;
        switch (InId) {
        case ELeonTournamentWeaponId::Shotgun:
            visual.MeshKind = ELeonTournamentWeaponMeshKind::Cube;
            visual.CubeSize = 0.28f;
            break;
        case ELeonTournamentWeaponId::Rocket:
            visual.Radius = 0.07f;
            visual.TopRadius = 0.05f;
            visual.Length = 0.52f;
            break;
        case ELeonTournamentWeaponId::Laser:
            visual.Radius = 0.028f;
            visual.TopRadius = 0.028f;
            visual.Length = 0.48f;
            break;
        case ELeonTournamentWeaponId::Flamethrower:
            visual.MeshKind = ELeonTournamentWeaponMeshKind::Cube;
            visual.CubeSize = 0.32f;
            break;
        case ELeonTournamentWeaponId::Rifle:
        default:
            break;
        }
        return visual;
    }

    /** First-person view mesh (larger, readable at arm's length). */
    inline FLeonTournamentWeaponVisual LeonTournamentWeaponFirstPersonVisualPreset(ELeonTournamentWeaponId InId) {
        FLeonTournamentWeaponVisual visual = LeonTournamentWeaponVisualPreset(InId);
        switch (InId) {
        case ELeonTournamentWeaponId::Shotgun:
            visual.MeshKind = ELeonTournamentWeaponMeshKind::Cube;
            visual.CubeSize = 0.44f;
            break;
        case ELeonTournamentWeaponId::Rocket:
            visual.Radius = 0.085f;
            visual.TopRadius = 0.06f;
            visual.Length = 0.68f;
            break;
        case ELeonTournamentWeaponId::Laser:
            visual.Radius = 0.038f;
            visual.TopRadius = 0.038f;
            visual.Length = 0.62f;
            break;
        case ELeonTournamentWeaponId::Flamethrower:
            visual.MeshKind = ELeonTournamentWeaponMeshKind::Cube;
            visual.CubeSize = 0.48f;
            break;
        case ELeonTournamentWeaponId::Rifle:
        default:
            visual.Radius = 0.058f;
            visual.TopRadius = 0.048f;
            visual.Length = 0.58f;
            break;
        }
        return visual;
    }

    inline std::string LeonTournamentWeaponMeshTag(ELeonTournamentWeaponId InId, bool bFirstPerson = false) {
        return std::string("Weapon:") + LeonTournamentWeaponName(InId) + (bFirstPerson ? ":FP" : ":TP");
    }

    inline std::string LeonTournamentWeaponMeshTagLegacy(ELeonTournamentWeaponId InId) {
        return std::string("Weapon:") + LeonTournamentWeaponName(InId);
    }

} // namespace Leon
