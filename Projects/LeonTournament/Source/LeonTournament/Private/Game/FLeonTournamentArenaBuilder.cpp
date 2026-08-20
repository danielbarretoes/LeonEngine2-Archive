#include "FLeonTournamentArenaBuilder.hpp"
#include "Gameplay/FProceduralPrimitiveSpawner.hpp"
#include "Assets/UAssetManager.hpp"
#include "Core/FApplication.hpp"
#include "Engine/Components.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"
#include "Renderer/FWorldRenderer.hpp"

#include <cstdio>

namespace Leon {

    namespace {
        const char* ArenaMaterialPath(ELeonTournamentArenaSurface InSurface) {
            switch (InSurface) {
            case ELeonTournamentArenaSurface::Floor:
                return "/Game/Materials/M_LabFloor.lmat";
            case ELeonTournamentArenaSurface::Wall:
                return "/Game/Materials/M_LabWall.lmat";
            case ELeonTournamentArenaSurface::Metal:
                return "/Game/Materials/M_ArenaMetal.lmat";
            case ELeonTournamentArenaSurface::Accent:
                return "/Game/Materials/M_ArenaAccent.lmat";
            case ELeonTournamentArenaSurface::Ceiling:
                return "/Game/Materials/M_LabCeiling.lmat";
            case ELeonTournamentArenaSurface::Mirror:
                return "/Game/Materials/M_ArenaMirror.lmat";
            case ELeonTournamentArenaSurface::Prop:
            default:
                return "/Game/Materials/M_LabProp.lmat";
            }
        }

        float MetersPerUv(ELeonTournamentArenaSurface InSurface) {
            switch (InSurface) {
            case ELeonTournamentArenaSurface::Floor:
                return 2.0f;
            case ELeonTournamentArenaSurface::Wall:
                return 1.25f;
            case ELeonTournamentArenaSurface::Ceiling:
                return 3.0f;
            case ELeonTournamentArenaSurface::Metal:
            case ELeonTournamentArenaSurface::Prop:
                return 1.5f;
            case ELeonTournamentArenaSurface::Accent:
                return 1.0f;
            case ELeonTournamentArenaSurface::Mirror:
            default:
                return 1.0f;
            }
        }

        AActor* Box(UWorld* W, const char* Name, const glm::vec3& Loc, const glm::vec3& Scale,
                    ELeonTournamentArenaSurface Surf, const glm::vec3& Tint = glm::vec3(1.0f)) {
            return FLeonTournamentArenaBuilder::SpawnBox(W, Name, Loc, Scale, Surf, Tint, MetersPerUv(Surf));
        }
    } // namespace

    AActor* FLeonTournamentArenaBuilder::SpawnBox(UWorld* InWorld, const std::string& InName,
                                                  const glm::vec3& InLocation, const glm::vec3& InScale,
                                                  ELeonTournamentArenaSurface InSurface, const glm::vec3& InTint,
                                                  float InMetersPerUv) {
        const bool planar =
            InSurface == ELeonTournamentArenaSurface::Floor || InSurface == ELeonTournamentArenaSurface::Mirror;
        const bool visibleInReflection = InSurface != ELeonTournamentArenaSurface::Floor;
        const float mpu = InMetersPerUv > 0.0f ? InMetersPerUv : MetersPerUv(InSurface);
        return FProceduralPrimitiveSpawner::SpawnMeshBox(InWorld, InName, InLocation, InScale, InTint,
                                                         ArenaMaterialPath(InSurface), mpu, planar,
                                                         visibleInReflection);
    }

    AActor* FLeonTournamentArenaBuilder::SpawnSimpleBox(UWorld* InWorld, const std::string& InName,
                                                        const glm::vec3& InLocation, const glm::vec3& InScale,
                                                        const glm::vec3& InColor) {
        return FProceduralPrimitiveSpawner::SpawnMeshBox(InWorld, InName, InLocation, InScale, InColor);
    }

    AActor* FLeonTournamentArenaBuilder::SpawnPointLight(UWorld* InWorld, const std::string& InName,
                                                         const glm::vec3& InPos, const glm::vec3& InColor,
                                                         float InIntensity, float InRadius, ELightMobility InMobility) {
        return FProceduralPrimitiveSpawner::SpawnPointLight(InWorld, InName, InPos, InColor, InIntensity, InRadius,
                                                            InMobility);
    }

