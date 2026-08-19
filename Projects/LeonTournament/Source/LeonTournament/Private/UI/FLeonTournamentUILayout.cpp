#include "FLeonTournamentUILayout.hpp"
#include "FLeonTournamentTypes.hpp"
#include "ULeonTournamentGameInstance.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentGameState.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "UMG/UWidget.hpp"

#include <algorithm>
#include <glm/glm.hpp>

namespace Leon {

    ULeonTournamentGameInstance* FLeonTournamentUILayout::GI() {
        return UEngine::HasInstance()
                   ? dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get())
                   : nullptr;
    }

    ALeonTournamentGameMode* FLeonTournamentUILayout::GM(APlayerController* InPC) {
        UWorld* world = InPC ? InPC->GetWorld() : nullptr;
        return world ? dynamic_cast<ALeonTournamentGameMode*>(world->GetGameMode()) : nullptr;
    }

    ALeonTournamentGameState* FLeonTournamentUILayout::GS(APlayerController* InPC) {
        UWorld* world = InPC ? InPC->GetWorld() : nullptr;
        return world ? dynamic_cast<ALeonTournamentGameState*>(world->GetGameState()) : nullptr;
    }

    bool FLeonTournamentUILayout::IsClientWorld(APlayerController* InPC) {
        UWorld* world = InPC ? InPC->GetWorld() : nullptr;
        return world && world->GetNetMode() == ENetMode::Client;
    }

    void FLeonTournamentUILayout::ApplyMenuRailLayout(UCanvasPanel& InRoot, const TRef<UWidget>& InPanel,
                                                      const TRef<UButton>& InPrev, const TRef<UTextBlock>& InLabel,
                                                      const TRef<UButton>& InNext, bool bLobby, float InScale) {
        const glm::vec2 vp = ResolveViewportSize(&InRoot);
        InRoot.SetSize(vp);
        const float s = InScale > 1.0e-4f ? InScale : LayoutScale(vp.x, vp.y);
        const float designPanel = bLobby ? kLeonTournamentLobbyPanelDesignWidth : kLeonTournamentMenuPanelDesignWidth;
        const float panelW = std::min(designPanel * s, vp.x * 0.62f);
        auto place = [&](const TRef<UWidget>& widget, const FAnchors& anchors, const FMargin& offsets) {
            if (!widget)
                return;
            if (!InRoot.SetChildLayout(widget, anchors, offsets))
                InRoot.AddChild(widget, anchors, offsets);
        };
        place(InPanel, FAnchors::LeftStretch(), BoxLeftStretch(0.0f, panelW));

        const float inset = 48.0f * s;
        const float gap = 12.0f * s;
        const float nameW = 220.0f * s;
        const float prevW = InPrev ? InPrev->GetSize().x : 56.0f * s;
        const float prevH = InPrev ? InPrev->GetSize().y : 52.0f * s;
        const float nextW = InNext ? InNext->GetSize().x : 56.0f * s;
        const float nextH = InNext ? InNext->GetSize().y : 52.0f * s;
        const float nameH =
            InLabel ? MeasurePadded(InLabel->GetText().empty() ? "YBOT" : InLabel->GetText(), InLabel->GetFontScale()).y
                    : prevH;
        place(InNext, FAnchors::BottomRight(), BoxBR(inset, inset, nextW, nextH));
        place(InLabel, FAnchors::BottomRight(),
              BoxBR(inset + nextW + gap, inset + (prevH - nameH) * 0.5f, nameW, nameH));
        place(InPrev, FAnchors::BottomRight(), BoxBR(inset + nextW + gap + nameW + gap, inset, prevW, prevH));
    }

} // namespace Leon
