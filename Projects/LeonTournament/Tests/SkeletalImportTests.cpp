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
        const std::string meshFbx = "Projects/LeonTournament/Raw/YBot/Y Bot.fbx";
        const std::string idleFbx = "Projects/LeonTournament/Raw/Anim/Idle.fbx";
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
        CHECK(meshResult.SkeletalMesh->HasMeshBindPoses());
        CHECK(meshResult.SkeletalMesh->GetInverseBindPoses().size() == meshResult.Skeleton->GetNumBones());

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
    }

    TEST_CASE("Patrick keeps own skeleton; YBot anims link by bone name") {
        const std::string ybotFbx = "Projects/LeonTournament/Raw/YBot/Y Bot.fbx";
        const std::string patrickFbx = "Projects/LeonTournament/Raw/Patric/Patrick.fbx";
        const std::string idleFbx = "Projects/LeonTournament/Raw/Anim/Idle.fbx";
        REQUIRE(fs::exists(ybotFbx));
        REQUIRE(fs::exists(patrickFbx));
        REQUIRE(fs::exists(idleFbx));

        FMeshImportResult patrick;
        REQUIRE(FMeshImporter::ImportFBX(patrickFbx, {}, patrick));
        REQUIRE(patrick.SkeletalMesh);
        REQUIRE(patrick.Skeleton);
        CHECK(patrick.SkeletalMesh->HasMeshBindPoses());
        CHECK(patrick.SkeletalMesh->GetInverseBindPoses().size() == patrick.Skeleton->GetNumBones());
        CHECK(patrick.SkeletalMesh->GetVertices().size() > 100);

        // Rest palette must stay near identity (own skeleton rest × matching IBPs).
        FPose rest;
        FAnimRuntime::RestPose(*patrick.Skeleton, rest);
        std::vector<glm::mat4> component, palette;
        FAnimRuntime::LocalToComponent(*patrick.Skeleton, rest, component);
        FAnimRuntime::BuildSkinningPalette(*patrick.Skeleton, component, patrick.SkeletalMesh->GetInverseBindPoses(),
                                           palette);
        REQUIRE(palette.size() == patrick.Skeleton->GetNumBones());
        float identErr = 0.0f;
        for (size_t b = 0; b < palette.size(); ++b) {
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    identErr += std::abs(palette[b][c][r] - (c == r ? 1.0f : 0.0f));
        }
        CHECK(identErr < 0.5f);

        // Shared-skeleton mesh retarget would make rest palette explode — guard against regressing.
        FMeshImportResult ybot;
        REQUIRE(FMeshImporter::ImportFBX(ybotFbx, {}, ybot));
        REQUIRE(ybot.Skeleton);
        FMeshImportSettings badSettings;
        badSettings.SharedSkeleton = ybot.Skeleton;
        FMeshImportResult badPatrick;
        REQUIRE(FMeshImporter::ImportFBX(patrickFbx, badSettings, badPatrick));
        REQUIRE(badPatrick.SkeletalMesh);
        FPose yRest;
        FAnimRuntime::RestPose(*ybot.Skeleton, yRest);
        std::vector<glm::mat4> yComp, badPalette;
        FAnimRuntime::LocalToComponent(*ybot.Skeleton, yRest, yComp);
        FAnimRuntime::BuildSkinningPalette(*ybot.Skeleton, yComp, badPatrick.SkeletalMesh->GetInverseBindPoses(),
                                           badPalette);
        float badErr = 0.0f;
        for (size_t b = 0; b < badPalette.size(); ++b) {
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    badErr += std::abs(badPalette[b][c][r] - (c == r ? 1.0f : 0.0f));
        }
        CHECK(badErr > 10.0f);

        FMeshImportSettings idleSettings;
        idleSettings.SharedSkeleton = ybot.Skeleton;
        FMeshImportResult idle;
        REQUIRE(FMeshImporter::ImportFBX(idleFbx, idleSettings, idle));
        REQUIRE(!idle.Animations.empty());
        idle.Animations[0]->LinkSkeleton(patrick.Skeleton);

        FPose pose;
        FAnimRuntime::SampleSequence(*idle.Animations[0], *patrick.Skeleton, 0.05f, true, pose);
        // Non-root bones must keep Patrick bind lengths (YBot Idle keys must not crush proportions).
        float lengthErr = 0.0f;
        int checked = 0;
        for (size_t b = 0; b < patrick.Skeleton->GetNumBones(); ++b) {
            const auto& bone = patrick.Skeleton->GetBones()[b];
            if (bone.ParentIndex < 0)
                continue;
            const float restLen = glm::length(bone.RestLocal.Translation);
            const float poseLen = glm::length(pose.LocalTransforms[b].Translation);
            lengthErr += std::abs(restLen - poseLen);
            ++checked;
        }
        CHECK(checked > 5);
        CHECK(lengthErr < 0.05f);

        FAnimRuntime::LocalToComponent(*patrick.Skeleton, pose, component);
        FAnimRuntime::BuildSkinningPalette(*patrick.Skeleton, component, patrick.SkeletalMesh->GetInverseBindPoses(),
                                           palette);
        CHECK(palette.size() == patrick.Skeleton->GetNumBones());
        float idlePaletteErr = 0.0f;
        for (size_t b = 0; b < palette.size(); ++b) {
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    idlePaletteErr += std::abs(palette[b][c][r]);
        }
        // Collapsed retarget produced huge matrices; a sane idle palette stays bounded.
        CHECK(idlePaletteErr < 5000.0f);
    }
}
