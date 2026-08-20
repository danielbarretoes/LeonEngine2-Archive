#include "LeonAssetTool/CommandRegistry.hpp"
#include "LeonAssetTool/Commands/ImportCommand.hpp"
#include "LeonAssetTool/Commands/ValidateCommand.hpp"
#include "LeonAssetTool/Commands/ValidateProjectCommand.hpp"
#include "LeonAssetTool/Commands/ValidateMapCommand.hpp"
#include "LeonAssetTool/Commands/BakeLightmapsCommand.hpp"
#include "LeonAssetTool/Commands/ValidateLightmapsCommand.hpp"
#include "LeonAssetTool/Commands/InspectCommand.hpp"

#include <memory>

int main(int argc, char** argv) {
    using namespace Leon::Tools;

    auto& registry = FCommandRegistry::Get();
    registry.RegisterCommand(std::make_unique<FValidateProjectCommand>());
    registry.RegisterCommand(std::make_unique<FImportCommand>());
    registry.RegisterCommand(std::make_unique<FValidateCommand>());
    registry.RegisterCommand(std::make_unique<FValidateMapCommand>());
    registry.RegisterCommand(std::make_unique<FBakeLightmapsCommand>());
    registry.RegisterCommand(std::make_unique<FValidateLightmapsCommand>());
    registry.RegisterCommand(std::make_unique<FInspectCommand>());

    return registry.Dispatch(argc, argv);
}
