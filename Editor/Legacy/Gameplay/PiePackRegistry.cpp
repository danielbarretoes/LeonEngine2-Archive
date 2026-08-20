#include <leon/core/Ascii.h>
#include <leon/editor/PiePackRegistry.h>
#include <leon/gameplay/ThirdPersonGameMode.h>
#include <leon/packs/Blank/RegisterModes.h>
#include <leon/packs/Furytoon/RegisterModes.h>
#include <memory>

namespace leon::editor {
namespace {

void RegisterThirdPersonModes(Engine& /*engine*/, GameplayRouter& router) {
    router.AddMode(std::make_unique<ThirdPersonGameMode>());
}

void RegisterShowcaseModes(Engine& engine, GameplayRouter& router) {
    leon::packs::blank::RegisterModes(engine, router);
}

void RegisterFurytoonModes(Engine& engine, GameplayRouter& router) {
    leon::packs::furytoon::RegisterModes(engine, router);
}

} // namespace

PiePackInfo FindPiePack(std::string_view projectName) {
    const std::string key = AsciiToLower(std::string(projectName));
    PiePackInfo info;
    if (key == "thirdperson" || key == "third-person") {
        // Engine ThirdPersonGameMode — same class as Shipping New Project RegisterModes.
        info.known = true;
        info.registerModes = &RegisterThirdPersonModes;
        return info;
    }
    if (key == "blank") {
        // No custom GameModes — GameHostSession uses DefaultGameMode only.
        info.known = true;
        info.registerModes = {};
        return info;
    }
    if (key == "showcase") {
        info.known = true;
        info.registerModes = &RegisterShowcaseModes;
        return info;
    }
    if (key == "furytoon") {
        info.known = true;
        info.registerModes = &RegisterFurytoonModes;
        return info;
    }
    return info;
}

} // namespace leon::editor
