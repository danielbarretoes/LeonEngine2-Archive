#pragma once

#include "engine/core/events/Event.hpp"

namespace Leon {

    class FMouseMovedEvent : public FEvent {
    public:
        FMouseMovedEvent(float InX, float InY) : m_MouseX(InX), m_MouseY(InY) {}

        float GetX() const { return m_MouseX; }
        float GetY() const { return m_MouseY; }

        std::string ToString() const override {
            std::stringstream ss;
            ss << "MouseMovedEvent: " << m_MouseX << ", " << m_MouseY;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseMoved)
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

    private:
        float m_MouseX, m_MouseY;
    };

    class FMouseScrolledEvent : public FEvent {
    public:
        FMouseScrolledEvent(float InXOffset, float InYOffset) : m_XOffset(InXOffset), m_YOffset(InYOffset) {}

        float GetXOffset() const { return m_XOffset; }
        float GetYOffset() const { return m_YOffset; }

        std::string ToString() const override {
            std::stringstream ss;
            ss << "MouseScrolledEvent: " << m_XOffset << ", " << m_YOffset;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseScrolled)
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

    private:
        float m_XOffset, m_YOffset;
    };

    class FMouseButtonEvent : public FEvent {
    public:
        int GetMouseButton() const { return m_Button; }

        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput | EventCategoryMouseButton)

    protected:
        FMouseButtonEvent(int InButton) : m_Button(InButton) {}

        int m_Button;
    };

    class FMouseButtonPressedEvent : public FMouseButtonEvent {
    public:
        FMouseButtonPressedEvent(int InButton) : FMouseButtonEvent(InButton) {}

        std::string ToString() const override {
            std::stringstream ss;
            ss << "MouseButtonPressedEvent: " << m_Button;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseButtonPressed)
    };

    class FMouseButtonReleasedEvent : public FMouseButtonEvent {
    public:
        FMouseButtonReleasedEvent(int InButton) : FMouseButtonEvent(InButton) {}

        std::string ToString() const override {
            std::stringstream ss;
            ss << "MouseButtonReleasedEvent: " << m_Button;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseButtonReleased)
    };

    using MouseMovedEvent = FMouseMovedEvent;
    using MouseScrolledEvent = FMouseScrolledEvent;
    using MouseButtonEvent = FMouseButtonEvent;
    using MouseButtonPressedEvent = FMouseButtonPressedEvent;
    using MouseButtonReleasedEvent = FMouseButtonReleasedEvent;

} // namespace Leon
