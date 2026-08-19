#pragma once

#include "Gameplay/AActor.hpp"
#include "FLeonTournamentTypes.hpp"
#include "Engine/FNetBlob.hpp"

namespace Leon {

    class ALeonTournamentCharacter;

    /** Team flag for CTF: at base, carried, or dropped. Replicates for LAN clients. */
    class ALeonTournamentFlag : public AActor {
    public:
        ALeonTournamentFlag() = default;
        ALeonTournamentFlag(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "LeonTournamentFlag");

        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;

        ELeonTournamentTeam GetOwnerTeam() const { return OwnerTeam; }
        void SetOwnerTeam(ELeonTournamentTeam InTeam) { OwnerTeam = InTeam; }
        ELeonTournamentFlagStatus GetFlagStatus() const { return FlagStatus; }
        ALeonTournamentCharacter* GetCarrier() const { return Carrier; }

        void SetHomeLocation(const glm::vec3& InLocation);
        const glm::vec3& GetHomeLocation() const { return HomeLocation; }

        bool CanBePickedUpBy(const ALeonTournamentCharacter& InCharacter) const;
        void AttachToCarrier(ALeonTournamentCharacter& InCarrier);
        void DropAt(const glm::vec3& InLocation);
        void ReturnToBase();
        void UpdateCarriedVisual(float DeltaSeconds);

        void SerializeReplication(std::vector<uint8_t>& OutBytes) const override;
        void DeserializeReplication(const uint8_t* InData, size_t InSize) override;

        void BuildVisual();

    private:
        ELeonTournamentTeam OwnerTeam = ELeonTournamentTeam::None;
        ELeonTournamentFlagStatus FlagStatus = ELeonTournamentFlagStatus::AtBase;
        ALeonTournamentCharacter* Carrier = nullptr;
        glm::vec3 HomeLocation{0.0f};
        float DropReturnRemaining = 0.0f;
        float SpinYaw = 0.0f;
    };

} // namespace Leon
