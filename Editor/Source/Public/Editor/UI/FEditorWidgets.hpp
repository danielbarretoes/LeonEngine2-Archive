#pragma once

#include <glm/glm.hpp>
#include <string>

namespace Leon::Editor {

    class FEditorWidgets {
    public:
        /**
         * @brief Renders a stylized 3-component vector control with RGB colored axis buttons and reset capability.
         */
        static bool DrawVec3Control(const std::string& InLabel, glm::vec3& InValues, float InResetValue = 0.0f,
                                    float InColumnWidth = 80.0f);

        /**
         * @brief Renders a stylized search text box with a clear button when text is present.
         */
        static bool DrawSearchInput(const char* InId, char* InBuffer, size_t InBufferSize,
                                    const char* InHint = "Search...");
    };

} // namespace Leon::Editor

namespace Leon {
    using Editor::FEditorWidgets;
} // namespace Leon
