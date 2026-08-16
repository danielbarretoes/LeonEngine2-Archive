#include <doctest/doctest.h>
#include "Core/FConfigFile.hpp"
#include "Core/FInput.hpp"
#include "Core/FInputSettings.hpp"

#include <filesystem>
#include <fstream>

namespace Leon {

    TEST_SUITE("FInputSettings / DefaultInput.ini") {

        TEST_CASE("FKeyName parses common key strings") {
            CHECK(FKeyName::FromString("W", -1) == Key::W);
            CHECK(FKeyName::FromString("LeftShift", -1) == Key::LeftShift);
            CHECK(FKeyName::FromString("LeftControl", -1) == Key::LeftControl);
            CHECK(FKeyName::FromString("Space", -1) == Key::Space);
            CHECK(FKeyName::FromString("NotAKey", 42) == 42);
        }

        TEST_CASE("LoadFromConfig applies DefaultInput-style keys") {
            namespace fs = std::filesystem;
            fs::path tmp = fs::temp_directory_path() / "leon_input_settings_test.ini";
            {
                std::ofstream out(tmp);
                out << "[/Script/Engine.InputSettings]\n";
                out << "bEnableMouseLook=False\n";
                out << "MoveForwardKey=I\n";
                out << "MoveBackwardKey=K\n";
                out << "MoveLeftKey=J\n";
                out << "MoveRightKey=L\n";
                out << "MoveUpKey=E\n";
                out << "MoveDownKey=Q\n";
                out << "SprintKey=LeftAlt\n";
            }

            FConfigFile cfg;
            REQUIRE(cfg.Load(tmp.string()));

            FInputSettings settings;
            settings.LoadFromConfig(cfg);

            CHECK(settings.bEnableMouseLook == false);
            CHECK(settings.MoveForwardKey == Key::I);
            CHECK(settings.MoveBackwardKey == Key::K);
            CHECK(settings.MoveLeftKey == Key::J);
            CHECK(settings.MoveRightKey == Key::L);
            CHECK(settings.MoveUpKey == Key::E);
            CHECK(settings.MoveDownKey == Key::Q);
            CHECK(settings.SprintKey == Key::LeftAlt);

            fs::remove(tmp);
        }

        TEST_CASE("Sandbox DefaultInput.ini parses if present") {
            namespace fs = std::filesystem;
            fs::path sandboxIni = "Projects/Sandbox/Config/DefaultInput.ini";
            if (!fs::exists(sandboxIni)) {
                return;
            }
            FConfigFile cfg;
            REQUIRE(cfg.Load(sandboxIni.string()));
            FInputSettings settings;
            settings.LoadFromConfig(cfg);
            CHECK(settings.MoveForwardKey == Key::W);
            CHECK(settings.SprintKey == Key::LeftShift);
            CHECK(settings.bEnableMouseLook == true);
        }
    }

} // namespace Leon
