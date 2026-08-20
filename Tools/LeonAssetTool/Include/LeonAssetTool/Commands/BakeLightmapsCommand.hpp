#pragma once

#include "LeonAssetTool/ICommand.hpp"

namespace Leon::Tools {

    class FBakeLightmapsCommand : public ICommand {
    public:
        [[nodiscard]] std::string GetName() const override { return "bake_lightmaps"; }
        [[nodiscard]] std::string GetDescription() const override {
            return "Bake static lightmaps for a level using Lightmass.";
        }
        [[nodiscard]] std::string GetUsage() const override {
            return "LeonAssetTool bake_lightmaps --map <path.lmap> [--quality=Preview|Draft|Production] [--force]";
        }

        int Execute(const FCommandArgs& InArgs) override;
        void PrintHelp() const override;
    };

} // namespace Leon::Tools
