#pragma once

#include "Core/Base.hpp"
#include "Engine/UEngine.hpp"
#include "Gameplay/UClassRegistry.hpp"

#include <functional>

namespace Leon {

    /**
     * @brief Optional hooks supplied by a game module during registration.
     */
    struct FGameModuleHooks {
        UEngine::FGameInstanceFactory GameInstanceFactory;
        std::function<void()> TransportFactorySetup;
    };

    /**
     * @brief C ABI entry point expected from game module DLLs (LE_RegisterGameModule).
     */
    using FRegisterGameModuleFn = void (*)(UClassRegistry& InRegistry, FGameModuleHooks& OutHooks);

} // namespace Leon

#if defined(_WIN32)
#define LE_GAME_MODULE_EXPORT extern "C" __declspec(dllexport)
#else
#define LE_GAME_MODULE_EXPORT extern "C" __attribute__((visibility("default")))
#endif
