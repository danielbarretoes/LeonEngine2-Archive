#pragma once

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#define BIT(x) (1 << (x))
#define LE_BIND_EVENT_FN(fn)                                                                                           \
    [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

namespace Leon {

    // Unreal Engine Style Smart Pointer & Template Aliases
    template <typename T> using TScope = std::unique_ptr<T>;

    template <typename T, typename... TArgs> constexpr TScope<T> MakeScope(TArgs&&... InArgs) {
        return std::make_unique<T>(std::forward<TArgs>(InArgs)...);
    }

    template <typename T> using TRef = std::shared_ptr<T>;

    template <typename T, typename... TArgs> constexpr TRef<T> MakeRef(TArgs&&... InArgs) {
        return std::make_shared<T>(std::forward<TArgs>(InArgs)...);
    }

    // Convenience engine aliases
    template <typename T> using Scope = TScope<T>;

    template <typename T, typename... TArgs> constexpr Scope<T> CreateScope(TArgs&&... InArgs) {
        return MakeScope<T>(std::forward<TArgs>(InArgs)...);
    }

    template <typename T> using Ref = TRef<T>;

    template <typename T, typename... TArgs> constexpr Ref<T> CreateRef(TArgs&&... InArgs) {
        return MakeRef<T>(std::forward<TArgs>(InArgs)...);
    }

} // namespace Leon
