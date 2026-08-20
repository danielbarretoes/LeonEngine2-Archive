#pragma once

#include "LeonAssetTool/ICommand.hpp"

namespace Leon::Tools {

    class FValidateMapCommand : public ICommand {
    public:
        [[nodiscard]] std::string GetName() const override { return "validate_map"; }
        [[nodiscard]] std::string GetDescription() const override {
            return "Validate actor structure and referenced asset existence in a .lmap level file.";
        }
        [[nodiscard]] std::string GetUsage() const override { return "LeonAssetTool validate_map --map <path.lmap>"; }

        int Execute(const FCommandArgs& InArgs) override;
        void PrintHelp() const override;
    };

} // namespace Leon::Tools
