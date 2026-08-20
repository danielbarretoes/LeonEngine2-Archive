#pragma once

#include "Editor/Commands/IEditorCommand.hpp"
#include "Gameplay/AActor.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Leon::Editor {

    struct FActorTransformState {
        AActor* Actor = nullptr;
        glm::vec3 Location{0.0f};
        glm::vec3 Rotation{0.0f};
        glm::vec3 Scale{1.0f};
    };

    /** Undoable transform edit (gizmo drag or Details). Execute applies After; Undo restores Before. */
    class FTransformActorsCommand : public IEditorCommand {
    public:
        FTransformActorsCommand(std::vector<FActorTransformState> InBefore, std::vector<FActorTransformState> InAfter,
                                std::string InDescription = "Transform Actors");

        void Execute() override;
        void Undo() override;
        [[nodiscard]] std::string GetDescription() const override { return Description; }

        static FActorTransformState Capture(AActor* InActor);
        static void Apply(const FActorTransformState& InState);
        static bool Differ(const FActorTransformState& InA, const FActorTransformState& InB);

    private:
        std::vector<FActorTransformState> Before;
        std::vector<FActorTransformState> After;
        std::string Description;
    };

} // namespace Leon::Editor
