#pragma once

#include "ToolsCommon/ICommand.hpp"

namespace Leon::Tools {

    class FValidateCommand : public ICommand {
    public:
        [[nodiscard]] std::string GetName() const override { return "validate"; }
        [[nodiscard]] std::string GetDescription() const override {
            return "Validate integrity of native content assets within a directory.";
        }
        [[nodiscard]] std::string GetUsage() const override { return "AssetTool validate --content <dir>"; }

        int Execute(const FCommandArgs& InArgs) override;
        void PrintHelp() const override;
    };

} // namespace Leon::Tools
