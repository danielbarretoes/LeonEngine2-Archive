#pragma once

#include <string>

namespace Leon {

    /**
     * Engine built-in content (Unreal-style /Engine/... assets).
     * Writes missing files under Engine/Resources and registers them in UAssetManager.
     */
    struct FEngineBuiltins {
        static constexpr const char* kWorldGridMaterial = "Engine/Materials/M_WorldGrid.lmat";
        static constexpr const char* kWorldGridTexture = "Engine/Textures/T_WorldGrid.tga";
        static constexpr const char* kMeshCube = "Engine/Meshes/Cube.lmesh";
        static constexpr const char* kMeshSphere = "Engine/Meshes/Sphere.lmesh";
        static constexpr const char* kMeshCylinder = "Engine/Meshes/Cylinder.lmesh";
        static constexpr const char* kMeshPlane = "Engine/Meshes/Plane.lmesh";

        /** Create on-disk assets if missing, then register textures/materials/meshes in the asset manager. */
        static void EnsureAndRegister();
    };

} // namespace Leon
