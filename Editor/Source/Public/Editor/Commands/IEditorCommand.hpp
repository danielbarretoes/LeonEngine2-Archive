#pragma once

#include "Core/Base.hpp"
#include <memory>
#include <string>

namespace Leon {

    /**
     * @brief Interface for undoable editor commands.
     */
    class IEditorCommand {
    public:
        virtual ~IEditorCommand() = default;

        virtual void Execute() = 0;
        virtual void Undo() = 0;
        [[nodiscard]] virtual std::string GetDescription() const = 0;
    };

} // namespace Leon