    AActor* FLeonTournamentArenaBuilder::SpawnSpotLight(UWorld* InWorld, const std::string& InName,
                                                        const glm::vec3& InPos, const glm::vec3& InDir,
                                                        const glm::vec3& InColor, float InIntensity, float InRadius,
                                                        float InInnerDeg, float InOuterDeg, ELightMobility InMobility) {
        return FProceduralPrimitiveSpawner::SpawnSpotLight(InWorld, InName, InPos, InDir, InColor, InIntensity,
                                                           InRadius, InInnerDeg, InOuterDeg, InMobility);
    }

    void FLeonTournamentArenaBuilder::BuildGeometry(UWorld* InWorld, bool bNight) {
        if (!InWorld)
            return;

        const float H = kHalfExtent;
        const float WallH = kWallHeight;
        const float thick = 0.9f;

        // --- Shell ---
        Box(InWorld, "Floor", {0.0f, -0.25f, 0.0f}, {H * 2.0f, 0.5f, H * 2.0f}, ELeonTournamentArenaSurface::Floor);
        Box(InWorld, "Ceiling", {0.0f, kCeilingY, 0.0f}, {H * 2.0f, 0.5f, H * 2.0f},
            ELeonTournamentArenaSurface::Ceiling, {0.9f, 0.92f, 0.96f});
        Box(InWorld, "WallN", {0.0f, WallH * 0.5f, -H}, {H * 2.0f, WallH, thick}, ELeonTournamentArenaSurface::Wall);
        Box(InWorld, "WallS", {0.0f, WallH * 0.5f, H}, {H * 2.0f, WallH, thick}, ELeonTournamentArenaSurface::Wall);
        Box(InWorld, "WallW", {-H, WallH * 0.5f, 0.0f}, {thick, WallH, H * 2.0f}, ELeonTournamentArenaSurface::Wall);
        Box(InWorld, "WallE", {H, WallH * 0.5f, 0.0f}, {thick, WallH, H * 2.0f}, ELeonTournamentArenaSurface::Wall);

        // --- Lane dividers (open sightlines through center atrium) ---
        auto wall = [&](const char* n, const glm::vec3& loc, const glm::vec3& sc) {
            Box(InWorld, n, loc, sc, ELeonTournamentArenaSurface::Wall);
        };
        wall("LaneW_N", {-14.0f, 2.4f, -20.0f}, {0.7f, 4.8f, 14.0f});
        wall("LaneW_S", {-14.0f, 2.4f, 20.0f}, {0.7f, 4.8f, 14.0f});
        wall("LaneE_N", {14.0f, 2.4f, -20.0f}, {0.7f, 4.8f, 14.0f});
        wall("LaneE_S", {14.0f, 2.4f, 20.0f}, {0.7f, 4.8f, 14.0f});
        wall("LaneN_W", {-20.0f, 2.4f, -14.0f}, {14.0f, 4.8f, 0.7f});
        wall("LaneN_E", {20.0f, 2.4f, -14.0f}, {14.0f, 4.8f, 0.7f});
        wall("LaneS_W", {-20.0f, 2.4f, 14.0f}, {14.0f, 4.8f, 0.7f});
        wall("LaneS_E", {20.0f, 2.4f, 14.0f}, {14.0f, 4.8f, 0.7f});

        // --- Cover / props ---
        Box(InWorld, "CoverA", {-22.0f, 1.15f, -22.0f}, {3.8f, 2.3f, 1.5f}, ELeonTournamentArenaSurface::Prop);
        Box(InWorld, "CoverB", {22.0f, 1.15f, 22.0f}, {3.8f, 2.3f, 1.5f}, ELeonTournamentArenaSurface::Metal,
            {0.72f, 0.76f, 0.84f});
        Box(InWorld, "CoverC", {-22.0f, 1.15f, 22.0f}, {1.6f, 2.3f, 3.8f}, ELeonTournamentArenaSurface::Accent);
        Box(InWorld, "CoverD", {22.0f, 1.15f, -22.0f}, {1.6f, 2.3f, 3.8f}, ELeonTournamentArenaSurface::Accent);
        Box(InWorld, "CoverMidW", {-5.0f, 1.1f, 0.0f}, {2.6f, 2.2f, 1.2f}, ELeonTournamentArenaSurface::Metal);
        Box(InWorld, "CoverMidE", {5.0f, 1.1f, 0.0f}, {2.6f, 2.2f, 1.2f}, ELeonTournamentArenaSurface::Metal);
        Box(InWorld, "CoverN", {0.0f, 1.1f, -18.0f}, {4.2f, 2.2f, 1.3f}, ELeonTournamentArenaSurface::Prop);
        Box(InWorld, "CoverS", {0.0f, 1.1f, 18.0f}, {4.2f, 2.2f, 1.3f}, ELeonTournamentArenaSurface::Prop);

        Box(InWorld, "PillarNW", {-18.0f, 3.0f, -18.0f}, {1.3f, 6.0f, 1.3f}, ELeonTournamentArenaSurface::Metal);
        Box(InWorld, "PillarNE", {18.0f, 3.0f, -18.0f}, {1.3f, 6.0f, 1.3f}, ELeonTournamentArenaSurface::Metal);
        Box(InWorld, "PillarSW", {-18.0f, 3.0f, 18.0f}, {1.3f, 6.0f, 1.3f}, ELeonTournamentArenaSurface::Metal);
        Box(InWorld, "PillarSE", {18.0f, 3.0f, 18.0f}, {1.3f, 6.0f, 1.3f}, ELeonTournamentArenaSurface::Metal);

        if (bNight)
            return;

        // --- Day: raised north/south decks + stairs ---
        Box(InWorld, "Floor2_N", {0.0f, 4.6f, -22.0f}, {H * 2.0f - 4.0f, 0.4f, 16.0f},
            ELeonTournamentArenaSurface::Floor, {0.95f, 0.95f, 0.97f});
        Box(InWorld, "Floor2_S", {0.0f, 4.6f, 22.0f}, {H * 2.0f - 4.0f, 0.4f, 16.0f},
            ELeonTournamentArenaSurface::Floor, {0.95f, 0.95f, 0.97f});
        Box(InWorld, "RailN", {0.0f, 5.4f, -14.2f}, {H * 2.0f - 8.0f, 1.0f, 0.35f}, ELeonTournamentArenaSurface::Metal);
        Box(InWorld, "RailS", {0.0f, 5.4f, 14.2f}, {H * 2.0f - 8.0f, 1.0f, 0.35f}, ELeonTournamentArenaSurface::Metal);

        Box(InWorld, "StairsW1", {-26.0f, 1.0f, 0.0f}, {4.0f, 2.0f, 5.0f}, ELeonTournamentArenaSurface::Prop);
        Box(InWorld, "StairsW2", {-26.0f, 2.5f, 0.0f}, {4.0f, 1.5f, 4.0f}, ELeonTournamentArenaSurface::Prop);
        Box(InWorld, "StairsW3", {-26.0f, 3.8f, 0.0f}, {4.0f, 1.2f, 3.0f}, ELeonTournamentArenaSurface::Prop);
        Box(InWorld, "StairsE1", {26.0f, 1.0f, 0.0f}, {4.0f, 2.0f, 5.0f}, ELeonTournamentArenaSurface::Prop);
        Box(InWorld, "StairsE2", {26.0f, 2.5f, 0.0f}, {4.0f, 1.5f, 4.0f}, ELeonTournamentArenaSurface::Prop);
        Box(InWorld, "StairsE3", {26.0f, 3.8f, 0.0f}, {4.0f, 1.2f, 3.0f}, ELeonTournamentArenaSurface::Prop);

        Box(InWorld, "UpperCoverA", {-24.0f, 5.7f, -24.0f}, {3.0f, 2.0f, 1.4f}, ELeonTournamentArenaSurface::Accent);
        Box(InWorld, "UpperCoverB", {24.0f, 5.7f, 24.0f}, {3.0f, 2.0f, 1.4f}, ELeonTournamentArenaSurface::Accent);
    }

