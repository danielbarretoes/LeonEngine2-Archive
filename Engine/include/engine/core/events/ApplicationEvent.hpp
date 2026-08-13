#pragma once

#include "engine/core/events/Event.hpp"

namespace Leon {

    class FWindowResizeEvent : public FEvent {
    public:
        FWindowResizeEvent(unsigned int InWidth, unsigned int InHeight) : m_Width(InWidth), m_Height(InHeight) {}

        unsigned int GetWidth() const { return m_Width; }
        unsigned int GetHeight() const { return m_Height; }

        std::string ToString() const override {
            std::stringstream ss;
            ss << "WindowResizeEvent: " << m_Width << ", " << m_Height;
            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowResize)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)

    private:
        unsigned int m_Width, m_Height;
    };

    class FWindowCloseEvent : public FEvent {
    public:
        FWindowCloseEvent() = default;

        EVENT_CLASS_TYPE(WindowClose)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    using WindowResizeEvent = FWindowResizeEvent;
    using WindowCloseEvent = FWindowCloseEvent;

} // namespace Leon
