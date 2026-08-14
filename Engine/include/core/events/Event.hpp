#pragma once

#include "core/Base.hpp"
#include <sstream>
#include <string>

namespace Leon {

    enum class EEventType {
        None = 0,
        WindowClose,
        WindowResize,
        WindowFocus,
        WindowLostFocus,
        WindowMoved,
        AppTick,
        AppUpdate,
        AppRender,
        KeyPressed,
        KeyReleased,
        KeyTyped,
        MouseButtonPressed,
        MouseButtonReleased,
        MouseMoved,
        MouseScrolled
    };

    enum EEventCategory {
        None = 0,
        EventCategoryApplication = BIT(0),
        EventCategoryInput = BIT(1),
        EventCategoryKeyboard = BIT(2),
        EventCategoryMouse = BIT(3),
        EventCategoryMouseButton = BIT(4)
    };

    using EventType = EEventType;
    using EventCategory = EEventCategory;

#define EVENT_CLASS_TYPE(type)                                                                                         \
    static EEventType GetStaticType() {                                                                                \
        return EEventType::type;                                                                                       \
    }                                                                                                                  \
    virtual EEventType GetEventType() const override {                                                                 \
        return GetStaticType();                                                                                        \
    }                                                                                                                  \
    virtual const char* GetName() const override {                                                                     \
        return #type;                                                                                                  \
    }

#define EVENT_CLASS_CATEGORY(category)                                                                                 \
    virtual int GetCategoryFlags() const override {                                                                    \
        return category;                                                                                               \
    }

    class FEvent {
    public:
        virtual ~FEvent() = default;

        bool bHandled = false;

        virtual EEventType GetEventType() const = 0;
        virtual const char* GetName() const = 0;
        virtual int GetCategoryFlags() const = 0;
        virtual std::string ToString() const { return GetName(); }

        bool IsInCategory(EEventCategory InCategory) const { return GetCategoryFlags() & InCategory; }
    };

    using Event = FEvent;

    class FEventDispatcher {
    public:
        FEventDispatcher(FEvent& InEvent) : m_Event(InEvent) {}

        template <typename T, typename F> bool Dispatch(const F& InFunc) {
            if (m_Event.GetEventType() == T::GetStaticType()) {
                m_Event.bHandled |= InFunc(static_cast<T&>(m_Event));
                return true;
            }
            return false;
        }

    private:
        FEvent& m_Event;
    };

    using EventDispatcher = FEventDispatcher;

} // namespace Leon
