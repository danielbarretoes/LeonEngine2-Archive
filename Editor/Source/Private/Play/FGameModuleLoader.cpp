#include "Editor/Play/FGameModuleLoader.hpp"
#include "Core/FLog.hpp"
#include "Engine/UEngine.hpp"
#include "Gameplay/UClassRegistry.hpp"

#include <filesystem>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#if defined(LEON_EDITOR_HAS_TOURNAMENT_MODULE)
extern "C" void LE_RegisterGameModule(Leon::UClassRegistry& InRegistry, Leon::FGameModuleHooks& OutHooks);
#endif

namespace fs = std::filesystem;

namespace Leon::Editor {

    bool FGameModuleLoader::bLoaded = false;
    FGameModuleHooks FGameModuleLoader::Hooks{};
#ifdef _WIN32
    void* FGameModuleLoader::ModuleHandle = nullptr;
#endif

    void FGameModuleLoader::Unload() {
#ifdef _WIN32
        if (ModuleHandle) {
            FreeLibrary(static_cast<HMODULE>(ModuleHandle));
            ModuleHandle = nullptr;
        }
#endif
        Hooks = {};
        bLoaded = false;
    }

    bool FGameModuleLoader::TryLoadInProcess(const std::string& InProjectName) {
#if defined(LEON_EDITOR_HAS_TOURNAMENT_MODULE)
        if (InProjectName == "LeonTournament") {
            Hooks = {};
            LE_RegisterGameModule(UClassRegistry::Get(), Hooks);
            if (Hooks.TransportFactorySetup)
                Hooks.TransportFactorySetup();
            if (Hooks.GameInstanceFactory)
                UEngine::SetGameInstanceFactory(Hooks.GameInstanceFactory);
            bLoaded = true;
            LE_CORE_INFO("FGameModuleLoader: Registered in-process game module '{}'", InProjectName);
            return true;
        }
#else
        (void)InProjectName;
#endif
        return false;
    }

    bool FGameModuleLoader::TryLoadDynamicLibrary(const std::string& InDllPath) {
#ifdef _WIN32
        if (InDllPath.empty() || !fs::exists(InDllPath))
            return false;

        HMODULE Handle = LoadLibraryA(InDllPath.c_str());
        if (!Handle) {
            LE_CORE_WARN("FGameModuleLoader: LoadLibrary failed for '{}'", InDllPath);
            return false;
        }

        auto* Fn = reinterpret_cast<FRegisterGameModuleFn>(GetProcAddress(Handle, "LE_RegisterGameModule"));
        if (!Fn) {
            LE_CORE_ERROR("FGameModuleLoader: Missing LE_RegisterGameModule in '{}'", InDllPath);
            FreeLibrary(Handle);
            return false;
        }

        Hooks = {};
        Fn(UClassRegistry::Get(), Hooks);
        if (Hooks.TransportFactorySetup)
            Hooks.TransportFactorySetup();
        if (Hooks.GameInstanceFactory)
            UEngine::SetGameInstanceFactory(Hooks.GameInstanceFactory);

        ModuleHandle = Handle;
        bLoaded = true;
        LE_CORE_INFO("FGameModuleLoader: Loaded game module DLL '{}'", InDllPath);
        return true;
#else
        (void)InDllPath;
        return false;
#endif
    }

    bool FGameModuleLoader::LoadForProject(const FProjectDescriptor& InProject, const std::string& InProjectPath,
                                           const std::string& InProjectDir) {
        Unload();

        if (TryLoadInProcess(InProject.ProjectName))
            return true;

        std::vector<fs::path> Candidates;
        if (!InProject.GameModule.empty()) {
            fs::path P = InProject.GameModule;
            if (!P.is_absolute())
                P = fs::path(InProjectDir) / P;
            Candidates.push_back(P);
        }
        Candidates.push_back(fs::path(InProjectDir) / "Binaries" / (InProject.ProjectName + "GameModule.dll"));
        Candidates.push_back(fs::path(InProjectDir) / (InProject.ProjectName + "GameModule.dll"));
        // Dev layout: out/Projects/<Name>/
        fs::path RepoGuess = fs::path(InProjectPath).parent_path().parent_path().parent_path();
        Candidates.push_back(RepoGuess / "out" / "Projects" / InProject.ProjectName /
                             (InProject.ProjectName + "GameModule.dll"));

        for (const fs::path& C : Candidates) {
            if (TryLoadDynamicLibrary(C.string()))
                return true;
        }

        LE_CORE_WARN("FGameModuleLoader: No game module for project '{}' (PIE GameModes may be unavailable)",
                     InProject.ProjectName);
        return false;
    }

} // namespace Leon::Editor
