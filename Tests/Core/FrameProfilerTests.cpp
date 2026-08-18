#include <doctest/doctest.h>

#include "Core/FFrameProfiler.hpp"
#include "Core/FPerformanceTimer.hpp"

#include <thread>
#include <vector>
#include <chrono>

TEST_SUITE("Frame profiler") {
    TEST_CASE("percentiles are ordered and match a known set") {
        std::vector<float> values;
        for (int i = 1; i <= 100; ++i)
            values.push_back(static_cast<float>(i));
        auto p = Leon::FFrameStatsCollector::ComputePercentiles(values);
        CHECK(p.Samples == 100);
        CHECK(p.Min == doctest::Approx(1.0f));
        CHECK(p.Max == doctest::Approx(100.0f));
        CHECK(p.Average == doctest::Approx(50.5f).epsilon(0.01));
        CHECK(p.Median == doctest::Approx(50.5f).epsilon(0.02));
        CHECK(p.P1 <= p.P5);
        CHECK(p.P5 <= p.Median);
        CHECK(p.Median <= p.P95);
        CHECK(p.P95 <= p.P99);
        CHECK(p.P1 == doctest::Approx(1.99f).epsilon(0.05));
        CHECK(p.P99 == doctest::Approx(99.01f).epsilon(0.05));
    }

    TEST_CASE("scoped timer accumulates") {
        float ms = 0.0f;
        {
            Leon::FScopedTimer timer(&ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        CHECK(ms >= 4.0f);
        CHECK(ms < 50.0f);
    }

    TEST_CASE("collector warmup skips early frames") {
        Leon::FFrameStatsCollector::Reset();
        for (int i = 0; i < 10; ++i) {
            Leon::FFrameTiming t;
            t.FrameMs = 16.0f;
            t.GameMs = 1.0f;
            Leon::FFrameStatsCollector::Capture(t);
        }
        for (int i = 0; i < 20; ++i) {
            Leon::FFrameTiming t;
            t.FrameMs = 10.0f;
            t.GameMs = 4.0f;
            Leon::FFrameStatsCollector::Capture(t);
        }
        auto summary = Leon::FFrameStatsCollector::Compute(0.16f);
        CHECK(summary.SampleCount >= 10);
        CHECK(summary.Frame.Average == doctest::Approx(10.0f).epsilon(0.05));
        CHECK(summary.Game.Average == doctest::Approx(4.0f).epsilon(0.05));
        CHECK(Leon::FFrameStatsCollector::BoundClassName(summary.BoundClass) != nullptr);
    }
}
