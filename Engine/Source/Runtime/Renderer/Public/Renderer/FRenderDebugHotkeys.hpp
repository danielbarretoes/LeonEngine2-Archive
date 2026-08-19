#pragma once

#include "Core/Base.hpp"

#include <span>

namespace Leon {

    class FWorldRenderer;

    struct FRenderDebugView {
        int Mode = 0;
        const char* Name = "";
    };

    /** Snapshot used by F-key handlers (and tests) without constructing a full renderer. */
    struct FRenderDebugState {
        int ShaderMode = 0;
        int PostProcessDebugMode = 0;
        bool bWireframe = false;
        bool bPostProcessEnabled = true;
    };

    /**
     * @brief F3–F12 render debug. F1/F2 and Shift+F1/F5–F8 stay on FApplication.
     *
     * Flow: key → cycle table → shader u_DebugMode and/or FPostProcessSettings → log.
     * F12 restores lit composite, post debug 0, post enabled, and solid fill.
     */
    class FRenderDebugHotkeys {
    public:
        static constexpr FRenderDebugView MaterialCycle[] = {
            {14, "Albedo / Base Color"},
            {15, "Metallic"},
            {16, "Roughness"},
            {18, "Ambient Occlusion (AO)"},
            {19, "Emissive"},
            {22, "UV0"},
            {20, "Tangent"},
            {21, "Bitangent"},
            {23, "N dot L (sun)"},
        };

        static constexpr FRenderDebugView GeometryCycle[] = {
            {11, "World Normals"},
            {12, "Reflection Vector"},
        };

        static constexpr FRenderDebugView LightingCycle[] = {
            {10, "Dynamic Lighting Only (Lo)"},
            {38, "Direct Diffuse"},
            {39, "Direct Specular"},
            {9, "Specular IBL"},
            {35, "Diffuse IBL"},
            {31, "Baked Lighting Only"},
            {32, "Lightmap Irradiance (raw)"},
            {33, "Lightmap UV (atlas)"},
            {34, "Dynamic + Baked (no IBL)"},
            {37, "HDR before tone map (shader)"},
        };

        static constexpr FRenderDebugView IBLMapCycle[] = {
            {1, "Prefilter mip 0"},
            {3, "Prefilter mip 1"},
            {4, "Prefilter mip 2"},
            {5, "Prefilter mip 3"},
            {6, "Prefilter mip 4"},
            {7, "Irradiance map"},
            {8, "BRDF LUT"},
        };

        static constexpr FRenderDebugView ShadowCycle[] = {
            {24, "Shadow Occlusion Mask"},
            {25, "CSM False-Color Cascades"},
            {26, "Spot Shadow Factor"},
            {40, "Point Shadow Factor"},
            {27, "Cascade 0 Depth"},
            {28, "Cascade 1 Depth"},
            {29, "Cascade 2 Depth"},
            {30, "Cascade 3 Depth"},
        };

        static constexpr const char* PostProcessCycle[] = {
            "Post: Full Composite",
            "Post: Raw HDR (linear scene)",
            "Post: Bloom only",
            "Post: Bright pass only",
            "Post: Tone map (no FXAA)",
            "Post: SSAO only",
        };

        static constexpr int PostProcessCycleCount = 6;

        static int NextShaderMode(int InCurrent, std::span<const FRenderDebugView> InCycle, const char*& OutName);
        static const char* CycleNameOrNull(int InMode, std::span<const FRenderDebugView> InCycle);
        static const char* ShaderViewName(int InMode);
        static const char* PostViewName(int InPostMode);
        static bool ApplyKey(FRenderDebugState& InOutState, int InKeyCode, const char*& OutLog);
        static bool ApplyKey(FWorldRenderer& InRenderer, int InKeyCode);
    };

} // namespace Leon
