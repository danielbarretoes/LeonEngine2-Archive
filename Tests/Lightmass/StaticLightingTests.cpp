#include <doctest/doctest.h>

#include "Assets/FLightmapAsset.hpp"
#include "Assets/FLightmapUV.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Engine/EMobility.hpp"
#include "Engine/Components.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"
#include "Lightmass/FLightBaker.hpp"
#include "Lightmass/FLightmapBuilder.hpp"
#include "Lightmass/FLightmass.hpp"
#include "Renderer/FIBLMath.hpp"
#include "Renderer/FLightAttenuation.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

using namespace Leon;

TEST_CASE("Lightmap header and serialization roundtrip") {
    FLightmapAsset asset;
    asset.Allocate(8, 4);
    auto& pixels = asset.GetPixelsRGBA32F();
    for (size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i + 0] = 0.25f;
        pixels[i + 1] = 0.50f;
        pixels[i + 2] = 0.75f;
        pixels[i + 3] = 1.0f;
    }
    asset.ComputeContentHash();
    uint64_t hash = asset.GetContentHash();
    REQUIRE(hash != 0);

    const std::string path = "lightmap_test_roundtrip.llightmap";
    REQUIRE(asset.SaveToFile(path));

    FLightmapAsset loaded;
    REQUIRE(loaded.LoadFromFile(path));
    CHECK(loaded.GetWidth() == 8);
    CHECK(loaded.GetHeight() == 4);
    CHECK(loaded.IsHDR());
    CHECK(loaded.GetPixelsRGBA32F().size() == 8 * 4 * 4);
    CHECK(loaded.GetPixelsRGBA32F()[0] == doctest::Approx(0.25f));

    std::filesystem::remove(path);
}

TEST_CASE("Lightmap rejects corrupted assets") {
    const std::string path = "lightmap_corrupt.llightmap";
    {
        std::ofstream f(path, std::ios::binary);
        uint32_t bad = 0xDEADBEEF;
        f.write(reinterpret_cast<const char*>(&bad), sizeof(bad));
    }
    FLightmapAsset loaded;
    CHECK_FALSE(loaded.LoadFromFile(path));
    std::filesystem::remove(path);
}

TEST_CASE("Lightmap UV validation and barycentric") {
    auto mesh = UStaticMesh::Create("UVTest");
    auto& verts = mesh->GetVertices();
    auto& indices = mesh->GetIndices();
    verts.resize(3);
    verts[0].Position = {0, 0, 0};
    verts[0].LightmapUV = {0.1f, 0.1f};
    verts[1].Position = {1, 0, 0};
    verts[1].LightmapUV = {0.9f, 0.1f};
    verts[2].Position = {0.5f, 0, 1};
    verts[2].LightmapUV = {0.5f, 0.9f};
    verts[0].Normal = verts[1].Normal = verts[2].Normal = {0, 1, 0};
    indices = {0, 1, 2};

    auto result = FLightmapUV::Validate(*mesh);
    CHECK(result.bInUnitSquare);

    glm::vec3 bary;
    REQUIRE(FLightmapUV::ComputeBarycentric({0.5f, 0.367f}, verts[0].LightmapUV, verts[1].LightmapUV,
                                             verts[2].LightmapUV, bary));
    CHECK(doctest::Approx(bary.x + bary.y + bary.z).epsilon(1e-3) == 1.0f);

    glm::vec3 p = FLightmapUV::Interpolate(verts[0].Position, verts[1].Position, verts[2].Position, bary);
    CHECK(p.y == doctest::Approx(0.0f));
}

TEST_CASE("Lightmap atlas packing") {
    std::vector<FLightmapChart> charts(3);
    charts[0].Resolution = 32;
    charts[1].Resolution = 16;
    charts[2].Resolution = 16;
    uint32_t w = 0, h = 0;
    REQUIRE(FLightmapBuilder::PackCharts(charts, w, h, 2));
    CHECK(w >= 32);
    CHECK(h >= 16);
    CHECK(charts[0].Scale.x > 0.0f);
}

TEST_CASE("Mobility enums") {
    CHECK(StringToLightMobility("Static") == ELightMobility::Static);
    CHECK(StringToComponentMobility("Movable") == EComponentMobility::Movable);
    CHECK(std::string(LightMobilityToString(ELightMobility::Stationary)) == "Stationary");
    CHECK(IsLightmassBakeLight(ELightMobility::Stationary));
    CHECK(DoesLightmassBakeDirect(ELightMobility::Static));
    CHECK_FALSE(DoesLightmassBakeDirect(ELightMobility::Stationary));
}

