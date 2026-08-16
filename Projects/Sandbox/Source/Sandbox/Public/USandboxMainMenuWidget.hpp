#pragma once

#include "UMG/UUserWidget.hpp"
#include "UMG/UButton.hpp"
#include "UMG/UTextBlock.hpp"
#include "UMG/UCanvasPanel.hpp"

namespace Leon {

    /**
     * @brief Sandbox menu chip — travel between ShowcaseLevel and NightLevel.
     */
    class USandboxMainMenuWidget : public UUserWidget {
    public:
        USandboxMainMenuWidget(const std::string& InName = "SandboxMainMenuWidget");

        void Construct() override;

        /** @brief When false, shows Night Level title (button still opens NightLevel). */
        void SetIsShowcaseLayout(bool bShowcase) { bShowcaseLayout = bShowcase; }

    private:
        void BuildWidgetTree();
        void OnOpenNightLevelClicked();

        TRef<UCanvasPanel> RootCanvas;
        TRef<UTextBlock> TitleText;
        TRef<UButton> NightLevelButton;
        TRef<UTextBlock> ButtonLabel;
        bool bShowcaseLayout = true;
    };

} // namespace Leon
