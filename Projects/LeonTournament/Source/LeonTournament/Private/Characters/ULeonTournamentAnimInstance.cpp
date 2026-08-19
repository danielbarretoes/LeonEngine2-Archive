#include "ULeonTournamentAnimInstance.hpp"
#include "Assets/UAssetManager.hpp"

#include <cmath>
#include <filesystem>
#include <string_view>

namespace Leon {

    namespace {
        TRef<UAnimSequence> LoadAnim(const TRef<USkeleton>& InSkeleton, const std::string& InPath) {
            auto seq = UAssetManager::GetAnimSequence(InPath);
            if (seq && InSkeleton)
                seq->LinkSkeleton(InSkeleton);
            return seq;
        }

        bool PathEndsWith(const std::string& InPath, std::string_view InSuffix) {
            return InPath.size() >= InSuffix.size() &&
                   InPath.compare(InPath.size() - InSuffix.size(), InSuffix.size(), InSuffix) == 0;
        }

        float AngleDeltaDegrees(float InA, float InB) {
            float d = InA - InB;
            while (d > 180.0f)
                d -= 360.0f;
            while (d < -180.0f)
                d += 360.0f;
            return d;
        }
    } // namespace

    ULeonTournamentAnimInstance::ULeonTournamentAnimInstance(const std::string& InName) : UAnimInstance(InName) {}

    TRef<UAnimSequence> ULeonTournamentAnimInstance::LoadLinked(const std::string& InPath) {
        return LoadAnim(Skeleton, InPath);
    }

    TRef<UBlendSpace> ULeonTournamentAnimInstance::BuildLocomotionBlendSpace(const TRef<USkeleton>& InSkeleton) {
        auto bs = UBlendSpace::Create("BS_Locomotion");
        bs->Set2D(true);
        // Speed is cm/s (world m/s * 100). Walk≈600, sprint≈1080 with default multipliers.
        bs->SetAxisRange({0.0f, -180.0f}, {1200.0f, 180.0f});
        bs->SetSkeletonPath(InSkeleton && !InSkeleton->GetAssetPath().empty() ? InSkeleton->GetAssetPath()
                                                                              : "/Game/Skeletons/YBot.lskeleton");
        bs->SetAssetPath("/Game/BlendSpaces/BS_Locomotion.lblend");

        auto add = [&](float speed, float direction, const std::string& path) {
            auto seq = LoadAnim(InSkeleton, path);
            if (seq)
                bs->AddSample({speed, direction}, seq);
        };

        // Idle anchors — same clip at cardinal headings so stop facing stays stable.
        add(0.0f, 0.0f, "/Game/Animations/Idle.lanim");
        add(0.0f, 90.0f, "/Game/Animations/Idle.lanim");
        add(0.0f, -90.0f, "/Game/Animations/Idle.lanim");
        add(0.0f, 180.0f, "/Game/Animations/Idle.lanim");
        add(0.0f, -180.0f, "/Game/Animations/Idle.lanim");

        // Flow: authored 8-way run ring (no WalkForwardRight — wrong gait / missing RunForwardRight).
        // +45° is intentionally omitted so Forward+Right IDW synthesize the diagonal.
        auto addRunRing = [&](float speed) {
            add(speed, 0.0f, "/Game/Animations/RunForward.lanim");
            add(speed, 90.0f, "/Game/Animations/RunRight.lanim");
            add(speed, 135.0f, "/Game/Animations/RunBackwardRight.lanim");
            add(speed, 180.0f, "/Game/Animations/RunBackward.lanim");
            add(speed, -180.0f, "/Game/Animations/RunBackward.lanim");
            add(speed, -135.0f, "/Game/Animations/RunBackwardLeft.lanim");
            add(speed, -90.0f, "/Game/Animations/RunLeft.lanim");
            add(speed, -45.0f, "/Game/Animations/RunForwardLeft.lanim");
        };
        addRunRing(350.0f);
        addRunRing(600.0f);

        // Sprint only pushes the forward axis; other headings keep the run ring.
        add(1100.0f, 0.0f, "/Game/Animations/SprintForward.lanim");
        return bs;
    }