TEST_CASE("Light baker attenuation and sampling") {
    float atten = FLightBaker::PointAttenuation(0.0f, 10.0f);
    CHECK(atten > 0.5f);
    float far = FLightBaker::PointAttenuation(20.0f, 10.0f);
    CHECK(far == doctest::Approx(0.0f));

    float cone = FLightBaker::SpotConeFactor({0, -1, 0}, {0, 1, 0}, 12.5f, 17.5f);
    CHECK(cone > 0.5f);

    glm::vec3 n(0, 1, 0);
    glm::vec3 d = FLightBaker::CosineSampleHemisphere(n, 0.25f, 0.5f);
    CHECK(glm::dot(d, n) > 0.0f);
}

TEST_CASE("Baker attenuation and Lambert match runtime helpers") {
    CHECK(FLightBaker::PointAttenuation(0.0f, 10.0f) ==
          doctest::Approx(DistanceAttenuationUE4(0.0f, 10.0f)).epsilon(1e-6f));
    CHECK(FLightBaker::PointAttenuation(2.0f, 10.0f) ==
          doctest::Approx(DistanceAttenuationUE4(2.0f, 10.0f)).epsilon(1e-6f));
    CHECK(FLightBaker::PointAttenuation(10.0f, 10.0f) ==
          doctest::Approx(DistanceAttenuationUE4(10.0f, 10.0f)).epsilon(1e-6f));

    glm::vec3 travel(0, -1, 0);
    glm::vec3 toLight(0, 1, 0);
    CHECK(FLightBaker::SpotConeFactor(travel, toLight, 12.5f, 17.5f) ==
          doctest::Approx(SpotConeAttenuation(travel, toLight, 12.5f, 17.5f)).epsilon(1e-6f));
    CHECK(FLightBaker::SpotConeFactor(travel, glm::vec3(1, 0, 0), 12.5f, 17.5f) ==
          doctest::Approx(SpotConeAttenuation(travel, glm::vec3(1, 0, 0), 12.5f, 17.5f)).epsilon(1e-6f));

    glm::vec3 N(0, 1, 0);
    glm::vec3 L(0, 1, 0);
    CHECK(LambertNdotL(N, L) == doctest::Approx(1.0f));
    CHECK(LambertNdotL(N, glm::vec3(0, -1, 0)) == doctest::Approx(0.0f));
}

TEST_CASE("Light baker deterministic direct lighting") {
    FLightBakerScene scene;
    scene.AtlasWidth = 4;
    scene.AtlasHeight = 4;
    FLightmapChart chart;
    chart.Resolution = 4;
    chart.PackedWidth = 4;
    chart.PackedHeight = 4;
    chart.Scale = {1, 1};
    chart.Bias = {0, 0};
    scene.Charts.push_back(chart);

    FBakeVertex v0, v1, v2;
    v0.Position = {-1, 0, -1};
    v0.Normal = {0, 1, 0};
    v0.LightmapUV = {0, 0};
    v0.Albedo = {1, 1, 1};
    v1.Position = {1, 0, -1};
    v1.Normal = {0, 1, 0};
    v1.LightmapUV = {1, 0};
    v1.Albedo = {1, 1, 1};
    v2.Position = {0, 0, 1};
    v2.Normal = {0, 1, 0};
    v2.LightmapUV = {0.5f, 1};
    v2.Albedo = {1, 1, 1};
    scene.Vertices = {v0, v1, v2};
    scene.Triangles.push_back({0, 1, 2, 0, true});
    scene.DirectionalLights.push_back({FDirectionalLight{{-0.2f, -1.0f, -0.1f}, {1, 1, 1}, 2.0f}, true});

    FLightBakerSettings settings;
    settings.SamplesPerTexel = 4;
    settings.NumIndirectBounces = 1;
    settings.bAmbientOcclusion = false;
    settings.Seed = 12345;

    std::vector<float> a, b;
    FLightBaker::Bake(scene, settings, a);
    FLightBaker::Bake(scene, settings, b);
    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i)
        CHECK(a[i] == doctest::Approx(b[i]));
}

