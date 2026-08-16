#pragma once

#include "UMG/UUserWidget.hpp"
#include "UMG/UButton.hpp"
#include "UMG/UTextBlock.hpp"
#include "UMG/UCanvasPanel.hpp"

namespace Leon {

    /**
     * @brief Main showcase menu widget with title and Open Night Scene button.
     */
    class USandboxMainMenuWidget : public UUserWidget {
    public:
        USandboxMainMenuWidget(const std::string& InName = "SandboxMainMenuWidget");

        void Construct() override;

        /** @brief When false, shows Night Scene title (button still opens NightScene via virtual path). */
        void SetIsShowcaseLayout(bool bShowcase) { bShowcaseLayout = bShowcase; }

    private:
        void BuildWidgetTree();
        void OnOpenNightSceneClicked();

        TRef<UCanvasPanel> RootCanvas;
        TRef<UTextBlock> TitleText;
        TRef<UButton> NightSceneButton;
        TRef<UTextBlock> ButtonLabel;
        bool bShowcaseLayout = true;
    };

} // namespace Leon
