#include "ToolsCommon/CommandRegistry.hpp"
#include <iostream>
#include <iomanip>

namespace Leon::Tools {

    void FCommandRegistry::RegisterCommand(std::unique_ptr<ICommand> InCommand) {
        if (!InCommand)
            return;
        std::string name = InCommand->GetName();
        CommandMap[name] = InCommand.get();
        Commands.push_back(std::move(InCommand));
    }

    ICommand* FCommandRegistry::FindCommand(const std::string& InName) const {
        auto it = CommandMap.find(InName);
        return (it != CommandMap.end()) ? it->second : nullptr;
    }

    FCommandArgs FCommandRegistry::ParseArgs(int InArgc, char** InArgv) {
        FCommandArgs args;
        if (InArgc < 2)
            return args;

        args.CommandName = InArgv[1];

        for (int i = 2; i < InArgc; ++i) {
            std::string arg = InArgv[i];
            if (arg.rfind("--", 0) == 0) {
                std::string flag = arg.substr(2);
                size_t eq = flag.find('=');
                if (eq != std::string::npos) {
                    std::string key = flag.substr(0, eq);
                    std::string val = flag.substr(eq + 1);
                    args.Options[key] = val;
                } else if (i + 1 < InArgc && InArgv[i + 1][0] != '-') {
                    args.Options[flag] = InArgv[++i];
                } else {
                    args.Flags[flag] = true;
                }
            } else if (arg.rfind("-", 0) == 0) {
                std::string flag = arg.substr(1);
                if (flag == "h") {
                    args.Flags["help"] = true;
                } else if (i + 1 < InArgc && InArgv[i + 1][0] != '-') {
                    args.Options[flag] = InArgv[++i];
                } else {
                    args.Flags[flag] = true;
                }
            } else {
                args.PositionalArgs.push_back(arg);
            }
        }

        return args;
    }

    int FCommandRegistry::Dispatch(int InArgc, char** InArgv) {
        if (InArgc < 2) {
            PrintUsage();
            return 1;
        }

        std::string cmdName = InArgv[1];
        if (cmdName == "--help" || cmdName == "-h" || cmdName == "help") {
            PrintUsage();
            return 0;
        }

        ICommand* cmd = FindCommand(cmdName);
        if (!cmd) {
            std::cerr << "[ERROR] Unknown command: " << cmdName << "\n\n";
            PrintUsage();
            return 1;
        }

        FCommandArgs args = ParseArgs(InArgc, InArgv);
        if (args.HasFlag("help")) {
            cmd->PrintHelp();
            return 0;
        }

        return cmd->Execute(args);
    }

    void FCommandRegistry::PrintUsage() const {
        std::cout << "===============================================================\n";
        std::cout << " " << ToolTitle << "\n";
        std::cout << "===============================================================\n\n";
        std::cout << "Usage:\n";
        std::cout << "  <command> [options]\n\n";
        std::cout << "Available Commands:\n";

        for (const auto& cmd : Commands) {
            std::cout << "  " << std::left << std::setw(22) << cmd->GetName() << cmd->GetDescription() << "\n";
        }

        std::cout << "\nFor command-specific help, run with --help:\n";
        std::cout << "  <command> --help\n";
        std::cout << "===============================================================\n";
    }

} // namespace Leon::Tools
