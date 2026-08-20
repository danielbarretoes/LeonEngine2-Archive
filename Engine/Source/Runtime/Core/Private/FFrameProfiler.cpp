#include "Core/FFrameProfiler.hpp"
#include "Core/FPlatformMemory.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace Leon {

    void FFrameProfiler::ApplyGPUTimes(float InShadowMs, float InOpaqueMs, float InPostProcessMs, float InPlanarMs) {
        sLast.GPUShadowMs = InShadowMs;
        sLast.GPUOpaqueMs = InOpaqueMs;
        sLast.GPUPostProcessMs = InPostProcessMs;
        sLast.GPUPlanarMs = InPlanarMs;
        sLast.GPUMs = InShadowMs + InOpaqueMs + InPostProcessMs + InPlanarMs;
    }

    void FFrameStatsCollector::Reset() {
        Samples.clear();
        Samples.reserve(8192);
        MemoryBeginWorkingSetBytes = 0;
        MemoryEndWorkingSetBytes = 0;
        MemoryPeakWorkingSetBytes = 0;
        MemoryBeginVRAMBytes = 0;
        MemoryEndVRAMBytes = 0;
        bMemoryBeginSet = false;
    }

    void FFrameStatsCollector::Capture(const FFrameTiming& InTiming) {
        if (InTiming.FrameMs <= 0.05f)
            return;
        Samples.push_back(InTiming);
    }

    void FFrameStatsCollector::MarkMemoryBegin() {
        const FMemoryStats mem = FPlatformMemory::GetMemoryStats();
        MemoryBeginWorkingSetBytes = mem.WorkingSetBytes;
        MemoryBeginVRAMBytes = mem.UsedVideoMemoryBytes;
        MemoryPeakWorkingSetBytes = mem.PeakWorkingSetBytes;
        bMemoryBeginSet = true;
    }

    void FFrameStatsCollector::MarkMemoryEnd() {
        const FMemoryStats mem = FPlatformMemory::GetMemoryStats();
        MemoryEndWorkingSetBytes = mem.WorkingSetBytes;
        MemoryEndVRAMBytes = mem.UsedVideoMemoryBytes;
        MemoryPeakWorkingSetBytes = mem.PeakWorkingSetBytes;
        if (!bMemoryBeginSet) {
            MemoryBeginWorkingSetBytes = mem.WorkingSetBytes;
            MemoryBeginVRAMBytes = mem.UsedVideoMemoryBytes;
            bMemoryBeginSet = true;
        }
    }

    FTimingPercentiles FFrameStatsCollector::ComputePercentiles(std::vector<float> InValues) {
        FTimingPercentiles out;
        if (InValues.empty())
            return out;
        std::sort(InValues.begin(), InValues.end());
        out.Samples = static_cast<uint32_t>(InValues.size());
        out.Min = InValues.front();
        out.Max = InValues.back();
        double sum = 0.0;
        for (float v : InValues)
            sum += static_cast<double>(v);
        out.Average = static_cast<float>(sum / static_cast<double>(InValues.size()));

        auto at = [&](float p01) -> float {
            if (InValues.size() == 1)
                return InValues.front();
            const double idx = p01 * static_cast<double>(InValues.size() - 1);
            const size_t lo = static_cast<size_t>(std::floor(idx));
            const size_t hi = static_cast<size_t>(std::ceil(idx));
            const float t = static_cast<float>(idx - static_cast<double>(lo));
            return InValues[lo] * (1.0f - t) + InValues[hi] * t;
        };
        out.P1 = at(0.01f);
        out.P5 = at(0.05f);
        out.Median = at(0.50f);
        out.P95 = at(0.95f);
        out.P99 = at(0.99f);
        return out;
    }

    namespace {
        std::vector<float> CollectField(const std::vector<FFrameTiming>& InSamples, float FFrameTiming::* InField) {
            std::vector<float> values;
            values.reserve(InSamples.size());
            for (const auto& s : InSamples)
                values.push_back(s.*InField);
            return values;
        }

        EFrameBoundClass ClassifyBound(float InCPU, float InGPU, float InPresent, float InFrame) {
            const float work = std::max(InCPU, InGPU);
            if (InPresent > 4.0f && InPresent > work * 1.25f && InFrame > work * 1.2f)
                return EFrameBoundClass::PresentVSyncBound;
            if (InGPU > 1.0f && InGPU > InCPU * 1.30f)
                return EFrameBoundClass::GPUBound;
            if (InCPU > 1.0f && InCPU > InGPU * 1.30f)
                return EFrameBoundClass::CPUBound;
            if (InCPU > 0.2f || InGPU > 0.2f)
                return EFrameBoundClass::Balanced;
            return EFrameBoundClass::Unknown;
        }
    } // namespace

    FFrameCaptureSummary FFrameStatsCollector::Compute(float InWarmupSeconds) {
        FFrameCaptureSummary summary;
        summary.MemoryBeginWorkingSetBytes = MemoryBeginWorkingSetBytes;
        summary.MemoryEndWorkingSetBytes = MemoryEndWorkingSetBytes;
        summary.MemoryPeakWorkingSetBytes = MemoryPeakWorkingSetBytes;
        summary.MemoryBeginVRAMBytes = MemoryBeginVRAMBytes;
        summary.MemoryEndVRAMBytes = MemoryEndVRAMBytes;

        std::vector<FFrameTiming> window;
        window.reserve(Samples.size());
        float elapsed = 0.0f;
        for (const auto& s : Samples) {
            elapsed += s.FrameMs * 0.001f;
            if (elapsed < InWarmupSeconds)
                continue;
            if (s.FrameMs > 100.0f) {
                ++summary.SpikeCount;
                continue;
            }
            window.push_back(s);
        }
        if (window.empty())
            window = Samples;
        summary.SampleCount = static_cast<uint32_t>(window.size());
        if (window.empty())
            return summary;

        summary.Frame = ComputePercentiles(CollectField(window, &FFrameTiming::FrameMs));
        summary.Game = ComputePercentiles(CollectField(window, &FFrameTiming::GameMs));
        summary.Render = ComputePercentiles(CollectField(window, &FFrameTiming::RenderMs));
        summary.GPU = ComputePercentiles(CollectField(window, &FFrameTiming::GPUMs));
        summary.Physics = ComputePercentiles(CollectField(window, &FFrameTiming::PhysicsMs));
        summary.Animation = ComputePercentiles(CollectField(window, &FFrameTiming::AnimationMs));
        summary.AI = ComputePercentiles(CollectField(window, &FFrameTiming::AIMs));
        summary.Navigation = ComputePercentiles(CollectField(window, &FFrameTiming::NavigationMs));
        summary.Shadow = ComputePercentiles(CollectField(window, &FFrameTiming::ShadowMs));
        summary.Opaque = ComputePercentiles(CollectField(window, &FFrameTiming::OpaqueMs));
        summary.Planar = ComputePercentiles(CollectField(window, &FFrameTiming::PlanarMs));
        summary.PostProcess = ComputePercentiles(CollectField(window, &FFrameTiming::PostProcessMs));
        summary.Particles = ComputePercentiles(CollectField(window, &FFrameTiming::ParticlesMs));
        summary.UI = ComputePercentiles(CollectField(window, &FFrameTiming::UIMs));
        summary.Present = ComputePercentiles(CollectField(window, &FFrameTiming::PresentMs));
        summary.Characters = ComputePercentiles(CollectField(window, &FFrameTiming::CharactersMs));
        summary.Overlaps = ComputePercentiles(CollectField(window, &FFrameTiming::OverlapsMs));
        summary.Controllers = ComputePercentiles(CollectField(window, &FFrameTiming::ControllersMs));

        double cpu = 0, gpu = 0, present = 0, ticksA = 0, ticksC = 0, actors = 0;
        double draws = 0, tris = 0, shadowDraws = 0, paths = 0, particles = 0;
        uint32_t spikes = summary.SpikeCount;
        const float spikeMs = summary.Frame.Median > 0.1f ? summary.Frame.Median * 2.5f : 33.0f;
        for (const auto& s : window) {
            cpu += static_cast<double>(s.GameMs + s.RenderMs + s.UIMs);
            gpu += static_cast<double>(s.GPUMs);
            present += static_cast<double>(s.PresentMs);
            ticksA += s.TickActors;
            ticksC += s.TickComponents;
            actors += s.ActorCount;
            draws += s.DrawCalls;
            tris += s.TriangleCount;
            shadowDraws += s.ShadowDrawCalls;
            paths += s.PathRequests;
            particles += s.ParticleCount;
            if (s.FrameMs >= spikeMs)
                ++spikes;

            int bucket = static_cast<int>(s.FrameMs / 0.5f);
            if (bucket < 0)
                bucket = 0;
            if (bucket >= static_cast<int>(summary.Histogram.size()))
                bucket = static_cast<int>(summary.Histogram.size()) - 1;
            summary.Histogram[static_cast<size_t>(bucket)]++;
        }
        const double n = static_cast<double>(window.size());
        summary.AvgCPUWorkMs = static_cast<float>(cpu / n);
        summary.AvgGPUMs = static_cast<float>(gpu / n);
        summary.AvgPresentMs = static_cast<float>(present / n);
        summary.AvgTickActors = static_cast<float>(ticksA / n);
        summary.AvgTickComponents = static_cast<float>(ticksC / n);
        summary.AvgActors = static_cast<float>(actors / n);
        summary.AvgDrawCalls = static_cast<float>(draws / n);
        summary.AvgTriangles = static_cast<float>(tris / n);
        summary.AvgShadowDraws = static_cast<float>(shadowDraws / n);
        summary.AvgPathRequests = static_cast<float>(paths / n);
        summary.AvgParticles = static_cast<float>(particles / n);
        summary.SpikeCount = spikes;
        summary.BoundClass =
            ClassifyBound(summary.AvgCPUWorkMs, summary.AvgGPUMs, summary.AvgPresentMs, summary.Frame.Average);
        return summary;
    }

    const char* FFrameStatsCollector::BoundClassName(EFrameBoundClass InClass) {
        switch (InClass) {
        case EFrameBoundClass::CPUBound:
            return "CPU-bound";
        case EFrameBoundClass::GPUBound:
            return "GPU-bound";
        case EFrameBoundClass::Balanced:
            return "CPU/GPU balanced";
        case EFrameBoundClass::PresentVSyncBound:
            return "Present/VSync-bound";
        default:
            return "Unknown";
        }
    }

    std::string FFrameStatsCollector::FormatReport(const FFrameCaptureSummary& InSummary) {
        auto line = [](const char* name, const FTimingPercentiles& p) {
            std::ostringstream os;
            os << name << ": avg=" << p.Average << " median=" << p.Median << " p1=" << p.P1 << " p5=" << p.P5
               << " p95=" << p.P95 << " p99=" << p.P99 << " min=" << p.Min << " max=" << p.Max
               << " samples=" << p.Samples << "\n";
            return os.str();
        };
        std::ostringstream out;
        const float fps = InSummary.Frame.Average > 0.01f ? 1000.0f / InSummary.Frame.Average : 0.0f;
        const float fpsP1 = InSummary.Frame.P99 > 0.01f ? 1000.0f / InSummary.Frame.P99 : 0.0f;
        out << "LeonEngine performance capture\n";
        out << "bound_class=" << BoundClassName(InSummary.BoundClass) << "\n";
        out << "samples=" << InSummary.SampleCount << " spikes=" << InSummary.SpikeCount << "\n";
        out << "avg_fps=" << fps << " fps_at_p99=" << fpsP1 << "\n";
        out << line("frame_ms", InSummary.Frame);
        out << line("game_ms", InSummary.Game);
        out << line("render_ms", InSummary.Render);
        out << line("gpu_ms", InSummary.GPU);
        out << line("physics_ms", InSummary.Physics);
        out << line("animation_ms", InSummary.Animation);
        out << line("characters_ms", InSummary.Characters);
        out << line("ai_ms", InSummary.AI);
        out << line("overlaps_ms", InSummary.Overlaps);
        out << line("controllers_ms", InSummary.Controllers);
        out << line("navigation_ms", InSummary.Navigation);
        out << line("shadow_ms", InSummary.Shadow);
        out << line("opaque_ms", InSummary.Opaque);
        out << line("planar_ms", InSummary.Planar);
        out << line("postprocess_ms", InSummary.PostProcess);
        out << line("particles_ms", InSummary.Particles);
        out << line("ui_ms", InSummary.UI);
        out << line("present_ms", InSummary.Present);
        out << "avg_cpu_work_ms=" << InSummary.AvgCPUWorkMs << "\n";
        out << "avg_gpu_ms=" << InSummary.AvgGPUMs << "\n";
        out << "avg_present_ms=" << InSummary.AvgPresentMs << "\n";
        out << "avg_actors=" << InSummary.AvgActors << " avg_tick_actors=" << InSummary.AvgTickActors
            << " avg_tick_components=" << InSummary.AvgTickComponents << "\n";
        out << "avg_draw_calls=" << InSummary.AvgDrawCalls << " avg_triangles=" << InSummary.AvgTriangles
            << " avg_shadow_draws=" << InSummary.AvgShadowDraws << " avg_path_requests=" << InSummary.AvgPathRequests
            << " avg_particles=" << InSummary.AvgParticles << "\n";
        out << "ram_begin_mb=" << (InSummary.MemoryBeginWorkingSetBytes / (1024.0 * 1024.0))
            << " ram_end_mb=" << (InSummary.MemoryEndWorkingSetBytes / (1024.0 * 1024.0))
            << " ram_peak_mb=" << (InSummary.MemoryPeakWorkingSetBytes / (1024.0 * 1024.0)) << "\n";
        out << "vram_begin_mb=" << (InSummary.MemoryBeginVRAMBytes / (1024.0 * 1024.0))
            << " vram_end_mb=" << (InSummary.MemoryEndVRAMBytes / (1024.0 * 1024.0)) << "\n";
        out << "histogram_0.5ms_buckets=";
        for (size_t i = 0; i < InSummary.Histogram.size(); ++i) {
            if (i)
                out << ",";
            out << InSummary.Histogram[i];
        }
        out << "\n";
        return out.str();
    }

    bool FFrameStatsCollector::WriteReport(const std::string& InPath, const FFrameCaptureSummary& InSummary) {
        std::ofstream out(InPath, std::ios::trunc);
        if (!out)
            return false;
        out << FormatReport(InSummary);
        return static_cast<bool>(out);
    }

} // namespace Leon
