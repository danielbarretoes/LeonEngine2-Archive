#pragma once

#include "core/Base.hpp"
#include "renderer/Texture.hpp"
#include "renderer/TextureCube.hpp"
#include "scene/Components.hpp"

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

    using IBLGenerator = FIBLGenerator;

} // namespace Leon