    void FLeonTournamentArenaBuilder::BuildLighting(UWorld* InWorld, bool bNight) {
        if (!InWorld)
            return;

        const glm::vec3 warm{1.0f, 0.82f, 0.55f};
        const glm::vec3 cool{0.55f, 0.72f, 1.0f};
        const glm::vec3 soft{1.0f, 0.96f, 0.90f};
        const glm::vec3 sodium{1.0f, 0.65f, 0.28f};

        // Ceiling softbox grid (Static — full bake).
        int idx = 0;
        for (float z = -24.0f; z <= 24.0f + 0.1f; z += 12.0f) {
            for (float x = -24.0f; x <= 24.0f + 0.1f; x += 12.0f) {
                char name[48];
                std::snprintf(name, sizeof(name), "PL_Ceil_%02d", idx++);
                const glm::vec3 color = bNight ? cool : soft;
                const float intensity = bNight ? 6.5f : 11.0f;
                SpawnPointLight(InWorld, name, {x, bNight ? 8.2f : 9.0f, z}, color, intensity, 16.0f,
                                ELightMobility::Static);
            }
        }

        // Corner / lane accents
        SpawnPointLight(InWorld, "PL_NW", {-22.0f, 5.5f, -22.0f}, bNight ? sodium : warm, bNight ? 9.0f : 12.0f, 14.0f);
        SpawnPointLight(InWorld, "PL_NE", {22.0f, 5.5f, -22.0f}, bNight ? sodium : warm, bNight ? 9.0f : 12.0f, 14.0f);
        SpawnPointLight(InWorld, "PL_SW", {-22.0f, 5.5f, 22.0f}, bNight ? sodium : warm, bNight ? 9.0f : 12.0f, 14.0f);
        SpawnPointLight(InWorld, "PL_SE", {22.0f, 5.5f, 22.0f}, bNight ? sodium : warm, bNight ? 9.0f : 12.0f, 14.0f);
        SpawnPointLight(InWorld, "PL_Atrium", {0.0f, 7.5f, 0.0f}, bNight ? cool : soft, bNight ? 8.0f : 10.0f, 18.0f);

        SpawnSpotLight(InWorld, "Spot_Mid", {0.0f, 9.5f, 0.0f}, {0.0f, -1.0f, 0.0f}, bNight ? cool : soft,
                       bNight ? 16.0f : 22.0f, 26.0f, 18.0f, 32.0f);
        SpawnSpotLight(InWorld, "Spot_T1", {-20.0f, 9.0f, -20.0f}, {0.35f, -1.0f, 0.35f}, warm, bNight ? 14.0f : 18.0f,
                       20.0f, 14.0f, 28.0f);
        SpawnSpotLight(InWorld, "Spot_T2", {20.0f, 9.0f, 20.0f}, {-0.35f, -1.0f, -0.35f}, warm, bNight ? 14.0f : 18.0f,
                       20.0f, 14.0f, 28.0f);

        if (!bNight) {
            SpawnPointLight(InWorld, "PL_UpperN", {0.0f, 7.5f, -22.0f}, warm, 9.0f, 14.0f);
            SpawnPointLight(InWorld, "PL_UpperS", {0.0f, 7.5f, 22.0f}, warm, 9.0f, 14.0f);
            SpawnSpotLight(InWorld, "Spot_StairsW", {-26.0f, 8.5f, 0.0f}, {0.2f, -1.0f, 0.0f}, soft, 16.0f, 18.0f,
                           12.0f, 24.0f);
            SpawnSpotLight(InWorld, "Spot_StairsE", {26.0f, 8.5f, 0.0f}, {-0.2f, -1.0f, 0.0f}, soft, 16.0f, 18.0f,
                           12.0f, 24.0f);
        }
    }

