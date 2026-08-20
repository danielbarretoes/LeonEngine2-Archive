#include "ToolsCommon/CommandRegistry.hpp"
#include "AssetTool/Commands/ImportCommand.hpp"
#include "AssetTool/Commands/ValidateCommand.hpp"
#include "AssetTool/Commands/InspectCommand.hpp"

#include <memory>

int main(int argc, char** argv) {
    using namespace Leon::Tools;

    FCommandRegistry registry("LeonEngine2 Asset Tool (Asset Pipeline & Cooking)");
    registry.RegisterCommand(std::make_unique<FImportCommand>());
    registry.RegisterCommand(std::make_unique<FValidateCommand>());
    registry.RegisterCommand(std::make_unique<FInspectCommand>());

    return registry.Dispatch(argc, argv);
}
