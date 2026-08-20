#pragma once

#include "Core/events/FEvent.hpp"

namespace Leon {

    class FWindowResizeEvent : public FEvent {
    public:
        FWindowResizeEvent(unsigned int InWidth, unsigned int InHeight) : Width(InWidth), Height(InHeight) {}

        unsigned int GetWidth() const { return Width; }
        unsigned int GetHeight() const { return Height; }

        std::string ToString() const override {
            std::stringstream ss;
            ss << "FWindowResizeEvent: " << Width << ", " << Height;
            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowResize)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)

    private:
        unsigned int Width, Height;
    };

    class FWindowCloseEvent : public FEvent {
    public:
        FWindowCloseEvent() = default;

        EVENT_CLASS_TYPE(WindowClose)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

} // namespace Leon