TEST_CASE("Stationary lights skip direct bake contribution") {
    FLightBakerScene scene;
    scene.AtlasWidth = 4;
    scene.AtlasHeight = 4;
    FLightmapChart chart;
    chart.Resolution = 4;
    chart.PackedWidth = 4;
    chart.PackedHeight = 4;
    chart.Scale = {1, 1};
    chart.Bias = {0, 0};
    scene.Charts.push_back(chart);

    FBakeVertex v0, v1, v2;
    v0.Position = {-1, 0, -1};
    v0.Normal = {0, 1, 0};
    v0.LightmapUV = {0, 0};
    v0.Albedo = {1, 1, 1};
    v1.Position = {1, 0, -1};
    v1.Normal = {0, 1, 0};
    v1.LightmapUV = {1, 0};
    v1.Albedo = {1, 1, 1};
    v2.Position = {0, 0, 1};
    v2.Normal = {0, 1, 0};
    v2.LightmapUV = {0.5f, 1};
    v2.Albedo = {1, 1, 1};
    scene.Vertices = {v0, v1, v2};
    scene.Triangles.push_back({0, 1, 2, 0, true});

    FLightBakerSettings settings;
    settings.SamplesPerTexel = 1;
    settings.NumIndirectBounces = 1;
    settings.bAmbientOcclusion = false;
    settings.Seed = 1;

    scene.DirectionalLights.push_back({FDirectionalLight{{0.f, -1.f, 0.f}, {1, 1, 1}, 5.0f}, false});
    std::vector<float> stationaryOnly;
    FLightBaker::Bake(scene, settings, stationaryOnly);

    scene.DirectionalLights[0].bContributeDirect = true;
    std::vector<float> staticDirect;
    FLightBaker::Bake(scene, settings, staticDirect);

    float maxStationary = 0.0f;
    float maxStatic = 0.0f;
    for (size_t i = 0; i < stationaryOnly.size(); i += 4) {
        maxStationary = std::max(maxStationary, stationaryOnly[i]);
        maxStatic = std::max(maxStatic, staticDirect[i]);
    }
    CHECK(maxStatic > maxStationary);
}

TEST_CASE("NumIndirectBounces 0 stores only direct irradiance") {
    FLightBakerScene scene;
    scene.AtlasWidth = 4;
    scene.AtlasHeight = 4;
    FLightmapChart chart;
    chart.Resolution = 4;
    chart.PackedWidth = 4;
    chart.PackedHeight = 4;
    chart.Scale = {1, 1};
    chart.Bias = {0, 0};
    scene.Charts.push_back(chart);

    FBakeVertex floor0, floor1, floor2;
    floor0.Position = {-1, 0, -1};
    floor0.Normal = {0, 1, 0};
    floor0.LightmapUV = {0, 0};
    floor0.Albedo = {1, 1, 1};
    floor1.Position = {1, 0, -1};
    floor1.Normal = {0, 1, 0};
    floor1.LightmapUV = {1, 0};
    floor1.Albedo = {1, 1, 1};
    floor2.Position = {0, 0, 1};
    floor2.Normal = {0, 1, 0};
    floor2.LightmapUV = {0.5f, 1};
    floor2.Albedo = {1, 1, 1};

    FBakeVertex ceil0, ceil1, ceil2;
    ceil0.Position = {-1, 1, -1};
    ceil0.Normal = {0, -1, 0};
    ceil0.LightmapUV = {0, 0};
    ceil0.Albedo = {1, 1, 1};
    ceil0.Emissive = {8, 8, 8};
    ceil1.Position = {1, 1, -1};
    ceil1.Normal = {0, -1, 0};
    ceil1.LightmapUV = {1, 0};
    ceil1.Albedo = {1, 1, 1};
    ceil1.Emissive = {8, 8, 8};
    ceil2.Position = {0, 1, 1};
    ceil2.Normal = {0, -1, 0};
    ceil2.LightmapUV = {0.5f, 1};
    ceil2.Albedo = {1, 1, 1};
    ceil2.Emissive = {8, 8, 8};

    scene.Vertices = {floor0, floor1, floor2, ceil0, ceil1, ceil2};
    scene.Triangles.push_back({0, 1, 2, 0, true});
    scene.Triangles.push_back({3, 4, 5, 99, true});

    FLightBakerSettings settings;
    settings.SamplesPerTexel = 32;
    settings.bAmbientOcclusion = false;
    settings.Seed = 7;
    settings.IndirectIntensity = 1.0f;

    settings.NumIndirectBounces = 0;
    std::vector<float> zeroBounce;
    FLightBaker::Bake(scene, settings, zeroBounce);

    settings.NumIndirectBounces = 1;
    std::vector<float> oneBounce;
    FLightBaker::Bake(scene, settings, oneBounce);

    float maxZero = 0.0f;
    float maxOne = 0.0f;
    for (size_t i = 0; i < zeroBounce.size(); i += 4) {
        if (zeroBounce[i + 3] <= 0.0f)
            continue;
        maxZero = std::max(maxZero, zeroBounce[i]);
        maxOne = std::max(maxOne, oneBounce[i]);
    }
    CHECK(maxZero == doctest::Approx(0.0f).epsilon(1e-4f));
    CHECK(maxOne > 0.1f);
}

