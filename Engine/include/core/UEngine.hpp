#pragma once

#include "core/Application.hpp"
#include "core/ConfigFile.hpp"
#include "gameplay/UObject.hpp"
#include "world/UGameInstance.hpp"
#include "world/UWorld.hpp"

#include <string>

namespace Leon {

    /**
     * @brief Unreal Engine aligned UEngine runtime singleton.
     *
     * Drives the initialization, config loading, GameInstance, World loading, and engine loop.
     */
    class UEngine : public UObject {
    public:
        UEngine();
        ~UEngine() override;

        static UEngine& Get();

        static int Run(FApplicationCommandLineArgs InArgs,
                       const std::string& InProjectOrConfigPath = "Projects/Sandbox/Sandbox.lproject");

        TRef<UWorld> GetWorld() const { return m_ActiveWorld; }
        TRef<UGameInstance> GetGameInstance() const { return m_GameInstance; }

    private:
        int InternalRun(FApplicationCommandLineArgs InArgs, const std::string& InConfigPath);

        TRef<UGameInstance> m_GameInstance;
        TRef<UWorld> m_ActiveWorld;
    };

} // namespace Leon
