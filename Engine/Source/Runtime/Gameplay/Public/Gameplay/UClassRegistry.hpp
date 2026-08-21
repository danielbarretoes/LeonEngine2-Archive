#pragma once

#include "Gameplay/AActor.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

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
            Factories[InClassName] = [InClassName](UWorld* InWorld, const std::string& InName) -> AActor* {
                AActor* actor = InWorld->SpawnActor<T>(InName);
                if (actor)
                    actor->SetClass(InClassName);
                return actor;
            };
        }

        AActor* CreateActorOfClass(const std::string& InClassName, UWorld* InWorld, const std::string& InName = "");
        bool HasClass(const std::string& InClassName) const;

        /** Sorted list of registered class names (for editor class pickers). */
        std::vector<std::string> GetRegisteredClassNames() const;

        /** Subset whose name contains InSubstring (e.g. "GameMode"). */
        std::vector<std::string> GetRegisteredClassNamesContaining(const std::string& InSubstring) const;

    private:
        UClassRegistry();
        void RegisterBuiltins();

        std::unordered_map<std::string, FActorFactory> Factories;
    };

} // namespace Leon
