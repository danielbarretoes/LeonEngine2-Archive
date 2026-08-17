#include "ULeonTournamentAnimInstance.hpp"
#include "Assets/UAssetManager.hpp"

#include <cmath>
#include <filesystem>

namespace Leon {

    namespace {
        TRef<UAnimSequence> LoadAnim(const TRef<USkeleton>& InSkeleton, const std::string& InPath) {
            auto seq = UAssetManager::GetAnimSequence(InPath);
            if (seq && InSkeleton)
                seq->LinkSkeleton(InSkeleton);
            return seq;
        }
    } // namespace

    ULeonTournamentAnimInstance::ULeonTournamentAnimInstance(const std::string& InName) : UAnimInstance(InName) {}

    TRef<UAnimSequence> ULeonTournamentAnimInstance::LoadLinked(const std::string& InPath) {
        return LoadAnim(Skeleton, InPath);
    }

    TRef<UBlendSpace> ULeonTournamentAnimInstance::BuildLocomotionBlendSpace(const TRef<USkeleton>& InSkeleton) {
        auto bs = UBlendSpace::Create("BS_Locomotion");
        bs->Set2D(true);
        bs->SetAxisRange({0.0f, -180.0f}, {1200.0f, 180.0f});
        bs->SetSkeletonPath("/Game/Skeletons/YBot.lskeleton");
        bs->SetAssetPath("/Game/BlendSpaces/BS_Locomotion.lblend");

        auto add = [&](float speed, float direction, const std::string& path) {
            auto seq = LoadAnim(InSkeleton, path);
            if (seq)
                bs->AddSample({speed, direction}, seq);
        };

        add(0.0f, 0.0f, "/Game/Animations/Idle.lanim");
        add(0.0f, 90.0f, "/Game/Animations/Idle.lanim");
        add(0.0f, -90.0f, "/Game/Animations/Idle.lanim");
        add(0.0f, 180.0f, "/Game/Animations/Idle.lanim");
        add(0.0f, -180.0f, "/Game/Animations/Idle.lanim");

        add(180.0f, 0.0f, "/Game/Animations/RunForward.lanim");
        add(180.0f, 45.0f, "/Game/Animations/WalkForwardRight.lanim");
        add(180.0f, 90.0f, "/Game/Animations/RunRight.lanim");
        add(180.0f, 135.0f, "/Game/Animations/RunBackwardRight.lanim");
        add(180.0f, 180.0f, "/Game/Animations/RunBackward.lanim");
        add(180.0f, -180.0f, "/Game/Animations/RunBackward.lanim");
        add(180.0f, -135.0f, "/Game/Animations/RunBackwardLeft.lanim");
        add(180.0f, -90.0f, "/Game/Animations/RunLeft.lanim");
        add(180.0f, -45.0f, "/Game/Animations/RunForwardLeft.lanim");

        add(500.0f, 0.0f, "/Game/Animations/RunForward.lanim");
        add(500.0f, 45.0f, "/Game/Animations/WalkForwardRight.lanim");
        add(500.0f, 90.0f, "/Game/Animations/RunRight.lanim");
        add(500.0f, 135.0f, "/Game/Animations/RunBackwardRight.lanim");
        add(500.0f, 180.0f, "/Game/Animations/RunBackward.lanim");
        add(500.0f, -180.0f, "/Game/Animations/RunBackward.lanim");
        add(500.0f, -135.0f, "/Game/Animations/RunBackwardLeft.lanim");
        add(500.0f, -90.0f, "/Game/Animations/RunLeft.lanim");
        add(500.0f, -45.0f, "/Game/Animations/RunForwardLeft.lanim");
        return bs;
    }

    bool ULeonTournamentAnimInstance::LocomotionBlendCoversEightDirections(const UBlendSpace& InBlend) {
        if (!InBlend.Is2D() || InBlend.GetSamples().empty())
            return false;

        const float kDirs[8] = {0.0f, 45.0f, 90.0f, 135.0f, 180.0f, -135.0f, -90.0f, -45.0f};
        bool hasIdle = false;
        bool hasDir[8] = {};
        for (const auto& sample : InBlend.GetSamples()) {
            if (!sample.Sequence)
                return false;
            if (sample.Coord.x > 600.0f)
                return false;
            if (sample.Coord.x <= 1.0f)
                hasIdle = true;
            if (sample.Coord.x < 400.0f)
                continue;
            for (int i = 0; i < 8; ++i) {
                float d = sample.Coord.y - kDirs[i];
                if (d > 180.0f)
                    d -= 360.0f;
                if (d < -180.0f)
                    d += 360.0f;
                if (std::abs(d) <= 6.0f)
                    hasDir[i] = true;
            }
        }
        if (!hasIdle)
            return false;
        for (bool ok : hasDir) {
            if (!ok)
                return false;
        }
        return true;
    }

