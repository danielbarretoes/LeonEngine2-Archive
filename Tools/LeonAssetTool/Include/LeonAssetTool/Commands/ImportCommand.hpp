#pragma once

#include "LeonAssetTool/ICommand.hpp"

namespace Leon::Tools {

    class FImportCommand : public ICommand {
    public:
        [[nodiscard]] std::string GetName() const override { return "import"; }
        [[nodiscard]] std::string GetDescription() const override {
            return "Import raw source assets (HDR, Textures, Meshes, Animations) into native runtime format.";
        }
        [[nodiscard]] std::string GetUsage() const override {
            return "LeonAssetTool import --raw <dir> --content <dir> [--force]";
        }

        int Execute(const FCommandArgs& InArgs) override;
        void PrintHelp() const override;
    };

} // namespace Leon::Tools
