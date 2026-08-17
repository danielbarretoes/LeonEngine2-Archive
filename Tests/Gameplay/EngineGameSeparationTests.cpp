#include <doctest/doctest.h>

#include "Engine/UWorld.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/UCharacterMovementComponent.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"

#include <glm/glm.hpp>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

TEST_SUITE("Engine / Game separation") {

    TEST_CASE("Engine sources do not reference game types or assets") {
        const fs::path engineSrc = "Engine/Source";
        REQUIRE(fs::exists(engineSrc));

        const char* forbidden[] = {
            "AShooter",
            "UShooter",
            "LeonTournament",
            "Projects/LeonTournament",
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

    TEST_CASE("Engine can spawn a bare ACharacter without game-project types") {
        using namespace Leon;
        auto world = UWorld::Create("BareCharacterWorld");
        REQUIRE(world);

        auto* floor = world->SpawnActor<AActor>("Floor");
        REQUIRE(floor);
        floor->SetActorLocation({0.0f, -0.25f, 0.0f});
        floor->SetActorScale({20.0f, 0.5f, 20.0f});
        auto box = floor->AddActorComponent<UBoxComponent>("Box");
        REQUIRE(box);
        box->SetBoxExtent({0.5f, 0.5f, 0.5f});

        auto* character = world->SpawnActor<ACharacter>("Hero");
        REQUIRE(character);
        auto movement = character->GetCharacterMovement();
        REQUIRE(movement);
        character->SetFloorZ(0.0f);
        character->SetActorLocation({0.0f, 1.7f, 0.0f});
        CHECK(character->IsMovingOnGround());

        character->AddMovementInput({0.0f, 0.0f, 1.0f}, 1.0f);
        movement->PerformMovement(0.05f);
        const glm::vec3 velocity = movement->GetVelocity();
        CHECK(glm::length(glm::vec2(velocity.x, velocity.z)) > 0.0f);
    }
}
