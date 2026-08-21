#pragma once

#include "Core/Base.hpp"
#include <memory>
#include <string>

namespace Leon::Editor {

    /**
     * @brief Interface for undoable editor commands.
     */
    class IEditorCommand {
    public:
        virtual ~IEditorCommand() = default;

        virtual void Execute() = 0;
        virtual void Undo() = 0;
        [[nodiscard]] virtual std::string GetDescription() const = 0;
        /** Selection-only commands must return false so Ctrl+Z of a pick does not dirty the map. */
        [[nodiscard]] virtual bool AffectsMap() const { return true; }
    };

} // namespace Leon::Editor

namespace Leon {
    using Editor::IEditorCommand;
} // namespace Leon
