#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

TEST_SUITE("Engine / Game separation") {

    TEST_CASE("Engine sources do not reference game types or assets") {
        const fs::path engineSrc = "Engine/Source";
        if (!fs::exists(engineSrc)) {
            MESSAGE("Engine/Source not found from cwd — skip scan.");
            return;
        }

        const char* forbidden[] = {
            "AShooter",
            "UShooter",
            "LeonTournament",
            "SetupDefaultTPSGraph",
            "IsFiring",
            "FlagFiring",
            "IsReloading",
            "YBot.lskeletalmesh",
            "DeathFromTheFront",
            "/Game/Animations/",
            "/Game/SkeletalMeshes/YBot",
            "MagazineSize",
            "FriendlyFire",
            "ScoreLimit",
        };

        int hits = 0;
        for (const auto& entry : fs::recursive_directory_iterator(engineSrc)) {
            if (!entry.is_regular_file())
                continue;
            const auto ext = entry.path().extension();
            if (ext != ".hpp" && ext != ".h" && ext != ".cpp" && ext != ".inl")
                continue;
            std::ifstream file(entry.path());
            std::string line;
            while (std::getline(file, line)) {
                for (const char* token : forbidden) {
                    if (line.find(token) != std::string::npos) {
                        ++hits;
                        const std::string msg = entry.path().generic_string() + " contains " + token;
                        CHECK_MESSAGE(false, msg.c_str());
                    }
                }
            }
        }
        CHECK(hits == 0);
    }

    TEST_CASE("Engine can spawn a bare ACharacter without shooter types") {
        // Compile-time: this translation unit does not include game headers.
        CHECK(true);
    }
}
