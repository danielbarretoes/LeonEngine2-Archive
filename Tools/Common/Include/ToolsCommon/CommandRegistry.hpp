#pragma once

#include "ICommand.hpp"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

namespace Leon::Tools {

    class FCommandRegistry {
    public:
        explicit FCommandRegistry(std::string InToolTitle) : ToolTitle(std::move(InToolTitle)) {}

        void RegisterCommand(std::unique_ptr<ICommand> InCommand);
        ICommand* FindCommand(const std::string& InName) const;
        [[nodiscard]] const std::vector<std::unique_ptr<ICommand>>& GetAllCommands() const { return Commands; }

        int Dispatch(int InArgc, char** InArgv);
        void PrintUsage() const;

    private:
        static FCommandArgs ParseArgs(int InArgc, char** InArgv);

        std::string ToolTitle;
        std::vector<std::unique_ptr<ICommand>> Commands;
        std::unordered_map<std::string, ICommand*> CommandMap;
    };

} // namespace Leon::Tools