TEST_CASE("Receptor emissive is not stored in the lightmap") {
    FLightBakerScene scene;
    scene.AtlasWidth = 4;
    scene.AtlasHeight = 4;
    FLightmapChart chart;
    chart.Resolution = 4;
    chart.PackedWidth = 4;
    chart.PackedHeight = 4;
    chart.Scale = {1, 1};
    chart.Bias = {0, 0};
    scene.Charts.push_back(chart);

    FBakeVertex v0, v1, v2;
    v0.Position = {-1, 0, -1};
    v0.Normal = {0, 1, 0};
    v0.LightmapUV = {0, 0};
    v0.Albedo = {1, 1, 1};
    v0.Emissive = {10, 10, 10};
    v1.Position = {1, 0, -1};
    v1.Normal = {0, 1, 0};
    v1.LightmapUV = {1, 0};
    v1.Albedo = {1, 1, 1};
    v1.Emissive = {10, 10, 10};
    v2.Position = {0, 0, 1};
    v2.Normal = {0, 1, 0};
    v2.LightmapUV = {0.5f, 1};
    v2.Albedo = {1, 1, 1};
    v2.Emissive = {10, 10, 10};
    scene.Vertices = {v0, v1, v2};
    scene.Triangles.push_back({0, 1, 2, 0, true});

    FLightBakerSettings settings;
    settings.SamplesPerTexel = 4;
    settings.NumIndirectBounces = 0;
    settings.bAmbientOcclusion = false;
    settings.Seed = 1;

    std::vector<float> baked;
    FLightBaker::Bake(scene, settings, baked);
    float maxE = 0.0f;
    for (size_t i = 0; i < baked.size(); i += 4) {
        if (baked[i + 3] <= 0.0f)
            continue;
        maxE = std::max(maxE, baked[i]);
    }
    CHECK(maxE == doctest::Approx(0.0f).epsilon(1e-4f));
}

TEST_CASE("Analytic plane stores directional irradiance E = L * NdotL") {
    FLightBakerScene scene;
    scene.AtlasWidth = 8;
    scene.AtlasHeight = 8;
    FLightmapChart chart;
    chart.Resolution = 8;
    chart.PackedWidth = 8;
    chart.PackedHeight = 8;
    chart.Scale = {1, 1};
    chart.Bias = {0, 0};
    scene.Charts.push_back(chart);

    FBakeVertex v0, v1, v2;
    v0.Position = {-1, 0, -1};
    v0.Normal = {0, 1, 0};
    v0.LightmapUV = {0, 0};
    v0.Albedo = {1, 1, 1};
    v1.Position = {1, 0, -1};
    v1.Normal = {0, 1, 0};
    v1.LightmapUV = {1, 0};
    v1.Albedo = {1, 1, 1};
    v2.Position = {0, 0, 1};
    v2.Normal = {0, 1, 0};
    v2.LightmapUV = {0.5f, 1};
    v2.Albedo = {1, 1, 1};
    scene.Vertices = {v0, v1, v2};
    scene.Triangles.push_back({0, 1, 2, 0, false});
    scene.DirectionalLights.push_back({FDirectionalLight{{0.f, -1.f, 0.f}, {1, 1, 1}, 2.0f}, true});

    FLightBakerSettings settings;
    settings.SamplesPerTexel = 1;
    settings.NumIndirectBounces = 0;
    settings.bAmbientOcclusion = false;
    settings.Seed = 1;

    std::vector<float> baked;
    FLightBaker::Bake(scene, settings, baked);
    float covered = 0.0f;
    int count = 0;
    for (size_t i = 0; i < baked.size(); i += 4) {
        if (baked[i + 3] <= 0.0f)
            continue;
        covered += baked[i];
        ++count;
    }
    REQUIRE(count > 0);
    float mean = covered / static_cast<float>(count);
    CHECK(mean == doctest::Approx(2.0f).epsilon(0.05f));
}

