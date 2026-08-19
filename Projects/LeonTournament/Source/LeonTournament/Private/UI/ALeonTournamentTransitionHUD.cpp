#include "ALeonTournamentTransitionHUD.hpp"
#include "ULeonTournamentGameInstance.hpp"
#include "Engine/UEngine.hpp"
#include "UMG/UUserWidget.hpp"

namespace Leon {

    ALeonTournamentTransitionHUD::ALeonTournamentTransitionHUD(entt::entity InHandle, UWorld* InWorld,
                                                               const std::string& InName)
        : AHUD(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentTransitionHUD");
    }

    void ALeonTournamentTransitionHUD::BeginPlay() {
        AHUD::BeginPlay();
        if (!PlayerController)
            return;

        std::string label = "LOADING...";
        if (UEngine::HasInstance()) {
            if (auto* gi = dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get()))
                label = gi->GetLoadingOverlayLabel();
        }

        auto overlay = UUserWidget::CreateWidget<ULeonTournamentLoadingOverlayWidget>(PlayerController);
        if (!overlay)
            return;
        overlay->SetStatusText(label);
        overlay->AddToViewport(100);
    }

} // namespace Leon
