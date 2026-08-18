#pragma once

#include "Core/FPerformanceTimer.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Leon {

    struct FPhysicsStats {
        float StepMs = 0.0f;
        uint32_t BodyCount = 0;
        uint32_t Raycasts = 0;
    };

    struct FAnimationStats {
        float EvaluateMs = 0.0f;
        uint32_t SkeletalMeshes = 0;
        uint32_t BonesEvaluated = 0;
    };

    struct FAIStats {
        float TickMs = 0.0f;
        float NavigationMs = 0.0f;
        uint32_t Controllers = 0;
        uint32_t PathRequests = 0;
    };

    struct FNetworkStats {
        float TickMs = 0.0f;
        float PingMs = 0.0f;
        uint32_t PacketsSent = 0;
        uint32_t PacketsReceived = 0;
        uint32_t BytesPerSec = 0;
        int32_t ReplicatedActors = 0;
    };

    /**
     * GPU timer query slots. Passes are sequential — TIME_ELAPSED queries must not nest.
     */
    namespace EGPUTimerSlot {
        constexpr uint32_t Shadow = 0;
        constexpr uint32_t Opaque = 1;
        constexpr uint32_t Sky = 2;
        constexpr uint32_t Transparent = 3;
        constexpr uint32_t Particles = 4;
        constexpr uint32_t PostProcess = 5;
        constexpr uint32_t Planar = 6;
        constexpr uint32_t Count = 7;
    } // namespace EGPUTimerSlot

    /**
     * One-frame CPU timings and counters for the F1 HUD and FFrameStatsCollector.
     * GPU ms is filled from lagged GL timer queries (typically 2–3 frames late).
     */
    struct FFrameTiming {
        float FrameMs = 0.0f;
        float GameMs = 0.0f;
        float InputMs = 0.0f;
        float GameModeMs = 0.0f;
        float ControllersMs = 0.0f;
        float CharactersMs = 0.0f;
        float AIMs = 0.0f;
        float NavigationMs = 0.0f;
        float PhysicsMs = 0.0f;
        float OverlapsMs = 0.0f;
        float NetworkMs = 0.0f;
        float AnimationMs = 0.0f;
        float RenderMs = 0.0f;
        float CullingMs = 0.0f;
        float ShadowMs = 0.0f;
        float OpaqueMs = 0.0f;
        float TransparentMs = 0.0f;
        float PlanarMs = 0.0f;
        float SkyMs = 0.0f;
        float IBLMs = 0.0f;
        float ParticlesMs = 0.0f;
        float PostProcessMs = 0.0f;
        float UIMs = 0.0f;
        float PresentMs = 0.0f;
        float GPUMs = 0.0f;
        float GPUShadowMs = 0.0f;
        float GPUOpaqueMs = 0.0f;
        float GPUPostProcessMs = 0.0f;
        float GPUPlanarMs = 0.0f;

        uint32_t DrawCalls = 0;
        uint32_t TriangleCount = 0;
        uint32_t ShadowDrawCalls = 0;
        uint32_t ShaderChanges = 0;
        uint32_t TextureBinds = 0;
        uint32_t VAOBinds = 0;
        uint32_t FBOSwitches = 0;
        uint32_t MaterialBatches = 0;
        int32_t VisibleActors = 0;
        int32_t CulledActors = 0;
        int32_t ReplicatedActors = 0;
        int32_t ParticleCount = 0;
        int32_t ActorCount = 0;
        int32_t TickActors = 0;
        int32_t TickComponents = 0;
        uint32_t PathRequests = 0;

        float PingMs = 0.0f;
        uint32_t PacketsSent = 0;
        uint32_t PacketsReceived = 0;
        uint32_t BytesPerSec = 0;
    };

    using FFrameStats = FFrameTiming;

    struct FTimingPercentiles {
        float Average = 0.0f;
        float Median = 0.0f;
        float P1 = 0.0f;
        float P5 = 0.0f;
        float P95 = 0.0f;
        float P99 = 0.0f;
        float Min = 0.0f;
        float Max = 0.0f;
        uint32_t Samples = 0;
    };

    enum class EFrameBoundClass : uint8_t {
        Unknown = 0,
        CPUBound = 1,
        GPUBound = 2,
        Balanced = 3,
        PresentVSyncBound = 4,
    };

    struct FFrameCaptureSummary {
        FTimingPercentiles Frame;
        FTimingPercentiles Game;
        FTimingPercentiles Render;
        FTimingPercentiles GPU;
        FTimingPercentiles Physics;
        FTimingPercentiles Animation;
        FTimingPercentiles AI;
        FTimingPercentiles Navigation;
        FTimingPercentiles Shadow;
        FTimingPercentiles Opaque;
        FTimingPercentiles Planar;
        FTimingPercentiles PostProcess;
        FTimingPercentiles Particles;
        FTimingPercentiles UI;
        FTimingPercentiles Present;
        FTimingPercentiles Characters;
        FTimingPercentiles Overlaps;
        FTimingPercentiles Controllers;

        float AvgCPUWorkMs = 0.0f;
        float AvgGPUMs = 0.0f;
        float AvgPresentMs = 0.0f;
        EFrameBoundClass BoundClass = EFrameBoundClass::Unknown;

        uint32_t SpikeCount = 0;
        uint32_t SampleCount = 0;
        uint32_t HistogramBucketUs = 500;
        std::array<uint32_t, 64> Histogram{};

        float AvgTickActors = 0.0f;
        float AvgTickComponents = 0.0f;
        float AvgActors = 0.0f;
        float AvgDrawCalls = 0.0f;
        float AvgTriangles = 0.0f;
        float AvgShadowDraws = 0.0f;
        float AvgPathRequests = 0.0f;
        float AvgParticles = 0.0f;

        size_t MemoryBeginWorkingSetBytes = 0;
        size_t MemoryEndWorkingSetBytes = 0;
        size_t MemoryPeakWorkingSetBytes = 0;
        size_t MemoryBeginVRAMBytes = 0;
        size_t MemoryEndVRAMBytes = 0;
    };

    class FFrameProfiler {
    public:
        using FScope = FScopedTimer;

        static FFrameTiming& Working() { return sWorking; }
        static const FFrameTiming& Last() { return sLast; }
        static FFrameTiming& LastMutable() { return sLast; }

        static void BeginFrame() { sWorking = FFrameTiming{}; }
        static void EndFrame(float InFrameMs) {
            sWorking.FrameMs = InFrameMs;
            sLast = sWorking;
        }

        static void ApplyGPUTimes(float InShadowMs, float InOpaqueMs, float InPostProcessMs, float InPlanarMs);
        static void SetPresentMs(float InPresentMs) { sLast.PresentMs = InPresentMs; }

    private:
        static inline FFrameTiming sWorking{};
        static inline FFrameTiming sLast{};
    };

    class FFrameStatsCollector {
    public:
        static void Reset();
        static void Capture(const FFrameTiming& InTiming);
        static void MarkMemoryBegin();
        static void MarkMemoryEnd();
        static FFrameCaptureSummary Compute(float InWarmupSeconds = 3.0f);
        static std::string FormatReport(const FFrameCaptureSummary& InSummary);
        static bool WriteReport(const std::string& InPath, const FFrameCaptureSummary& InSummary);
        static const char* BoundClassName(EFrameBoundClass InClass);
        static FTimingPercentiles ComputePercentiles(std::vector<float> InValues);

    private:
        static inline std::vector<FFrameTiming> Samples;
        static inline size_t MemoryBeginWorkingSetBytes = 0;
        static inline size_t MemoryEndWorkingSetBytes = 0;
        static inline size_t MemoryPeakWorkingSetBytes = 0;
        static inline size_t MemoryBeginVRAMBytes = 0;
        static inline size_t MemoryEndVRAMBytes = 0;
        static inline bool bMemoryBeginSet = false;
    };

} // namespace Leon
