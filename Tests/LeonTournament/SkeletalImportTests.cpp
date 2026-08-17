#include <doctest/doctest.h>

#include "Assets/FAnimRuntime.hpp"
#include "Assets/FMeshImporter.hpp"
#include "Assets/UAnimSequence.hpp"
#include "Assets/USkeletalMesh.hpp"
#include "Assets/USkeleton.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>

using namespace Leon;
namespace fs = std::filesystem;

TEST_SUITE("LeonTournament skeletal import") {

    TEST_CASE("FBX Mixamo Y Bot and Idle import") {
        const std::string meshFbx = "Projects/LeonTournament/Raw/Y Bot.fbx";
        const std::string idleFbx = "Projects/LeonTournament/Raw/Idle.fbx";
        REQUIRE(fs::exists(meshFbx));
        REQUIRE(fs::exists(idleFbx));

        FMeshImportSettings settings;
        FMeshImportResult meshResult;
        REQUIRE(FMeshImporter::ImportFBX(meshFbx, settings, meshResult));
        REQUIRE(meshResult.Skeleton);
        CHECK(meshResult.Skeleton->GetNumBones() > 10);
        REQUIRE(meshResult.SkeletalMesh);
        CHECK(meshResult.SkeletalMesh->GetVertices().size() > 100);
        CHECK(meshResult.SkeletalMesh->GetVertices()[0].BoneWeights.x +
                  meshResult.SkeletalMesh->GetVertices()[0].BoneWeights.y +
                  meshResult.SkeletalMesh->GetVertices()[0].BoneWeights.z +
                  meshResult.SkeletalMesh->GetVertices()[0].BoneWeights.w ==
              doctest::Approx(1.0f).epsilon(1e-3f));

        FMeshImportResult idleResult;
        FMeshImportSettings idleSettings;
        idleSettings.SharedSkeleton = meshResult.Skeleton;
        REQUIRE(FMeshImporter::ImportFBX(idleFbx, idleSettings, idleResult));
        REQUIRE(!idleResult.Animations.empty());
        CHECK(idleResult.Skeleton.get() == meshResult.Skeleton.get());
        CHECK(idleResult.Animations[0]->GetTracks().size() == meshResult.Skeleton->GetNumBones());
        idleResult.Animations[0]->LinkSkeleton(meshResult.Skeleton);
        const auto& trackMap = idleResult.Animations[0]->GetTrackToBone();
        int mapped = 0;
        for (int32_t bone : trackMap) {
            if (bone >= 0)
                ++mapped;
        }
        CHECK(mapped == meshResult.Skeleton->GetNumBones());

        FPose pose;
        FAnimRuntime::SampleSequence(*idleResult.Animations[0], *meshResult.Skeleton, 0.0f, true, pose);
        CHECK(pose.Num() == meshResult.Skeleton->GetNumBones());
        std::vector<glm::mat4> component, palette;
        FAnimRuntime::LocalToComponent(*meshResult.Skeleton, pose, component);
        FAnimRuntime::BuildSkinningPalette(*meshResult.Skeleton, component, palette);
        CHECK(palette.size() == meshResult.Skeleton->GetNumBones());

        float minY = 1e9f, maxY = -1e9f;
        for (const auto& v : meshResult.SkeletalMesh->GetVertices()) {
            minY = std::min(minY, v.Position.y);
            maxY = std::max(maxY, v.Position.y);
        }
        CHECK(maxY - minY >= 1.5f);
        CHECK(maxY - minY <= 2.1f);

        FPose rest;
        FAnimRuntime::RestPose(*meshResult.Skeleton, rest);
        std::vector<glm::mat4> restComp, restPalette;
        FAnimRuntime::LocalToComponent(*meshResult.Skeleton, rest, restComp);
        FAnimRuntime::BuildSkinningPalette(*meshResult.Skeleton, restComp, restPalette);
        REQUIRE(!restPalette.empty());
        float identErr = 0.0f;
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                identErr += std::abs(restPalette[0][c][r] - (c == r ? 1.0f : 0.0f));
        CHECK(identErr < 0.15f);
        CHECK(glm::length(meshResult.Skeleton->GetBones()[0].RestLocal.Translation) < 3.0f);
    }
}
