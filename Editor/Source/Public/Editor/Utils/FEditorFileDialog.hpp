#pragma once

#include "Core/Base.hpp"
#include <string>

namespace Leon::Editor {

    /**
     * @brief Native OS file and folder selection dialog helpers.
     */
    class FEditorFileDialog {
    public:
        static std::string OpenFile(const char* InFilter = nullptr, const char* InTitle = "Open File");
        static std::string SaveFile(const char* InFilter = nullptr, const char* InTitle = "Save File",
                                    const char* InDefaultName = nullptr);
        static std::string PickFolder(const char* InTitle = "Select Folder");
    };

} // namespace Leon::Editor
