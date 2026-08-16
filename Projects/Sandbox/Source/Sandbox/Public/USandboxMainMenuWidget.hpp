#pragma once

#include "UMG/UUserWidget.hpp"
#include "UMG/UButton.hpp"
#include "UMG/UTextBlock.hpp"
#include "UMG/UCanvasPanel.hpp"

namespace Leon {

    /**
     * @brief Sandbox menu chip — HUD button travels ShowcaseLevel ↔ NightLevel.
     */
    class USandboxMainMenuWidget : public UUserWidget {
    public:
        USandboxMainMenuWidget(const std::string& InName = "SandboxMainMenuWidget");

        void Construct() override;

        void SetIsShowcaseLayout(bool bShowcase) { bShowcaseLayout = bShowcase; }

    private:
        void BuildWidgetTree();
        void OnSwitchSceneClicked();

        TRef<UCanvasPanel> RootCanvas;
        TRef<UTextBlock> TitleText;
        TRef<UButton> SceneSwitchButton;
        TRef<UTextBlock> ButtonLabel;
        bool bShowcaseLayout = true;
    };

} // namespace Leon
