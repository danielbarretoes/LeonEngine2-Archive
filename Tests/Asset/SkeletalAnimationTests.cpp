#include <doctest/doctest.h>

#include "Assets/FAnimRuntime.hpp"
#include "Assets/FMeshImporter.hpp"
#include "Assets/UAnimSequence.hpp"
#include "Assets/UBlendSpace.hpp"
#include "Assets/USkeleton.hpp"
#include "Assets/USkeletalMesh.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/FAnimStateMachine.hpp"
#include "Gameplay/UAnimInstance.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/Components.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace Leon;
namespace fs = std::filesystem;

namespace {

    TRef<USkeleton> MakeTwoBoneSkeleton() {
        auto skel = USkeleton::Create("TwoBone");
        FSkeletonBone root;
        root.Name = "root";
        root.ParentIndex = -1;
        root.RestLocal.Translation = glm::vec3(0.0f);
        root.InverseBindPose = glm::mat4(1.0f);

        FSkeletonBone child;
        child.Name = "spine";
        child.ParentIndex = 0;
        child.RestLocal.Translation = glm::vec3(0.0f, 1.0f, 0.0f);
        child.InverseBindPose = glm::inverse(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)));

        skel->GetBones() = {root, child};
        skel->RebuildLookup();
        return skel;
    }

    TRef<UAnimSequence> MakeSpinAnim(const TRef<USkeleton>& InSkel) {
        auto seq = UAnimSequence::Create("Spin");
        seq->SetDuration(1.0f);
        seq->SetSampleRate(30.0f);
        FAnimBoneTrack track;
        track.BoneName = "spine";
        track.RotationKeys.push_back({0.0f, glm::quat(1, 0, 0, 0)});
        track.RotationKeys.push_back({1.0f, glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0))});
        seq->GetTracks().push_back(track);
        seq->LinkSkeleton(InSkel);
        return seq;
    }

} // namespace

