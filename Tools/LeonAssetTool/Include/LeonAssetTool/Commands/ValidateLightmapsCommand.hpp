#pragma once

#include "LeonAssetTool/ICommand.hpp"

namespace Leon::Tools {

    class FValidateLightmapsCommand : public ICommand {
    public:
        [[nodiscard]] std::string GetName() const override { return "validate_lightmaps"; }
        [[nodiscard]] std::string GetDescription() const override {
            return "Validate existence and content hash integrity of baked lightmaps for a level.";
        }
        [[nodiscard]] std::string GetUsage() const override {
            return "LeonAssetTool validate_lightmaps --map <path.lmap>";
        }

        int Execute(const FCommandArgs& InArgs) override;
        void PrintHelp() const override;
    };

} // namespace Leon::Tools
