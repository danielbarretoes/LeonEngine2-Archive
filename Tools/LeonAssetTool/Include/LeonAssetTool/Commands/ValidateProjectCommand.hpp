#pragma once

#include "LeonAssetTool/ICommand.hpp"

namespace Leon::Tools {

    class FValidateProjectCommand : public ICommand {
    public:
        [[nodiscard]] std::string GetName() const override { return "validate_project"; }
        [[nodiscard]] std::string GetDescription() const override {
            return "Validate a .lproject descriptor, directories, configuration INIs, and default map.";
        }
        [[nodiscard]] std::string GetUsage() const override {
            return "LeonAssetTool validate_project --project <path.lproject>";
        }

        int Execute(const FCommandArgs& InArgs) override;
        void PrintHelp() const override;
    };

} // namespace Leon::Tools
