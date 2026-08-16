#pragma once

namespace Leon {

    class FTimestep {
    public:
        FTimestep(float InTime = 0.0f) : Time(InTime) {}

        operator float() const { return Time; }

        float GetSeconds() const { return Time; }
        float GetMilliseconds() const { return Time * 1000.0f; }

    private:
        float Time;
    };

} // namespace Leon
