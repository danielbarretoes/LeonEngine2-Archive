#pragma once

#include "FLeonTournamentTypes.hpp"

#include <string>

namespace Leon {

    /** Per-weapon audio paths and mix. */
    struct FLeonTournamentWeaponAudio {
        std::string FirePath = "/Game/Audio/SFX_RifleFire";
        std::string ReloadPath = "/Game/Audio/SFX_RifleReload";
        float FireVolume = 0.95f;
        float ReloadVolume = 0.7f;
        float FireAttenuationRadius = 4500.0f;
    };

    inline FLeonTournamentWeaponAudio LeonTournamentWeaponAudioPreset(ELeonTournamentWeaponId InId) {
        FLeonTournamentWeaponAudio audio;
        switch (InId) {
        case ELeonTournamentWeaponId::Shotgun:
            audio.FirePath = "/Game/Audio/SFX_ShotgunFire";
            audio.ReloadPath = "/Game/Audio/SFX_ShotgunReload";
            audio.FireVolume = 1.05f;
            audio.FireAttenuationRadius = 5200.0f;
            break;
        case ELeonTournamentWeaponId::Rocket:
            audio.FirePath = "/Game/Audio/SFX_RocketFire";
            audio.ReloadPath = "/Game/Audio/SFX_RocketReload";
            audio.FireVolume = 1.15f;
            audio.ReloadVolume = 0.78f;
            audio.FireAttenuationRadius = 5500.0f;
            break;
        case ELeonTournamentWeaponId::Laser:
            audio.FirePath = "/Game/Audio/SFX_LaserFire";
            audio.ReloadPath = "/Game/Audio/SFX_LaserReload";
            audio.FireVolume = 0.85f;
            audio.ReloadVolume = 0.65f;
            audio.FireAttenuationRadius = 4200.0f;
            break;
        case ELeonTournamentWeaponId::Flamethrower:
            audio.FirePath = "/Game/Audio/SFX_FlamethrowerFire";
            audio.ReloadPath = "/Game/Audio/SFX_FlamethrowerReload";
            audio.FireVolume = 0.55f;
            audio.ReloadVolume = 0.6f;
            audio.FireAttenuationRadius = 3800.0f;
            break;
        case ELeonTournamentWeaponId::Rifle:
        default:
            break;
        }
        return audio;
    }

} // namespace Leon
