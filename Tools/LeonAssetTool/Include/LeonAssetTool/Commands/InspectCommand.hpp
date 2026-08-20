#pragma once

#include "LeonAssetTool/ICommand.hpp"

namespace Leon::Tools {

    class FInspectCommand : public ICommand {
    public:
        [[nodiscard]] std::string GetName() const override { return "inspect"; }
        [[nodiscard]] std::string GetDescription() const override {
            return "Inspect binary headers and content structure of any native LeonEngine asset.";
        }
        [[nodiscard]] std::string GetUsage() const override {
            return "LeonAssetTool inspect <file.lhdr | file.ltex | file.lmesh | file.lskeleton | file.lskeletalmesh | "
                   "file.lanim | file.lblend | file.llightmap | file.lmat>";
        }

        int Execute(const FCommandArgs& InArgs) override;
        void PrintHelp() const override;
    };

} // namespace Leon::Tools
