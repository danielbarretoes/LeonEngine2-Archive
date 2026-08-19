#include "Renderer/FRenderDebugHotkeys.hpp"
#include "Renderer/FWorldRenderer.hpp"
#include "Core/FInput.hpp"
#include "Core/FLog.hpp"

namespace Leon {

    int FRenderDebugHotkeys::NextShaderMode(int InCurrent, std::span<const FRenderDebugView> InCycle,
                                            const char*& OutName) {
        if (InCycle.empty()) {
            OutName = "Lit";
            return 0;
        }
        for (size_t i = 0; i < InCycle.size(); ++i) {
            if (InCycle[i].Mode == InCurrent) {
                const FRenderDebugView& next = InCycle[(i + 1) % InCycle.size()];
                OutName = next.Name;
                return next.Mode;
            }
        }
        OutName = InCycle[0].Name;
        return InCycle[0].Mode;
    }

    const char* FRenderDebugHotkeys::CycleNameOrNull(int InMode, std::span<const FRenderDebugView> InCycle) {
        for (const FRenderDebugView& view : InCycle) {
            if (view.Mode == InMode)
                return view.Name;
        }
        return nullptr;
    }

    const char* FRenderDebugHotkeys::ShaderViewName(int InMode) {
        if (InMode == 0)
            return "Lit";
        if (InMode == 13)
            return "Planar Reflections Buffer";
        if (const char* name = CycleNameOrNull(InMode, MaterialCycle))
            return name;
        if (const char* name = CycleNameOrNull(InMode, GeometryCycle))
            return name;
        if (const char* name = CycleNameOrNull(InMode, LightingCycle))
            return name;
        if (const char* name = CycleNameOrNull(InMode, IBLMapCycle))
            return name;
        if (const char* name = CycleNameOrNull(InMode, ShadowCycle))
            return name;
        return "Unknown";
    }

    const char* FRenderDebugHotkeys::PostViewName(int InPostMode) {
        if (InPostMode < 0 || InPostMode >= PostProcessCycleCount)
            return PostProcessCycle[0];
        return PostProcessCycle[InPostMode];
    }

    bool FRenderDebugHotkeys::ApplyKey(FRenderDebugState& InOutState, int InKeyCode, const char*& OutLog) {
        OutLog = "";
        switch (InKeyCode) {
        case Key::F3:
            InOutState.bWireframe = !InOutState.bWireframe;
            OutLog = InOutState.bWireframe ? "Wireframe: ENABLED" : "Wireframe: DISABLED";
            return true;
        case Key::F4:
            InOutState.PostProcessDebugMode = 0;
            InOutState.ShaderMode = NextShaderMode(InOutState.ShaderMode, MaterialCycle, OutLog);
            return true;
        case Key::F5:
            InOutState.PostProcessDebugMode = 0;
            InOutState.ShaderMode = NextShaderMode(InOutState.ShaderMode, GeometryCycle, OutLog);
            return true;
        case Key::F6:
            InOutState.PostProcessDebugMode = 0;
            InOutState.ShaderMode = NextShaderMode(InOutState.ShaderMode, LightingCycle, OutLog);
            return true;
        case Key::F7:
            InOutState.PostProcessDebugMode = 0;
            InOutState.ShaderMode = NextShaderMode(InOutState.ShaderMode, IBLMapCycle, OutLog);
            return true;
        case Key::F8:
            InOutState.PostProcessDebugMode = 0;
            InOutState.ShaderMode = NextShaderMode(InOutState.ShaderMode, ShadowCycle, OutLog);
            return true;
        case Key::F9:
            InOutState.PostProcessDebugMode = 0;
            InOutState.ShaderMode = 13;
            OutLog = "Planar Reflections Buffer";
            return true;
        case Key::F10: {
            InOutState.ShaderMode = 0;
            InOutState.PostProcessDebugMode = (InOutState.PostProcessDebugMode + 1) % PostProcessCycleCount;
            OutLog = PostProcessCycle[InOutState.PostProcessDebugMode];
            return true;
        }
        case Key::F11:
            InOutState.bPostProcessEnabled = !InOutState.bPostProcessEnabled;
            OutLog = InOutState.bPostProcessEnabled ? "Post-Process: ENABLED" : "Post-Process: DISABLED";
            return true;
        case Key::F12:
            InOutState = FRenderDebugState{};
            OutLog = "Lit / Standard PBR Composite";
            return true;
        default:
            return false;
        }
    }

    bool FRenderDebugHotkeys::ApplyKey(FWorldRenderer& InRenderer, int InKeyCode) {
        FRenderDebugState state;
        state.ShaderMode = InRenderer.GetDebugMode();
        state.PostProcessDebugMode = InRenderer.GetPostProcessSettings().DebugMode;
        state.bWireframe = InRenderer.IsWireframeEnabled();
        state.bPostProcessEnabled = InRenderer.GetPostProcessSettings().bEnabled;

        const char* log = "";
        if (!ApplyKey(state, InKeyCode, log))
            return false;

        InRenderer.SetDebugMode(state.ShaderMode);
        InRenderer.SetWireframeEnabled(state.bWireframe);
        InRenderer.GetPostProcessSettings().DebugMode = state.PostProcessDebugMode;
        InRenderer.GetPostProcessSettings().bEnabled = state.bPostProcessEnabled;
        LE_CORE_INFO("[RENDER DEBUG] {0}", log);
        return true;
    }

} // namespace Leon