    void FLeonTournamentArenaBuilder::PopulateArena(UWorld* InWorld, bool bNight) {
        if (!InWorld)
            return;
        BuildGeometry(InWorld, bNight);
        BuildLighting(InWorld, bNight);

        if (FApplication::HasInstance()) {
            if (auto* renderer = InWorld->GetWorldRenderer()) {
                renderer->ClearPlanarReflectionPlanes();
                renderer->AddPlanarReflectionPlane({0.0f, 1.0f, 0.0f}, 0.0f);
            }
        }

        if (!FApplication::HasInstance())
            return;

        AActor* env = InWorld->FindActorByName("Environment");
        if (!env)
            env = InWorld->SpawnActor<AActor>("Environment");

        FSkyboxComponent sky;
        sky.bEnabled = true;
        sky.bUseHDREnvironmentMap = true;
        if (bNight) {
            sky.Exposure = 1.05f;
            sky.SunIntensity = 1.4f;
            sky.EnvironmentIntensity = 0.85f;
            sky.HDREnvironmentMapPath = "/Game/HDR/NightSky1k.lhdr";
            sky.SkyZenithColor = {0.02f, 0.04f, 0.10f};
            sky.HorizonColor = {0.08f, 0.10f, 0.18f};
            sky.GroundColor = {0.04f, 0.04f, 0.05f};
            sky.SunColor = {0.45f, 0.58f, 0.95f};
        } else {
            sky.Exposure = 0.95f;
            sky.SunIntensity = 2.4f;
            sky.EnvironmentIntensity = 1.35f;
            sky.HDREnvironmentMapPath = "/Game/HDR/DaySky1k.lhdr";
            sky.SkyZenithColor = {0.12f, 0.28f, 0.55f};
            sky.HorizonColor = {0.55f, 0.62f, 0.75f};
            sky.GroundColor = {0.18f, 0.19f, 0.22f};
            sky.SunColor = {1.0f, 0.96f, 0.88f};
        }
        sky.HDREnvironmentMap = UAssetManager::GetTexture2D(sky.HDREnvironmentMapPath);
        if (env->HasComponent<FSkyboxComponent>())
            env->GetComponent<FSkyboxComponent>() = sky;
        else
            env->AddComponent<FSkyboxComponent>(sky);

        FWorldSettingsComponent ws;
        ws.bStaticLighting = true;
        ws.LightingBuildQuality = ELightingBuildQuality::Draft;
        ws.LightmapResolution = 64;
        ws.NumIndirectBounces = 2;
        ws.SamplesPerTexel = 8;
        ws.IndirectIntensity = bNight ? 1.15f : 1.1f;
        ws.bAmbientOcclusion = true;
        ws.LightmapAssetPath =
            bNight ? "/Game/Lightmaps/TournamentArenaNight.llightmap" : "/Game/Lightmaps/TournamentArena.llightmap";
        if (env->HasComponent<FWorldSettingsComponent>())
            env->GetComponent<FWorldSettingsComponent>() = ws;
        else
            env->AddComponent<FWorldSettingsComponent>(ws);

        bool bHasSun = false;
        for (const auto& actor : InWorld->GetAllActors()) {
            if (actor && actor->HasComponent<FDirectionalLightComponent>()) {
                auto& sun = actor->GetComponent<FDirectionalLightComponent>();
                sun.bEnabled = true;
                sun.Mobility = ELightMobility::Stationary;
                sun.Light.Direction =
                    glm::normalize(glm::vec3(bNight ? 0.28f : -0.35f, -1.0f, bNight ? -0.32f : -0.45f));
                sun.Light.Color = bNight ? glm::vec3(0.45f, 0.58f, 0.95f) : glm::vec3(1.0f, 0.97f, 0.90f);
                sun.Light.Intensity = bNight ? 1.6f : 3.0f;
                bHasSun = true;
                break;
            }
        }
        if (!bHasSun) {
            AActor* sunActor = InWorld->SpawnActor<AActor>(bNight ? "Moonlight" : "Directional Sunlight");
            sunActor->SetActorLocation({0.0f, 14.0f, 0.0f});
            FDirectionalLightComponent sun;
            sun.bEnabled = true;
            sun.Mobility = ELightMobility::Stationary;
            sun.Light.Direction = glm::normalize(glm::vec3(bNight ? 0.28f : -0.35f, -1.0f, bNight ? -0.32f : -0.45f));
            sun.Light.Color = bNight ? glm::vec3(0.45f, 0.58f, 0.95f) : glm::vec3(1.0f, 0.97f, 0.90f);
            sun.Light.Intensity = bNight ? 1.6f : 3.0f;
            sunActor->AddComponent<FDirectionalLightComponent>(sun);
        }
    }

} // namespace Leon
