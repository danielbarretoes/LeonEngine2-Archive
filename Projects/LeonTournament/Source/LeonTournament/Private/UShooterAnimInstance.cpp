#include "UShooterAnimInstance.hpp"
#include "Assets/UAssetManager.hpp"

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

    UShooterAnimInstance::UShooterAnimInstance(const std::string& InName) : UAnimInstance(InName) {}

    TRef<UAnimSequence> UShooterAnimInstance::LoadLinked(const std::string& InPath) {
        return LoadAnim(Skeleton, InPath);
    }

    TRef<UBlendSpace> UShooterAnimInstance::BuildLocomotionBlendSpace(const TRef<USkeleton>& InSkeleton) {
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

        // Ground locomotion only. Jump clips live in the state machine; death is health-driven.
        add(0.0f, 0.0f, "/Game/Animations/Idle.lanim");
        add(0.0f, 90.0f, "/Game/Animations/Idle.lanim");
        add(0.0f, -90.0f, "/Game/Animations/Idle.lanim");
        add(0.0f, 180.0f, "/Game/Animations/Idle.lanim");
        add(0.0f, -180.0f, "/Game/Animations/Idle.lanim");
        add(180.0f, 45.0f, "/Game/Animations/WalkForwardRight.lanim");
        add(500.0f, 0.0f, "/Game/Animations/RunForward.lanim");
        add(500.0f, -45.0f, "/Game/Animations/RunForwardLeft.lanim");
        add(500.0f, -90.0f, "/Game/Animations/RunLeft.lanim");
        add(500.0f, 90.0f, "/Game/Animations/RunRight.lanim");
        add(500.0f, 180.0f, "/Game/Animations/RunBackward.lanim");
        add(500.0f, -180.0f, "/Game/Animations/RunBackward.lanim");
        add(500.0f, -135.0f, "/Game/Animations/RunBackwardLeft.lanim");
        add(500.0f, 135.0f, "/Game/Animations/RunBackwardRight.lanim");
        add(1000.0f, 0.0f, "/Game/Animations/SprintForward.lanim");
        return bs;
    }

    void UShooterAnimInstance::NativeInitializeAnimation() {
        if (bGraphBuilt || !Skeleton)
            return;

        LocomotionBlend = nullptr;
        const std::string disk = UAssetManager::ResolveVirtualPath("/Game/BlendSpaces/BS_Locomotion.lblend");
        if (!disk.empty() && std::filesystem::exists(disk))
            LocomotionBlend = UAssetManager::GetBlendSpace("/Game/BlendSpaces/BS_Locomotion.lblend");

        bool bSamplesReady = false;
        if (LocomotionBlend && !LocomotionBlend->GetSamples().empty()) {
            LocomotionBlend->ResolveSequences();
            bSamplesReady = true;
            for (auto& sample : LocomotionBlend->GetSamples()) {
                if (!sample.Sequence) {
                    bSamplesReady = false;
                    break;
                }
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
        IdleSequence = LoadLinked("/Game/Animations/Idle.lanim");

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

        FAnimState jump;
        jump.Name = "Jump";
        jump.Sequence = jumpLoop ? jumpLoop : jumpUp;
        jump.bLoop = true;
        StateMachine.AddState(jump);

        FAnimState jumpLand;
        jumpLand.Name = "JumpLand";
        jumpLand.Sequence = jumpDown ? jumpDown : jumpLoop;
        jumpLand.bLoop = false;
        StateMachine.AddState(jumpLand);

        FAnimState death;
        death.Name = "Death";
        death.Sequence = DeathSequence;
        death.bLoop = false;
        StateMachine.AddState(death);

        StateMachine.AddTransition({"Locomotion", "JumpStart", "Bool:bIsFalling", 0.08f});
        StateMachine.AddTransition({"JumpStart", "Locomotion", "NotBool:bIsFalling", 0.12f});
        StateMachine.AddTransition({"JumpStart", "Jump", "FloatLessEqual:VerticalSpeed:1.5", 0.08f});
        StateMachine.AddTransition({"Jump", "Locomotion", "NotBool:bIsFalling", 0.12f});
        StateMachine.AddTransition({"Jump", "JumpLand", "FloatLessEqual:VerticalSpeed:-2.0", 0.08f});
        StateMachine.AddTransition({"JumpLand", "Locomotion", "NotBool:bIsFalling", 0.12f});
        StateMachine.AddTransition({"*", "Death", "Bool:bIsDead", 0.1f});
        StateMachine.SetDefaultState("Locomotion");
        StateMachine.Reset();
        bGraphBuilt = true;
    }

    void UShooterAnimInstance::NativeUpdateAnimation(float InDeltaSeconds) {
        UAnimInstance::NativeUpdateAnimation(InDeltaSeconds);
        const bool bSprint = GetBool("IsSprinting");
        const bool bUpper = !bSprint && (GetBool("IsAiming") || GetBool("IsFiring") || GetBool("IsReloading"));
        SetUpperBodySequence(bUpper ? IdleSequence : nullptr);
    }

} // namespace Leon
