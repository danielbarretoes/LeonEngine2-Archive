#pragma once

#include "Core/FProjectDescriptor.hpp"
#include "Engine/IGameModule.hpp"

#include <string>

namespace Leon::Editor {

    /**
     * @brief Loads LE_RegisterGameModule for the active project (in-process or DLL).
     */
    class FGameModuleLoader {
    public:
        static bool IsLoaded() { return bLoaded; }
        static const FGameModuleHooks& GetHooks() { return Hooks; }

        /** Unload is a no-op for in-process modules; clears hooks state. */
        static void Unload();

        /**
         * @brief Register gameplay classes for InProject.
         * @param InProjectPath Absolute path to the .lproject file.
         * @param InProjectDir Project directory (parent of Content/).
         */
        static bool LoadForProject(const FProjectDescriptor& InProject, const std::string& InProjectPath,
                                   const std::string& InProjectDir);

    private:
        static bool TryLoadDynamicLibrary(const std::string& InDllPath);
        static bool TryLoadInProcess(const std::string& InProjectName);

        static bool bLoaded;
        static FGameModuleHooks Hooks;
#ifdef _WIN32
        static void* ModuleHandle;
#endif
    };

} // namespace Leon::Editor
