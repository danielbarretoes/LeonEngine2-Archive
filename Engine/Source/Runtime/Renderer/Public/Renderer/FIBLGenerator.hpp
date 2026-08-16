#pragma once

#include "Core/Base.hpp"
#include "RHI/FTexture.hpp"
#include "RHI/FTextureCube.hpp"
#include "Engine/Components.hpp"

namespace Leon {

    struct FIBLEnvironment {
        TRef<FTextureCube> EnvironmentCubemap;
        TRef<FTextureCube> IrradianceMap;
        TRef<FTextureCube> PrefilterMap;
        TRef<FTexture2D> BRDFLUT;
    };

    /**
     * @brief High-fidelity Image-Based Lighting (IBL) generator implementing Cook-Torrance
     * Split-Sum approximation (Karis / Unreal Engine Standard).
     */
    class FIBLGenerator {
    public:
        static TRef<FTexture2D> GenerateBRDFLUT(uint32_t InSize = 512);
        static FIBLEnvironment CreateEnvironmentFromSkybox(const FSkyboxComponent& InSkybox);
    };

} // namespace Leon
