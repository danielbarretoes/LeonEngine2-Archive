#pragma once

#include "Editor/Play/EPlayTypes.hpp"

#include <string>

namespace Leon::Editor {

    /**
     * @brief Persisted Play In Editor settings (Unreal-like).
     */
    struct FPlaySettings {
        EPlayNetMode NetMode = EPlayNetMode::Standalone;
        EPlayMode PlayMode = EPlayMode::SelectedViewport;
        int NumberOfPlayers = 1;
        int ListenPort = 7777;
        std::string ClientAddress = "127.0.0.1";
        bool bAutoSaveMapBeforePlay = true;

        void Clamp();
        bool LoadFromFile(const std::string& InPath);
        bool SaveToFile(const std::string& InPath) const;

        bool IsStandaloneViewportReady() const {
            return NumberOfPlayers == 1 && NetMode == EPlayNetMode::Standalone &&
                   PlayMode == EPlayMode::SelectedViewport;
        }
    };

} // namespace Leon::Editor
