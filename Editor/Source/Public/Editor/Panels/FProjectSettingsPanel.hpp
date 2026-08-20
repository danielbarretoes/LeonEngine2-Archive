#pragma once

#include "Core/Base.hpp"
#include "Core/FProjectDescriptor.hpp"

namespace Leon::Editor {

    /**
     * @brief Project Settings panel for configuring project metadata and default maps.
     */
    class FProjectSettingsPanel {
    public:
        FProjectSettingsPanel() = default;

        void Draw(FProjectDescriptor& InOutDescriptor, const std::string& InProjectPath, bool* bInOutOpen = nullptr);
    };

} // namespace Leon::Editor
