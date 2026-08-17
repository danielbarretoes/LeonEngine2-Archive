#pragma once

#include <cstdint>
#include <chrono>
#include <cstring>

namespace Leon {

    /**
     * CPU frame timings and net/render counters for the F1 diagnostics HUD.
     * GPU ms is filled when a GL timer query is available; otherwise 0.
     */
    struct FFrameTiming {
        float FrameMs = 0.0f;
        float GameMs = 0.0f;
        float InputMs = 0.0f;
        float GameModeMs = 0.0f;
        float ControllersMs = 0.0f;
        float CharactersMs = 0.0f;
        float AIMs = 0.0f;
        float PhysicsMs = 0.0f;
        float NetworkMs = 0.0f;
        float AnimationMs = 0.0f;
        float RenderMs = 0.0f;
        float ShadowMs = 0.0f;
        float OpaqueMs = 0.0f;
        float TransparentMs = 0.0f;
        float SkyMs = 0.0f;
        float IBLMs = 0.0f;
        float PostProcessMs = 0.0f;
        float UIMs = 0.0f;
        float GPUMs = 0.0f;

        uint32_t ShadowDrawCalls = 0;
        uint32_t ShaderChanges = 0;
        uint32_t TextureBinds = 0;
        uint32_t MaterialBatches = 0;
        int32_t VisibleActors = 0;
        int32_t CulledActors = 0;
        int32_t ReplicatedActors = 0;

        float PingMs = 0.0f;
        uint32_t PacketsSent = 0;
        uint32_t PacketsReceived = 0;
        uint32_t BytesPerSec = 0;
    };

    class FFrameProfiler {
    public:
        class FScope {
        public:
            explicit FScope(float* InTargetMs) : TargetMs(InTargetMs) {
                Start = std::chrono::high_resolution_clock::now();
            }
            ~FScope() {
                if (!TargetMs)
                    return;
                auto end = std::chrono::high_resolution_clock::now();
                *TargetMs += std::chrono::duration<float, std::milli>(end - Start).count();
            }
            FScope(const FScope&) = delete;
            FScope& operator=(const FScope&) = delete;

        private:
            float* TargetMs = nullptr;
            std::chrono::high_resolution_clock::time_point Start;
        };

        static FFrameTiming& Working() { return sWorking; }
        static const FFrameTiming& Last() { return sLast; }

        static void BeginFrame() { sWorking = FFrameTiming{}; }
        static void EndFrame(float InFrameMs) {
            sWorking.FrameMs = InFrameMs;
            sLast = sWorking;
        }

    private:
        static inline FFrameTiming sWorking{};
        static inline FFrameTiming sLast{};
    };

} // namespace Leon
