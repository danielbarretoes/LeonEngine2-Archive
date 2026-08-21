#pragma once

#include "ToolsCommon/ICommand.hpp"

namespace Leon::Tools {

    class FExportCommand : public ICommand {
    public:
        [[nodiscard]] std::string GetName() const override { return "export"; }
        [[nodiscard]] std::string GetDescription() const override {
            return "Export native assets back to authoring formats (OBJ/FBX, TGA/BMP, Radiance HDR).";
        }
        [[nodiscard]] std::string GetUsage() const override {
            return "AssetTool export --content <dir> --raw <dir> [--force] [--format auto|obj|fbx|png|tga|jpg|bmp|hdr|exr]\n"
                   "       AssetTool export --asset <file> --out <path> [--format ...]";
        }

        int Execute(const FCommandArgs& InArgs) override;
        void PrintHelp() const override;
    };

} // namespace Leon::Tools
