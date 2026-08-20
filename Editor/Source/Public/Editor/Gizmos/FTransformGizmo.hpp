#pragma once

#include "Core/Base.hpp"
#include "Gameplay/AActor.hpp"
#include "Renderer/FPerspectiveCamera.hpp"

namespace Leon::Editor {

    enum class EGizmoOperation { Translate, Rotate, Scale };

    enum class EGizmoMode { Local, World };

    /**
     * @brief 3D viewport gizmo controller for manipulating actor transforms.
     */
    class FTransformGizmo {
    public:
        FTransformGizmo() = default;

        EGizmoOperation GetOperation() const { return CurrentOperation; }
        void SetOperation(EGizmoOperation InOp) { CurrentOperation = InOp; }

        EGizmoMode GetMode() const { return CurrentMode; }
        void SetMode(EGizmoMode InMode) { CurrentMode = InMode; }

        void ProcessHotkeys();
        void Draw(AActor* InSelectedActor, const FPerspectiveCamera& InCamera, float InViewportX, float InViewportY,
                  float InViewportW, float InViewportH);

    private:
        EGizmoOperation CurrentOperation = EGizmoOperation::Translate;
        EGizmoMode CurrentMode = EGizmoMode::World;
    };

} // namespace Leon::Editor
