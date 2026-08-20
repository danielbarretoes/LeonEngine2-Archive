#include "Editor/Commands/FTransformActorsCommand.hpp"
#include "Engine/Components.hpp"

namespace Leon::Editor {

    FActorTransformState FTransformActorsCommand::Capture(AActor* InActor) {
        FActorTransformState state;
        state.Actor = InActor;
        if (!InActor || !InActor->HasComponent<FTransformComponent>())
            return state;
        const auto& tc = InActor->GetComponent<FTransformComponent>();
        state.Location = tc.Translation;
        state.Rotation = tc.Rotation;
        state.Scale = tc.Scale;
        return state;
    }

    void FTransformActorsCommand::Apply(const FActorTransformState& InState) {
        if (!InState.Actor || InState.Actor->IsPendingKill())
            return;
        InState.Actor->SetActorLocation(InState.Location);
        InState.Actor->SetActorRotation(InState.Rotation);
        InState.Actor->SetActorScale(InState.Scale);
    }

    bool FTransformActorsCommand::Differ(const FActorTransformState& InA, const FActorTransformState& InB) {
        constexpr float kEps = 1e-5f;
        auto ne = [](const glm::vec3& a, const glm::vec3& b) {
            return glm::abs(a.x - b.x) > kEps || glm::abs(a.y - b.y) > kEps || glm::abs(a.z - b.z) > kEps;
        };
        return InA.Actor != InB.Actor || ne(InA.Location, InB.Location) || ne(InA.Rotation, InB.Rotation) ||
               ne(InA.Scale, InB.Scale);
    }

    FTransformActorsCommand::FTransformActorsCommand(std::vector<FActorTransformState> InBefore,
                                                     std::vector<FActorTransformState> InAfter,
                                                     std::string InDescription)
        : Before(std::move(InBefore)), After(std::move(InAfter)), Description(std::move(InDescription)) {
        if (Description.empty()) {
            if (After.size() == 1 && After[0].Actor)
                Description = "Transform " + After[0].Actor->GetName();
            else
                Description = "Transform " + std::to_string(After.size()) + " Actors";
        }
    }

    void FTransformActorsCommand::Execute() {
        for (const auto& state : After)
            Apply(state);
    }

    void FTransformActorsCommand::Undo() {
        for (const auto& state : Before)
            Apply(state);
    }

} // namespace Leon::Editor