    void ULeonTournamentAnimInstance::NativeInitializeAnimation() {
        if (bGraphBuilt || !Skeleton)
            return;

        LocomotionBlend = nullptr;
        const std::string disk = UAssetManager::ResolveVirtualPath("/Game/BlendSpaces/BS_Locomotion.lblend");
        if (!disk.empty() && std::filesystem::exists(disk))
            LocomotionBlend = UAssetManager::GetBlendSpace("/Game/BlendSpaces/BS_Locomotion.lblend");

        bool bSamplesReady = false;
        if (LocomotionBlend && !LocomotionBlend->GetSamples().empty()) {
            LocomotionBlend->ResolveSequences();
            bSamplesReady = LocomotionBlendCoversEightDirections(*LocomotionBlend);
            if (bSamplesReady) {
                for (auto& sample : LocomotionBlend->GetSamples())
                    sample.Sequence->LinkSkeleton(Skeleton);
            }
        }
        if (!bSamplesReady) {
            LocomotionBlend = BuildLocomotionBlendSpace(Skeleton);
            if (!disk.empty()) {
                std::filesystem::create_directories(std::filesystem::path(disk).parent_path());
                LocomotionBlend->SaveToFile(disk);
            }
            UAssetManager::AddBlendSpace("/Game/BlendSpaces/BS_Locomotion.lblend", LocomotionBlend);
        }

        auto jumpUp = LoadLinked("/Game/Animations/JumpUp.lanim");
        auto jumpLoop = LoadLinked("/Game/Animations/JumpLoop.lanim");
        auto jumpDown = LoadLinked("/Game/Animations/JumpDown.lanim");
        DeathSequence = LoadLinked("/Game/Animations/DeathFromTheFront.lanim");
        if (DeathSequence)
            DeathSequence->SetLooping(false);
        if (jumpUp)
            jumpUp->SetLooping(false);
        if (jumpDown)
            jumpDown->SetLooping(false);

        StateMachine = FAnimStateMachine{};
        SetBlendParamNames("Speed", "Direction");

        FAnimState loco;
        loco.Name = "Locomotion";
        loco.Type = EAnimNodeType::BlendSpace;
        loco.BlendSpace = LocomotionBlend;
        loco.bLoop = true;
        StateMachine.AddState(loco);

        FAnimState jumpStart;
        jumpStart.Name = "JumpStart";
        jumpStart.Sequence = jumpUp ? jumpUp : jumpLoop;
        jumpStart.bLoop = false;
        StateMachine.AddState(jumpStart);

        FAnimState falling;
        falling.Name = "Falling";
        falling.Sequence = jumpLoop;
        falling.bLoop = true;
        StateMachine.AddState(falling);

        FAnimState landing;
        landing.Name = "Landing";
        landing.Sequence = jumpDown ? jumpDown : jumpLoop;
        landing.bLoop = false;
        StateMachine.AddState(landing);

        FAnimState death;
        death.Name = "Death";
        death.Sequence = DeathSequence;
        death.bLoop = false;
        StateMachine.AddState(death);

        StateMachine.AddTransition({"Locomotion", "JumpStart", "FloatGreater:VerticalSpeed:1.5", 0.06f});
        StateMachine.AddTransition({"JumpStart", "Falling", "TimeGreater:0.08", 0.08f});
        StateMachine.AddTransition({"JumpStart", "Falling", "FloatLessEqual:VerticalSpeed:0.2", 0.08f});
        StateMachine.AddTransition({"JumpStart", "Locomotion", "NotBool:bIsFalling", 0.08f});
        StateMachine.AddTransition({"Locomotion", "Falling", "Bool:bIsFalling", 0.08f});
        StateMachine.AddTransition({"Falling", "Landing", "NotBool:bIsFalling", 0.08f});
        StateMachine.AddTransition({"Landing", "Locomotion", "TimeGreater:0.12", 0.10f});
        StateMachine.AddTransition({"Landing", "Falling", "Bool:bIsFalling", 0.08f});
        StateMachine.AddTransition({"*", "Death", "Bool:bIsDead", 0.1f});
        StateMachine.AddTransition({"Death", "Locomotion", "NotBool:bIsDead", 0.08f});
        StateMachine.SetDefaultState("Locomotion");
        StateMachine.ResetToDefault();
        bGraphBuilt = true;
    }

    void ULeonTournamentAnimInstance::PlayDeathMontage() {
        if (DeathSequence)
            SetOverrideSequence(DeathSequence, false);
    }

    void ULeonTournamentAnimInstance::NativeUpdateAnimation(float InDeltaSeconds) {
        SetUpperBodySequence(nullptr);
        UAnimInstance::NativeUpdateAnimation(InDeltaSeconds);
    }

} // namespace Leon
