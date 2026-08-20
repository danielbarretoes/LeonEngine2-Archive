#pragma once

#include <functional>
#include <leon/Engine.h>
#include <leon/gameplay/GameplayRouter.h>
#include <string_view>

namespace leon::editor {

using PieRegisterModesFn = std::function<void(Engine&, GameplayRouter&)>;

/// PIE pack lookup: templates (Blank / ThirdPerson) and repo product packs (Showcase / Furytoon).
struct PiePackInfo {
    /// False → unknown project; Editor falls back to Default / ThirdPersonGameMode by GM id.
    bool known = false;
    /// May be empty for packs with only DefaultGameMode (e.g. Blank).
    PieRegisterModesFn registerModes;
};

/// Lookup by project folder name or `templateId` (`ThirdPerson`, `Blank`, …).
[[nodiscard]] PiePackInfo FindPiePack(std::string_view projectName);

} // namespace leon::editor
