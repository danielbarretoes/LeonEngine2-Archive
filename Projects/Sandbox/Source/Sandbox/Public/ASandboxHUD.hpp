#pragma once

#include "Gameplay/AHUD.hpp"
#include "UMG/UUserWidget.hpp"

namespace Leon {

    class USandboxRenderLabWidget;

    /** Sandbox HUD — Render Lab overlay (H to toggle) + PrintString demo (P). */
    class ASandboxHUD : public AHUD {
    public:
        ASandboxHUD() = default;
        ASandboxHUD(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "SandboxHUD");

        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;

    private:
        TRef<USandboxRenderLabWidget> RenderLabWidget;
        bool bPrintKeyWasDown = false;
        bool bHideKeyWasDown = false;
    };

} // namespace Leon
