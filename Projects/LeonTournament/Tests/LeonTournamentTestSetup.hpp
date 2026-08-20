#pragma once

#include "Assets/UAssetManager.hpp"
#include "Core/FProjectPaths.hpp"

#include <string>

namespace Leon::Test {

    inline void BindLeonTournamentProject() {
        const std::string project =
            FProjectPaths::LocateProjectFile("Projects/LeonTournament/LeonTournament.lproject");
        if (project.empty())
            return;
        FProjectPaths::SetProjectRoot(project);
        UAssetManager::SetContentRoot(FProjectPaths::ProjectContentDir());
    }

} // namespace Leon::Test
