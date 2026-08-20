#pragma once

#include <leon/core/Paths.h>
#include <leon/level/Level.h>
#include <leon/level/Light.h>
#include <leon/render/ResourceCache.h>
#include <string>

namespace leon::editor {

/// Default HDR used by isolated editor previews (mesh / asset / material).
inline constexpr const char* kDefaultPreviewHdrRel = "Hdr/AutumnFieldPuresky1k.hdr";

/// Studio-like sun for preview Levels (brighter than world default; no shadows).
[[nodiscard]] inline DirectionalLight MakePreviewSun() {
    DirectionalLight sun{};
    // Match blank-level sun aim so lit faces read clearly under orbit camera.
    sun.transform.rotationDegrees = {50.0f, -30.0f, 0.0f};
    sun.lightColor = {1.0f, 0.98f, 0.95f};
    sun.intensity = 3.25f;
    // Single-mesh shadow maps often self-shadow and crush albedo.
    sun.castShadows = false;
    return sun;
}

inline void ApplyDefaultPreviewSun(Level& level) {
    level.DirectionalLights().clear();
    level.DirectionalLights().push_back(MakePreviewSun());
}

/// Skybox + IBL for preview Levels (DrawScene already renders EnvMap when set).
inline void ApplyDefaultPreviewEnvironment(Level& level, ResourceCache& resources) {
    const std::string hdrPath = ResolveAssetPath(kDefaultPreviewHdrRel);
    if (hdrPath.empty()) {
        return;
    }
    auto env = resources.LoadEnvMap(hdrPath);
    if (env == nullptr) {
        return;
    }
    level.SetEnvironment(std::move(env));
    level.SetEnvironmentPath(kDefaultPreviewHdrRel);
    level.SetEnvironmentExposure(1.15f);
}

/// Sun + skybox for an isolated preview Level.
inline void ApplyDefaultPreviewLighting(Level& level, ResourceCache& resources) {
    ApplyDefaultPreviewSun(level);
    ApplyDefaultPreviewEnvironment(level, resources);
}

} // namespace leon::editor
