#pragma once

namespace Leon {

    class FTimestep {
    public:
        FTimestep(float InTime = 0.0f) : m_Time(InTime) {}

        operator float() const { return m_Time; }

        float GetSeconds() const { return m_Time; }
        float GetMilliseconds() const { return m_Time * 1000.0f; }

    private:
        float m_Time;
    };

    using Timestep = FTimestep;

} // namespace Leon
