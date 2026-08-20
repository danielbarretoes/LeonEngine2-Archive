#include "ALeonTournamentFlagBase.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "FLeonTournamentArenaBuilder.hpp"
#include "Engine/Components.hpp"

#include <cmath>

namespace Leon {

    namespace {
        glm::vec3 TeamColor(ELeonTournamentTeam InTeam) {
            switch (InTeam) {
            case ELeonTournamentTeam::Team1:
                return {0.25f, 0.55f, 1.0f};
            case ELeonTournamentTeam::Team2:
                return {1.0f, 0.35f, 0.25f};
            default:
                return {0.7f, 0.7f, 0.7f};
            }
        }
    } // namespace

    ALeonTournamentFlagBase::ALeonTournamentFlagBase(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentFlagBase");
    }

    void ALeonTournamentFlagBase::BeginPlay() {
        AActor::BeginPlay();
        BuildVisual();
    }

    bool ALeonTournamentFlagBase::IsCharacterInside(const ALeonTournamentCharacter& InCharacter) const {
        glm::vec3 delta = InCharacter.GetActorLocation() - GetActorLocation();
        delta.y = 0.0f;
        return glm::dot(delta, delta) <= CaptureRadius * CaptureRadius;
    }

    void ALeonTournamentFlagBase::BuildVisual() {
        if (!World)
            return;
        const glm::vec3 color = TeamColor(Team);
        FLeonTournamentArenaBuilder::SpawnSimpleBox(World, GetName() + "_Pad", GetActorLocation(),
                                                    {CaptureRadius * 2.0f, 0.08f, CaptureRadius * 2.0f}, color * 0.45f);
        FLeonTournamentArenaBuilder::SpawnSimpleBox(
            World, GetName() + "_Pillar", GetActorLocation() + glm::vec3(0.0f, 0.65f, 0.0f), {0.7f, 1.3f, 0.7f}, color);
    }

} // namespace Leon
