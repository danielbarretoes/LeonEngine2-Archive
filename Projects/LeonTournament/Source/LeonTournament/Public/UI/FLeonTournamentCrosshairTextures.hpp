#pragma once

#include "FLeonTournamentTypes.hpp"
#include "RHI/FTexture.hpp"

namespace Leon {

    class UImage;

    /** Returns a cached procedural RGBA crosshair texture for the weapon. */
    TRef<FTexture2D> LeonTournamentGetCrosshairTexture(ELeonTournamentWeaponId InId);

    /** Applies weapon config path override or the procedural crosshair to a HUD image. */
    void LeonTournamentApplyCrosshairBrush(UImage& InImage, ELeonTournamentWeaponId InId,
                                         const FLeonTournamentWeaponConfig& InConfig);

} // namespace Leon
