#pragma once

#include "gameplay/AActor.hpp"
#include <functional>
#include <string>
#include <unordered_map>

namespace Leon {

    class UWorld;

    using FActorFactory = std::function<AActor*(UWorld*, const std::string&)>;

    /**
     * @brief Unreal Engine aligned ClassRegistry for dynamic spawning of game framework classes configured via INI.
     */
    class UClassRegistry {
    public:
        static UClassRegistry& Get();

        template <typename T> void RegisterClass(const std::string& InClassName) {
            m_Factories[InClassName] = [](UWorld* InWorld, const std::string& InName) -> AActor* {
                return InWorld->SpawnActor<T>(InName);
            };
        }

        AActor* CreateActorOfClass(const std::string& InClassName, UWorld* InWorld, const std::string& InName = "");
        bool HasClass(const std::string& InClassName) const;

    private:
        UClassRegistry();
        void RegisterBuiltins();

        std::unordered_map<std::string, FActorFactory> m_Factories;
    };

} // namespace Leon
