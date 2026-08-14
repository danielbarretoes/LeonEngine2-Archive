#pragma once

#include "core/events/Event.hpp"

namespace Leon {

    class FKeyEvent : public FEvent {
    public:
        int GetKeyCode() const { return m_KeyCode; }

        EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)

    protected:
        FKeyEvent(int InKeyCode) : m_KeyCode(InKeyCode) {}

        int m_KeyCode;
    };

    class FKeyPressedEvent : public FKeyEvent {
    public:
        FKeyPressedEvent(int InKeyCode, bool bInRepeat = false) : FKeyEvent(InKeyCode), bIsRepeat(bInRepeat) {}

        bool IsRepeat() const { return bIsRepeat; }

        std::string ToString() const override {
            std::stringstream ss;
            ss << "KeyPressedEvent: " << m_KeyCode << " (repeat = " << bIsRepeat << ")";
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
            ss << "KeyReleasedEvent: " << m_KeyCode;
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyReleased)
    };

    using KeyEvent = FKeyEvent;
    using KeyPressedEvent = FKeyPressedEvent;
    using KeyReleasedEvent = FKeyReleasedEvent;

} // namespace Leon
