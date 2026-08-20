#pragma once

#include <chrono>

namespace Leon {

    /**
     * Manual high-resolution timer. Not a production per-draw marker — use the
     * hierarchical FFrameProfiler scopes on the main thread.
     */
    class FPerformanceTimer {
    public:
        void Start() {
            Begin = std::chrono::high_resolution_clock::now();
            bRunning = true;
        }

        float StopMs() {
            if (!bRunning)
                return 0.0f;
            bRunning = false;
            auto end = std::chrono::high_resolution_clock::now();
            return std::chrono::duration<float, std::milli>(end - Begin).count();
        }

        float ElapsedMs() const {
            if (!bRunning)
                return 0.0f;
            auto now = std::chrono::high_resolution_clock::now();
            return std::chrono::duration<float, std::milli>(now - Begin).count();
        }

    private:
        std::chrono::high_resolution_clock::time_point Begin{};
        bool bRunning = false;
    };

    /**
     * RAII CPU scope that accumulates milliseconds into a caller-owned float.
     * Matches FFrameProfiler::FScope; kept as a named type for the performance contract.
     */
    class FScopedTimer {
    public:
        explicit FScopedTimer(float* InTargetMs) : TargetMs(InTargetMs) {
            Start = std::chrono::high_resolution_clock::now();
        }
        ~FScopedTimer() {
            if (!TargetMs)
                return;
            auto end = std::chrono::high_resolution_clock::now();
            *TargetMs += std::chrono::duration<float, std::milli>(end - Start).count();
        }
        FScopedTimer(const FScopedTimer&) = delete;
        FScopedTimer& operator=(const FScopedTimer&) = delete;

    private:
        float* TargetMs = nullptr;
        std::chrono::high_resolution_clock::time_point Start{};
    };

} // namespace Leon
