#pragma once

#include "ToolsCommon/ICommand.hpp"

namespace Leon::Tools {

    class FBakeCommand : public ICommand {
    public:
        [[nodiscard]] std::string GetName() const override { return "bake"; }
        [[nodiscard]] std::string GetDescription() const override {
            return "Bake static lightmaps and ambient radiometry for a level map.";
        }
        [[nodiscard]] std::string GetUsage() const override {
            return "Lightmass bake --map <path.lmap> [--quality=Preview|Draft|Production] [--force]";
        }

        int Execute(const FCommandArgs& InArgs) override;
        void PrintHelp() const override;
    };

} // namespace Leon::Tools