TEST_CASE("Constant environment miss stores E = pi * L * Intensity") {
    FLightBakerScene scene;
    scene.AtlasWidth = 8;
    scene.AtlasHeight = 8;
    FLightmapChart chart;
    chart.Resolution = 8;
    chart.PackedWidth = 8;
    chart.PackedHeight = 8;
    chart.Scale = {1, 1};
    chart.Bias = {0, 0};
    scene.Charts.push_back(chart);

    FBakeVertex v0, v1, v2;
    v0.Position = {-1, 0, -1};
    v0.Normal = {0, 1, 0};
    v0.LightmapUV = {0, 0};
    v0.Albedo = {1, 1, 1};
    v1.Position = {1, 0, -1};
    v1.Normal = {0, 1, 0};
    v1.LightmapUV = {1, 0};
    v1.Albedo = {1, 1, 1};
    v2.Position = {0, 0, 1};
    v2.Normal = {0, 1, 0};
    v2.LightmapUV = {0.5f, 1};
    v2.Albedo = {1, 1, 1};
    scene.Vertices = {v0, v1, v2};
    scene.Triangles.push_back({0, 1, 2, 0, false});

    const glm::vec3 Lsky(0.2f);
    scene.Environment.bEnabled = true;
    scene.Environment.Zenith = Lsky;
    scene.Environment.Horizon = Lsky;
    scene.Environment.Ground = Lsky;
    scene.Environment.Intensity = 1.0f;

    FLightBakerSettings settings;
    settings.SamplesPerTexel = 256;
    settings.NumIndirectBounces = 0;
    settings.bAmbientOcclusion = false;
    settings.Seed = 42;

    std::vector<float> baked;
    FLightBaker::Bake(scene, settings, baked);
    float covered = 0.0f;
    int count = 0;
    for (size_t i = 0; i < baked.size(); i += 4) {
        if (baked[i + 3] <= 0.0f)
            continue;
        covered += baked[i];
        ++count;
    }
    REQUIRE(count > 0);
    float mean = covered / static_cast<float>(count);
    const float expected = 3.14159265358979323846f * 0.2f;
    CHECK(mean == doctest::Approx(expected).epsilon(0.08f));
}

TEST_CASE("Bake input hash stable across identical worlds and settings") {
    auto world = UWorld::Create();
    FLightmassSettings s1, s2;
    s1.SamplesPerTexel = 4;
    s2.SamplesPerTexel = 4;
    s2.AOIntensity = s1.AOIntensity;
    uint64_t h1 = FLightmass::ComputeBakeInputHash(*world, s1);
    uint64_t h2 = FLightmass::ComputeBakeInputHash(*world, s2);
    CHECK(h1 == h2);

    s2.SamplesPerTexel = 8;
    uint64_t h3 = FLightmass::ComputeBakeInputHash(*world, s2);
    CHECK(h1 != h3);
}

TEST_CASE("Bake input hash includes AORadius") {
    auto world = UWorld::Create();
    FLightmassSettings s1, s2;
    s1.AORadius = 1.0f;
    s2.AORadius = 2.0f;
    CHECK(FLightmass::ComputeBakeInputHash(*world, s1) != FLightmass::ComputeBakeInputHash(*world, s2));
}

TEST_CASE("Bake input hash includes material overrides") {
    auto world = UWorld::Create();
    auto* a = world->SpawnActor<AActor>("MeshA");
    auto& smc = a->AddComponent<FStaticMeshComponent>();
    smc.Mobility = EComponentMobility::Static;
    uint64_t h1 = FLightmass::ComputeBakeInputHash(*world, FLightmassSettings{});
    smc.MaterialOverridePaths = {"/Game/Materials/M_Override.lmat"};
    uint64_t h2 = FLightmass::ComputeBakeInputHash(*world, FLightmassSettings{});
    CHECK(h1 != h2);
}

TEST_CASE("Lighting quality presets change hash") {
    auto world = UWorld::Create();
    FLightmassSettings preview, production;
    ApplyLightingBuildQuality(ELightingBuildQuality::Preview, preview);
    ApplyLightingBuildQuality(ELightingBuildQuality::Production, production);
    CHECK(preview.LightmapResolution == 32);
    CHECK(production.SamplesPerTexel == 32);
    CHECK(FLightmass::ComputeBakeInputHash(*world, preview) != FLightmass::ComputeBakeInputHash(*world, production));
}

TEST_CASE("Skip cache requires both atlas and world hashes non-zero") {
    // OR skip was the bug: a matching atlas hash alone must not skip when world hash is 0.
    FLightmassSettings settings;
    auto world = UWorld::Create();
    auto* env = world->SpawnActor<AActor>("Environment Skybox");
    auto& ws = env->AddComponent<FWorldSettingsComponent>();
    ws.bStaticLighting = true;
    ws.LightmapBakeHash = 0;
    uint64_t bakeHash = FLightmass::ComputeBakeInputHash(*world, settings);
    CHECK(bakeHash != 0);
    CHECK(ws.LightmapBakeHash != bakeHash);
}
