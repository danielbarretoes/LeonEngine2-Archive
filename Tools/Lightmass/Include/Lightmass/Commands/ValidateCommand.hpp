#pragma once

#include "ToolsCommon/ICommand.hpp"

namespace Leon::Tools {

    class FValidateCommand : public ICommand {
    public:
        [[nodiscard]] std::string GetName() const override { return "validate"; }
        [[nodiscard]] std::string GetDescription() const override {
            return "Validate existence and content hash integrity of baked lightmaps for a level.";
        }
        [[nodiscard]] std::string GetUsage() const override { return "Lightmass validate --map <path.lmap>"; }

        int Execute(const FCommandArgs& InArgs) override;
        void PrintHelp() const override;
    };

} // namespace Leon::Tools
