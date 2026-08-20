#include "ToolsCommon/CommandRegistry.hpp"
#include "Lightmass/Commands/BakeCommand.hpp"
#include "Lightmass/Commands/ValidateCommand.hpp"

#include <memory>

int main(int argc, char** argv) {
    using namespace Leon::Tools;

    FCommandRegistry registry("LeonEngine2 Lightmass (Static Lighting & Radiometry Baker)");
    registry.RegisterCommand(std::make_unique<FBakeCommand>());
    registry.RegisterCommand(std::make_unique<FValidateCommand>());

    return registry.Dispatch(argc, argv);
}
