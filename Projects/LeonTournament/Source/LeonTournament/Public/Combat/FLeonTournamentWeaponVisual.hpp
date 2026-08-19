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

    inline std::string LeonTournamentWeaponMeshTag(ELeonTournamentWeaponId InId) {
        return std::string("Weapon:") + LeonTournamentWeaponName(InId);
    }

} // namespace Leon
