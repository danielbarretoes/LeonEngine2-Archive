#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace Leon::Tools {

    struct FCommandArgs {
        std::string CommandName;
        std::vector<std::string> PositionalArgs;
        std::unordered_map<std::string, std::string> Options;
        std::unordered_map<std::string, bool> Flags;

        [[nodiscard]] bool HasOption(const std::string& InKey) const { return Options.find(InKey) != Options.end(); }

        [[nodiscard]] std::string GetOption(const std::string& InKey, const std::string& InDefault = "") const {
            auto it = Options.find(InKey);
            return (it != Options.end()) ? it->second : InDefault;
        }

        [[nodiscard]] bool HasFlag(const std::string& InKey) const {
            auto it = Flags.find(InKey);
            return (it != Flags.end()) && it->second;
        }
    };

    class ICommand {
    public:
        virtual ~ICommand() = default;

        [[nodiscard]] virtual std::string GetName() const = 0;
        [[nodiscard]] virtual std::string GetDescription() const = 0;
        [[nodiscard]] virtual std::string GetUsage() const = 0;

        virtual int Execute(const FCommandArgs& InArgs) = 0;
        virtual void PrintHelp() const = 0;
    };

} // namespace Leon::Tools
