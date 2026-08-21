#include "Engine/UEngine.hpp"
#include "Engine/IGameModule.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "FOpenGLRenderDriver.hpp"
#include "FJoltPhysicsDriver.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

LE_GAME_MODULE_EXPORT void LE_RegisterGameModule(Leon::UClassRegistry& InRegistry, Leon::FGameModuleHooks& OutHooks);

namespace {

    std::string ResolveSandboxProjectFile(int argc, char** argv) {
        namespace fs = std::filesystem;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg.rfind("--project=", 0) == 0)
                return arg.substr(10);
            if (arg.rfind("-project=", 0) == 0)
                return arg.substr(9);
            if ((arg == "--project" || arg == "-project") && i + 1 < argc && argv[i + 1])
                return argv[i + 1];
            if (arg.size() > 9 && arg.rfind(".lproject") == arg.size() - 9)
                return arg;
        }

        if (const char* env = std::getenv("LEON_PROJECT")) {
            if (env[0] != '\0')
                return env;
        }

        std::error_code ec;
        fs::path exeDir = fs::absolute(fs::path(argv[0]).parent_path(), ec);
        if (!ec) {
            for (const auto& entry : fs::directory_iterator(exeDir, ec)) {
                if (entry.path().extension() == ".lproject")
                    return entry.path().string();
            }
        }

        const char* candidates[] = {
            "Sandbox.lproject",
            "Projects/Sandbox/Sandbox.lproject",
        };
        for (const char* c : candidates) {
            if (fs::exists(c))
                return c;
        }

        return "Projects/Sandbox/Sandbox.lproject";
    }

} // namespace

int main(int argc, char** argv) {
    Leon::FOpenGLRenderDriver::Register();
    Leon::FJoltPhysicsDriver::Register();

    Leon::FGameModuleHooks Hooks;
    LE_RegisterGameModule(Leon::UClassRegistry::Get(), Hooks);

    Leon::FApplicationCommandLineArgs args{argc, argv};
    const std::string projectFile = ResolveSandboxProjectFile(argc, argv);
    return Leon::UEngine::Run(args, projectFile);
}
