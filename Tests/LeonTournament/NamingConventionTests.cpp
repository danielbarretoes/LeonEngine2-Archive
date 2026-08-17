#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

    bool IsSkippedDir(const fs::path& InPath) {
        for (const auto& part : InPath) {
            const std::string name = part.string();
            if (name == ".git" || name == "build" || name == "out" || name == "ThirdParty" || name == ".vs" ||
                name == "CMakeFiles" || name == "__pycache__")
                return true;
        }
        return false;
    }

    bool HeaderDeclaresPrimaryType(const std::string& InStem, const std::string& InText) {
        if (InStem.ends_with("Types") || InStem.ends_with("Widgets"))
            return true;
        const std::string classDecl = "class " + InStem;
        const std::string structDecl = "struct " + InStem;
        const std::string enumDecl = "enum class " + InStem;
        return InText.find(classDecl) != std::string::npos || InText.find(structDecl) != std::string::npos ||
               InText.find(enumDecl) != std::string::npos;
    }

} // namespace

TEST_SUITE("LeonTournament naming") {

    TEST_CASE("A/U public headers match primary type names") {
        const fs::path root = "Projects/LeonTournament/Source/LeonTournament/Public";
        REQUIRE(fs::exists(root));
        int mismatches = 0;
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file() || IsSkippedDir(entry.path()))
                continue;
            if (entry.path().extension() != ".hpp")
                continue;
            const std::string stem = entry.path().stem().string();
            if (stem.size() < 2)
                continue;
            const char prefix = stem.front();
            if (prefix != 'A' && prefix != 'U')
                continue;
            if (stem.ends_with("Widgets"))
                continue;
            std::ifstream file(entry.path());
            std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (!HeaderDeclaresPrimaryType(stem, text)) {
                ++mismatches;
                CHECK_MESSAGE(false, entry.path().generic_string());
            }
        }
        CHECK(mismatches == 0);
    }

    TEST_CASE("content uses supported Unreal-inspired prefixes") {
        const fs::path materials = "Projects/LeonTournament/Content/Materials";
        const fs::path blends = "Projects/LeonTournament/Content/BlendSpaces";
        REQUIRE(fs::exists(materials));
        REQUIRE(fs::exists(blends));

        int materialCount = 0;
        for (const auto& entry : fs::directory_iterator(materials)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".lmat")
                continue;
            ++materialCount;
            CHECK(entry.path().stem().string().rfind("M_", 0) == 0);
        }
        CHECK(materialCount > 0);

        int blendCount = 0;
        for (const auto& entry : fs::directory_iterator(blends)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".lblend")
                continue;
            ++blendCount;
            CHECK(entry.path().stem().string().rfind("BS_", 0) == 0);
        }
        CHECK(blendCount > 0);
    }
}
