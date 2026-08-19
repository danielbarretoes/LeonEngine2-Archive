#include "FLeonTournamentUILayout.hpp"
#include "FLeonTournamentTypes.hpp"
#include "ULeonTournamentGameInstance.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentGameState.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Engine/UEngine.hpp"
#include "Engine/UWorld.hpp"
#include "Core/FApplication.hpp"
#include "Core/FInputSettings.hpp"
#include "UMG/UWidget.hpp"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace Leon {

    ULeonTournamentGameInstance* FLeonTournamentUILayout::GI() {
        return UEngine::HasInstance()
                   ? dynamic_cast<ULeonTournamentGameInstance*>(UEngine::Get().GetGameInstance().get())
                   : nullptr;
    }

    bool FLeonTournamentUILayout::GamepadEdge(int InButton, bool& InOutWasDown) {
        const FInputSettings& input = FInputSettings::Get();
        if (!input.bEnableGamepad || !FInput::IsGamepadConnected(input.GamepadId)) {
            InOutWasDown = false;
            return false;
        }
        const bool down = FInput::IsGamepadButtonPressed(InButton, input.GamepadId);
        const bool edge = down && !InOutWasDown;
        InOutWasDown = down;
        return edge;
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

    glm::vec2 FLeonTournamentUILayout::ResolveViewportSize(const UCanvasPanel* InRoot) {
        const glm::vec2 window = ResolveWindowSize();
        if (window.x > 1.0f && window.y > 1.0f)
            return window;
        if (InRoot && InRoot->GetSize().x > 1.0f && InRoot->GetSize().y > 1.0f)
            return InRoot->GetSize();
        return {kDesignWidth, kDesignHeight};
    }

    float FLeonTournamentUILayout::SyncResolutionScale(UCanvasPanel& InRoot, float& InOutAppliedScale,
                                                       glm::vec2& InOutAppliedViewport) {
        const glm::vec2 vp = ResolveViewportSize(&InRoot);
        const float target = LayoutScale(vp.x, vp.y);
        InRoot.SetSize(vp);
        if (InOutAppliedScale <= 1.0e-4f)
            InOutAppliedScale = 1.0f;
        const float factor = target / InOutAppliedScale;
        if (std::abs(factor - 1.0f) > 0.001f || glm::length(vp - InOutAppliedViewport) > 1.0f) {
            if (std::abs(factor - 1.0f) > 0.001f)
                InRoot.ScaleLayout(factor);
            InOutAppliedScale = target;
            InOutAppliedViewport = vp;
        }
        return target;
    }

    void FLeonTournamentUILayout::ApplyMenuRailLayout(UCanvasPanel& InRoot, const TRef<UWidget>& InPanel,
                                                      const TRef<UButton>& InPrev, const TRef<UTextBlock>& InLabel,
                                                      const TRef<UButton>& InNext, bool bLobby) {
        const glm::vec2 vp = ResolveViewportSize(&InRoot);
        InRoot.SetSize(vp);
        const float s = LayoutScale(vp.x, vp.y);
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
