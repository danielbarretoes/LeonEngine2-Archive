#include "ToolsCommon/CommandRegistry.hpp"
#include "ProjectTool/Commands/ValidateProjectCommand.hpp"
#include "ProjectTool/Commands/ValidateMapCommand.hpp"

#include <memory>

int main(int argc, char** argv) {
    using namespace Leon::Tools;

    FCommandRegistry registry("LeonEngine2 Project Tool (Project & Level Map Integrity)");
    registry.RegisterCommand(std::make_unique<FValidateProjectCommand>());
    registry.RegisterCommand(std::make_unique<FValidateMapCommand>());

    return registry.Dispatch(argc, argv);
}
