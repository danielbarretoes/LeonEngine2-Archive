#pragma once

#include "Core/events/FEvent.hpp"

namespace Leon {

    class FKeyEvent : public FEvent {
    public:
        int GetKeyCode() const { return KeyCode; }

        EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)

    protected:
        FKeyEvent(int InKeyCode) : KeyCode(InKeyCode) {}

        int KeyCode;
    };

    class FKeyPressedEvent : public FKeyEvent {
    public:
        FKeyPressedEvent(int InKeyCode, bool bInRepeat = false) : FKeyEvent(InKeyCode), bIsRepeat(bInRepeat) {}

        bool IsRepeat() const { return bIsRepeat; }

        std::string ToString() const override {
            std::stringstream ss;
            ss << "FKeyPressedEvent: " << KeyCode << " (repeat = " << bIsRepeat << ")";
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyPressed)

    private:
        bool bIsRepeat;
    };

    class FKeyReleasedEvent : public FKeyEvent {
    public:
        FKeyReleasedEvent(int InKeyCode) : FKeyEvent(InKeyCode) {}

        std::string ToString() const override {
            std::stringstream ss;
            ss << "FKeyReleasedEvent: " << KeyCode;
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyReleased)
    };



} // namespace Leon