    bool ULeonTournamentAnimInstance::LocomotionBlendCoversEightDirections(const UBlendSpace& InBlend) {
        if (!InBlend.Is2D() || InBlend.GetSamples().empty())
            return false;

        // Reject legacy grids that wired WalkForwardRight into the run ring.
        for (const auto& sample : InBlend.GetSamples()) {
            if (!sample.Sequence && sample.SequencePath.empty())
                return false;
            if (sample.Coord.x > 50.0f && PathEndsWith(sample.SequencePath, "WalkForwardRight.lanim"))
                return false;
            if (sample.Coord.x > 1250.0f)
                return false;
        }

        const struct {
            float Dir;
            const char* RequiredSuffix; // nullptr => synthesized by neighbors
        } kExpected[] = {
            {0.0f, "RunForward.lanim"},    {45.0f, nullptr},
            {90.0f, "RunRight.lanim"},     {135.0f, "RunBackwardRight.lanim"},
            {180.0f, "RunBackward.lanim"}, {-135.0f, "RunBackwardLeft.lanim"},
            {-90.0f, "RunLeft.lanim"},     {-45.0f, "RunForwardLeft.lanim"},
        };

        bool hasIdle = false;
        bool hasDir[8] = {};
        for (const auto& sample : InBlend.GetSamples()) {
            if (sample.Coord.x <= 1.0f)
                hasIdle = true;
            if (sample.Coord.x < 300.0f || sample.Coord.x > 700.0f)
                continue;
            for (int i = 0; i < 8; ++i) {
                if (std::abs(AngleDeltaDegrees(sample.Coord.y, kExpected[i].Dir)) > 6.0f)
                    continue;
                if (!kExpected[i].RequiredSuffix) {
                    hasDir[i] = true;
                    continue;
                }
                if (PathEndsWith(sample.SequencePath, kExpected[i].RequiredSuffix))
                    hasDir[i] = true;
            }
        }
        // +45° may be omitted; mark covered when both forward and right run samples exist.
        if (!hasDir[1] && hasDir[0] && hasDir[2])
            hasDir[1] = true;

        if (!hasIdle)
            return false;
        for (bool ok : hasDir) {
            if (!ok)
                return false;
        }
        return true;
    }

    void ULeonTournamentAnimInstance::NativeInitializeAnimation() {
        if (!Skeleton)
            return;
        // Skins use different skeletons; rebuild when the linked skeleton changes.
        if (bGraphBuilt && GraphSkeleton.get() == Skeleton.get())
            return;
        bGraphBuilt = false;
        GraphSkeleton = Skeleton;

        LocomotionBlend = nullptr;
        const std::string disk = UAssetManager::ResolveVirtualPath("/Game/BlendSpaces/BS_Locomotion.lblend");
        if (!disk.empty() && std::filesystem::exists(disk))
            LocomotionBlend = UAssetManager::GetBlendSpace("/Game/BlendSpaces/BS_Locomotion.lblend");

        bool bSamplesReady = false;
        if (LocomotionBlend && !LocomotionBlend->GetSamples().empty()) {
            LocomotionBlend->ResolveSequences();
            bSamplesReady = LocomotionBlendCoversEightDirections(*LocomotionBlend);
            if (bSamplesReady) {
                for (auto& sample : LocomotionBlend->GetSamples()) {
                    if (sample.Sequence)
                        sample.Sequence->LinkSkeleton(Skeleton);
                }
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
            PlayOneShotOverride(DeathSequence);
    }

    void ULeonTournamentAnimInstance::NativeUpdateAnimation(float InDeltaSeconds) {
        SetUpperBodySequence(nullptr);
        UAnimInstance::NativeUpdateAnimation(InDeltaSeconds);
    }

} // namespace Leon
