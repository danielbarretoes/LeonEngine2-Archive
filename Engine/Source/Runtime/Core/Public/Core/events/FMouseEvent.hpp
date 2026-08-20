#pragma once

#include "Core/events/FEvent.hpp"

namespace Leon {

    class FMouseMovedEvent : public FEvent {
    public:
        FMouseMovedEvent(float InX, float InY) : MouseX(InX), MouseY(InY) {}

        float GetX() const { return MouseX; }
        float GetY() const { return MouseY; }

        std::string ToString() const override {
            std::stringstream ss;
            ss << "FMouseMovedEvent: " << MouseX << ", " << MouseY;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseMoved)
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

    private:
        float MouseX, MouseY;
    };

    class FMouseScrolledEvent : public FEvent {
    public:
        FMouseScrolledEvent(float InXOffset, float InYOffset) : XOffset(InXOffset), YOffset(InYOffset) {}

        float GetXOffset() const { return XOffset; }
        float GetYOffset() const { return YOffset; }

        std::string ToString() const override {
            std::stringstream ss;
            ss << "FMouseScrolledEvent: " << XOffset << ", " << YOffset;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseScrolled)
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

    private:
        float XOffset, YOffset;
    };

    class FMouseButtonEvent : public FEvent {
    public:
        int GetMouseButton() const { return Button; }

        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput | EventCategoryMouseButton)

    protected:
        FMouseButtonEvent(int InButton) : Button(InButton) {}

        int Button;
    };

    class FMouseButtonPressedEvent : public FMouseButtonEvent {
    public:
        FMouseButtonPressedEvent(int InButton) : FMouseButtonEvent(InButton) {}

        std::string ToString() const override {
            std::stringstream ss;
            ss << "FMouseButtonPressedEvent: " << Button;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseButtonPressed)
    };

    class FMouseButtonReleasedEvent : public FMouseButtonEvent {
    public:
        FMouseButtonReleasedEvent(int InButton) : FMouseButtonEvent(InButton) {}

        std::string ToString() const override {
            std::stringstream ss;
            ss << "FMouseButtonReleasedEvent: " << Button;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseButtonReleased)
    };

} // namespace Leon