TEST_SUITE("Skeletal Animation") {

    TEST_CASE("Skeleton lookup and upper-body mask") {
        auto skel = MakeTwoBoneSkeleton();
        CHECK(skel->FindBoneIndex("spine") == 1);
        CHECK(skel->FindBoneIndex("mixamorig:spine") == 1);
        auto mask = skel->BuildUpperBodyMask();
        REQUIRE(mask.size() == 2);
        CHECK(mask[0] == doctest::Approx(0.0f));
        CHECK(mask[1] == doctest::Approx(1.0f));
    }

    TEST_CASE("Rest pose skinning palette is identity for bind") {
        auto skel = MakeTwoBoneSkeleton();
        FPose rest;
        FAnimRuntime::RestPose(*skel, rest);
        std::vector<glm::mat4> component;
        FAnimRuntime::LocalToComponent(*skel, rest, component);
        std::vector<glm::mat4> palette;
        FAnimRuntime::BuildSkinningPalette(*skel, component, palette);
        REQUIRE(palette.size() == 2);
        glm::mat4 ident(1.0f);
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                CHECK(palette[1][c][r] == doctest::Approx(ident[c][r]).epsilon(1e-4f));
    }

    TEST_CASE("Sequence sample and pose blend are deterministic") {
        auto skel = MakeTwoBoneSkeleton();
        auto seq = MakeSpinAnim(skel);
        FPose a, b, c;
        FAnimRuntime::SampleSequence(*seq, *skel, 0.5f, true, a);
        FAnimRuntime::SampleSequence(*seq, *skel, 0.5f, true, b);
        CHECK(a.LocalTransforms[1].Rotation.w == doctest::Approx(b.LocalTransforms[1].Rotation.w));
        FAnimRuntime::BlendPoses(a, a, 0.3f, c);
        CHECK(c.LocalTransforms[1].Rotation.w == doctest::Approx(a.LocalTransforms[1].Rotation.w));
    }

    TEST_CASE("BlendSpace 1D interpolates neighboring samples") {
        auto skel = MakeTwoBoneSkeleton();
        auto idle = MakeSpinAnim(skel);
        auto run = MakeSpinAnim(skel);
        auto bs = UBlendSpace::Create("Locomotion");
        bs->Set2D(false);
        bs->SetAxisRange({0.0f, 0.0f}, {600.0f, 0.0f});
        bs->AddSample({0.0f, 0.0f}, idle);
        bs->AddSample({300.0f, 0.0f}, run);
        std::vector<float> w;
        bs->EvaluateWeights({150.0f, 0.0f}, w);
        REQUIRE(w.size() == 2);
        CHECK(w[0] == doctest::Approx(0.5f));
        CHECK(w[1] == doctest::Approx(0.5f));
    }

    TEST_CASE("BlendSpace 2D IDW and .lblend roundtrip") {
        auto skel = MakeTwoBoneSkeleton();
        auto a = MakeSpinAnim(skel);
        auto b = MakeSpinAnim(skel);
        auto bs = UBlendSpace::Create("Loco2D");
        bs->Set2D(true);
        bs->SetAxisRange({0.0f, -180.0f}, {600.0f, 180.0f});
        bs->SetSkeletonPath("/Game/Skeletons/Test.lskeleton");
        a->SetAssetPath("/Game/Animations/A.lanim");
        b->SetAssetPath("/Game/Animations/B.lanim");
        bs->AddSample({0.0f, 0.0f}, a);
        bs->AddSample({300.0f, 90.0f}, b);
        std::vector<float> w;
        bs->EvaluateWeights({0.0f, 0.0f}, w);
        REQUIRE(w.size() == 2);
        CHECK(w[0] > w[1]);

        const std::string path = "build/test.lblend";
        CHECK(bs->SaveToFile(path));
        auto loaded = UBlendSpace::Create("Loaded");
        CHECK(loaded->LoadFromFile(path));
        CHECK(loaded->Is2D());
        CHECK(loaded->GetSamples().size() == 2);
        CHECK(loaded->GetSamples()[1].SequencePath == "/Game/Animations/B.lanim");
        fs::remove(path);
    }

    TEST_CASE("Layered blend applies bone mask") {
        auto skel = MakeTwoBoneSkeleton();
        FPose base = skel->GetRestPose();
        FPose overlay = skel->GetRestPose();
        overlay.LocalTransforms[1].Translation = glm::vec3(0.0f, 5.0f, 0.0f);
        std::vector<float> mask = {0.0f, 1.0f};
        FPose out;
        FAnimRuntime::LayeredBlend(base, overlay, mask, out);
        CHECK(out.LocalTransforms[0].Translation.y == doctest::Approx(0.0f));
        CHECK(out.LocalTransforms[1].Translation.y == doctest::Approx(5.0f));
    }

    TEST_CASE("Binary roundtrip .lskeleton .lskeletalmesh .lanim") {
        auto skel = MakeTwoBoneSkeleton();
        fs::create_directories("build");
        const std::string skelPath = "build/test.lskeleton";
        CHECK(skel->SaveToFile(skelPath));
        auto loadedSkel = USkeleton::Create("Loaded");
        CHECK(loadedSkel->LoadFromFile(skelPath));
        CHECK(loadedSkel->GetNumBones() == 2);
        CHECK(loadedSkel->GetBones()[1].Name == "spine");

        auto mesh = USkeletalMesh::Create("Skinned");
        mesh->SetSkeleton(skel);
        FSkinnedMeshVertex v;
        v.Position = glm::vec3(0, 1, 0);
        v.BoneIndices = glm::ivec4(1, 0, 0, 0);
        v.BoneWeights = glm::vec4(1, 0, 0, 0);
        mesh->GetVertices() = {v, v, v};
        mesh->GetIndices() = {0, 1, 2};
        FSkeletalSubmesh sub;
        sub.Name = "Body";
        sub.IndexCount = 3;
        sub.VertexCount = 3;
        mesh->GetSubmeshes().push_back(sub);
        mesh->CalculateBounds();
        const std::string meshPath = "build/test.lskeletalmesh";
        CHECK(mesh->SaveToFile(meshPath));
        auto loadedMesh = USkeletalMesh::Create("LoadedMesh");
        CHECK(loadedMesh->LoadFromFile(meshPath));
        CHECK(loadedMesh->GetVertices().size() == 3);
        CHECK(loadedMesh->GetVertices()[0].BoneIndices.x == 1);

        auto seq = MakeSpinAnim(skel);
        const std::string animPath = "build/test.lanim";
        CHECK(seq->SaveToFile(animPath));
        auto loadedAnim = UAnimSequence::Create("LoadedAnim");
        CHECK(loadedAnim->LoadFromFile(animPath));
        CHECK(loadedAnim->GetTracks().size() == 1);
        CHECK(loadedAnim->GetDuration() == doctest::Approx(1.0f));

        fs::remove(skelPath);
        fs::remove(meshPath);
        fs::remove(animPath);
    }

    TEST_CASE("AnimInstance graph uses generic parameters") {
        auto skel = MakeTwoBoneSkeleton();
        auto seq = MakeSpinAnim(skel);
        auto inst = MakeRef<UAnimInstance>("Inst");
        inst->Initialize(skel);

        FAnimState idle;
        idle.Name = "Idle";
        idle.Sequence = seq;
        idle.bLoop = true;
        inst->GetStateMachine().AddState(idle);

        FAnimState move;
        move.Name = "Locomotion";
        move.Sequence = seq;
        move.bLoop = true;
        inst->GetStateMachine().AddState(move);
        inst->GetStateMachine().AddTransition({"Idle", "Locomotion", "FloatGreater:Speed:15", 0.12f});
        inst->GetStateMachine().AddTransition({"Locomotion", "Idle", "FloatLessEqual:Speed:15", 0.12f});
        inst->GetStateMachine().SetDefaultState("Idle");
        inst->GetStateMachine().Reset();

        inst->SetFloat("Speed", 0.0f);
        inst->NativeUpdateAnimation(0.016f);
        CHECK(inst->GetStateMachine().GetCurrentState() == "Idle");
        inst->SetFloat("Speed", 80.0f);
        inst->NativeUpdateAnimation(0.2f);
        CHECK(inst->GetStateMachine().GetCurrentState() == "Locomotion");
        FPose pose;
        inst->Evaluate(pose);
        CHECK(pose.Num() == 2);
    }

    TEST_CASE("ACharacter writes anim rep state from movement") {
        auto world = UWorld::Create("AnimWorld");
        auto* character = world->SpawnActor<ACharacter>("Hero");
        REQUIRE(character);
        character->SetFloorZ(0.0f);
        character->SetEyeHeight(1.7f);
        character->SetActorLocation({0.0f, 1.7f, 0.0f});
        character->Tick(0.016f);
        CHECK(character->GetMesh() != nullptr);
        CHECK(character->HasComponent<FSkeletalMeshComponent>());
        CHECK(character->GetAnimRepState().Speed >= 0.0f);
    }

    TEST_CASE("ATestCharacter derives ACharacter without game systems") {
        class ATestCharacter : public ACharacter {
        public:
            ATestCharacter(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "TestCharacter")
                : ACharacter(InHandle, InWorld, InName) {}
        };
        auto world = UWorld::Create("TestCharWorld");
        auto* character = world->SpawnActor<ATestCharacter>("Tester");
        REQUIRE(character);
        character->Tick(0.016f);
        CHECK(character->GetMesh() != nullptr);
        auto anim = character->GetMesh()->GetOrCreateAnimInstance();
        REQUIRE(anim);
        character->UpdateAnimInstance(*anim);
        CHECK(anim->GetFloat("Speed") >= 0.0f);
        CHECK_FALSE(anim->GetBool("bIsDead"));
    }

    TEST_CASE("FBX Mixamo Y Bot and Idle import when present") {
        const std::string meshFbx = "Projects/LeonTournament/Raw/Y Bot.fbx";
        const std::string idleFbx = "Projects/LeonTournament/Raw/Idle.fbx";
        if (!fs::exists(meshFbx) || !fs::exists(idleFbx)) {
            MESSAGE("Mixamo FBX not in workspace — skipping importer test.");
            return;
        }

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
