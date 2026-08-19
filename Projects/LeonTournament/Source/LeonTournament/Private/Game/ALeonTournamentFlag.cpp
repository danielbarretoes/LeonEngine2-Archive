#include "ALeonTournamentFlag.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "Assets/UAssetManager.hpp"
#include "Engine/Components.hpp"
#include "Renderer/FMaterialInstance.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "Core/FApplication.hpp"

#include <cmath>

namespace Leon {

    namespace {
        constexpr float kDropReturnSeconds = 30.0f;
        constexpr float kPickupRadiusSq = 1.8f * 1.8f;

        glm::vec3 TeamColor(ELeonTournamentTeam InTeam) {
            switch (InTeam) {
            case ELeonTournamentTeam::Team1:
                return {0.35f, 0.65f, 1.0f};
            case ELeonTournamentTeam::Team2:
                return {1.0f, 0.45f, 0.3f};
            default:
                return {0.85f, 0.85f, 0.85f};
            }
        }
    } // namespace

    ALeonTournamentFlag::ALeonTournamentFlag(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentFlag");
        SetReplicates(true);
        SetAlwaysRelevant(true);
    }

    void ALeonTournamentFlag::BeginPlay() {
        AActor::BeginPlay();
        BuildVisual();
    }

    void ALeonTournamentFlag::Tick(float DeltaSeconds) {
        AActor::Tick(DeltaSeconds);
        if (!HasAuthority())
            return;
        if (FlagStatus == ELeonTournamentFlagStatus::Dropped) {
            DropReturnRemaining -= DeltaSeconds;
            if (DropReturnRemaining <= 0.0f)
                ReturnToBase();
        }
        UpdateCarriedVisual(DeltaSeconds);
    }

    void ALeonTournamentFlag::SetHomeLocation(const glm::vec3& InLocation) {
        HomeLocation = InLocation;
        if (FlagStatus == ELeonTournamentFlagStatus::AtBase)
            SetActorLocation(HomeLocation);
    }

    bool ALeonTournamentFlag::CanBePickedUpBy(const ALeonTournamentCharacter& InCharacter) const {
        if (InCharacter.IsDeadFrozen())
            return false;
        if (InCharacter.GetCarriedFlag())
            return false;
        if (InCharacter.GetTeam() == OwnerTeam || InCharacter.GetTeam() == ELeonTournamentTeam::None)
            return false;
        if (FlagStatus == ELeonTournamentFlagStatus::Carried)
            return false;
        glm::vec3 delta = InCharacter.GetActorLocation() - GetActorLocation();
        return glm::dot(delta, delta) <= kPickupRadiusSq;
    }

    void ALeonTournamentFlag::AttachToCarrier(ALeonTournamentCharacter& InCarrier) {
        if (!HasAuthority())
            return;
        Carrier = &InCarrier;
        FlagStatus = ELeonTournamentFlagStatus::Carried;
        DropReturnRemaining = 0.0f;
        InCarrier.SetCarriedFlag(this);
        SetActorLocation(InCarrier.GetActorLocation() + glm::vec3(0.0f, 1.4f, -0.35f));
    }

    void ALeonTournamentFlag::DropAt(const glm::vec3& InLocation) {
        if (!HasAuthority())
            return;
        if (Carrier) {
            Carrier->SetCarriedFlag(nullptr);
            Carrier = nullptr;
        }
        FlagStatus = ELeonTournamentFlagStatus::Dropped;
        DropReturnRemaining = kDropReturnSeconds;
        SetActorLocation(InLocation + glm::vec3(0.0f, 0.9f, 0.0f));
    }

    void ALeonTournamentFlag::ReturnToBase() {
        if (!HasAuthority())
            return;
        if (Carrier) {
            Carrier->SetCarriedFlag(nullptr);
            Carrier = nullptr;
        }
        FlagStatus = ELeonTournamentFlagStatus::AtBase;
        DropReturnRemaining = 0.0f;
        SetActorLocation(HomeLocation);
    }

    void ALeonTournamentFlag::UpdateCarriedVisual(float DeltaSeconds) {
        if (FlagStatus == ELeonTournamentFlagStatus::Carried && Carrier && !Carrier->IsPendingKill()) {
            SetActorLocation(Carrier->GetActorLocation() + glm::vec3(0.0f, 1.35f, -0.35f));
            SpinYaw += DeltaSeconds * 90.0f;
            SetActorRotation({0.0f, SpinYaw, 0.0f});
        } else if (FlagStatus == ELeonTournamentFlagStatus::AtBase) {
            SpinYaw += DeltaSeconds * 45.0f;
            SetActorRotation({0.0f, SpinYaw, 0.0f});
        }
    }

    void ALeonTournamentFlag::BuildVisual() {
        if (!FApplication::HasInstance())
            return;
        auto shader = UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
        if (!shader)
            return;
        auto va = FMeshPrimitives::CreateCylinder(0.12f, 0.12f, 0.9f, 10, true);
        FMeshComponent& mesh = HasComponent<FMeshComponent>() ? GetComponent<FMeshComponent>()
                                                              : AddComponent<FMeshComponent>(va, shader);
        mesh.VertexArray = va;
        mesh.Shader = shader;
        mesh.MeshType = "FlagBanner";
        mesh.Mobility = EComponentMobility::Movable;
        mesh.bCastShadows = false;
        mesh.bVisible = true;
        const glm::vec3 color = TeamColor(OwnerTeam);
        if (!HasComponent<FMaterialComponent>()) {
            if (auto parent = UAssetManager::GetDefaultMaterial()) {
                auto inst = parent->CreateInstance("FlagMat");
                inst->SetAlbedoColor(color);
                inst->SetEmissiveColor(color);
                inst->SetEmissiveIntensity(2.5f);
                AddComponent<FMaterialComponent>(inst);
            }
        }
    }

    void ALeonTournamentFlag::SerializeReplication(std::vector<uint8_t>& OutBytes) const {
        AActor::SerializeReplication(OutBytes);
        FNetBlob::WriteU8(OutBytes, static_cast<uint8_t>(OwnerTeam));
        FNetBlob::WriteU8(OutBytes, static_cast<uint8_t>(FlagStatus));
        FNetBlob::WriteF32(OutBytes, DropReturnRemaining);
    }

    void ALeonTournamentFlag::DeserializeReplication(const uint8_t* InData, size_t InSize) {
        if (!InData || InSize < 6)
            return;
        std::vector<uint8_t> bytes(InData, InData + InSize);
        size_t offset = 0;
        uint8_t team = 0;
        uint8_t status = 0;
        if (!FNetBlob::ReadU8(bytes, offset, team) || !FNetBlob::ReadU8(bytes, offset, status) ||
            !FNetBlob::ReadF32(bytes, offset, DropReturnRemaining))
            return;
        OwnerTeam = static_cast<ELeonTournamentTeam>(team);
        FlagStatus = static_cast<ELeonTournamentFlagStatus>(status);
    }

} // namespace Leon
