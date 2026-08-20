#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <leon/core/Paths.h>
#include <leon/editor/EditorBuild.h>
#include <leon/editor/EditorOutputLog.h>
#include <leon/editor/EditorToast.h>
#include <regex>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <stdlib.h>
#endif

namespace leon::editor {
namespace fs = std::filesystem;

namespace {

std::atomic<bool> g_buildBusy{false};

[[nodiscard]] const char* BuildProjectScriptName() {
#if defined(_WIN32)
    return "build-project.bat";
#else
    return "build-project.sh";
#endif
}

/// Walk upward from seeds until `Scripts/build-project.*` is found (repo or Dist/LeonEditor).
[[nodiscard]] fs::path FindRepoRoot(const fs::path& projectPath = {}) {
    std::vector<fs::path> seeds;
    seeds.push_back(ExecutableDirectory());
    if (!projectPath.empty()) {
        seeds.push_back(projectPath);
    }
    std::error_code ec;
    seeds.push_back(fs::current_path(ec));

    for (fs::path start : seeds) {
        if (start.empty()) {
            continue;
        }
        start = start.lexically_normal();
        fs::path p = start;
        for (int i = 0; i < 8; ++i) {
            const fs::path scripts = p / "Scripts";
            if ((fs::is_regular_file(scripts / "build-project.bat", ec) && !ec) ||
                (fs::is_regular_file(scripts / "build-project.sh", ec) && !ec)) {
                return p;
            }
            const fs::path parent = p.parent_path();
            if (parent.empty() || parent == p) {
                break;
            }
            p = parent;
        }
    }
    return {};
}

[[nodiscard]] fs::path FindBuildProjectScript(const fs::path& projectPath) {
    const fs::path repo = FindRepoRoot(projectPath);
    if (repo.empty()) {
        return {};
    }
    const fs::path script = repo / "Scripts" / BuildProjectScriptName();
    std::error_code ec;
    if (fs::is_regular_file(script, ec) && !ec) {
        return script.lexically_normal();
    }
    return {};
}

void AppendBuildLine(const std::string& line) {
    if (line.find("ERROR") != std::string::npos || line.find("error C") != std::string::npos ||
        line.find("FAILED:") != std::string::npos) {
        EditorLogError(line);
    } else if (line.find("warning") != std::string::npos ||
               line.find("Warning") != std::string::npos) {
        EditorLogWarn(line);
    } else {
        EditorLogInfo(line);
    }
}

void RunScriptProcess(std::string label, std::string cmd, std::string successExtra) {
#if defined(_WIN32)
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (pipe == nullptr) {
        EditorLogError(label + ": failed to start process");
        g_buildBusy.store(false);
        return;
    }

    // Script exit codes: build-project.{bat,sh} echo LEON_BUILD_EXIT=N.
    bool sawExitMarker = false;
    int markerCode = 1;
    char buffer[512];
    std::string pending;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        pending += buffer;
        std::size_t pos = 0;
        while ((pos = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, pos);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                static const char kMarker[] = "LEON_BUILD_EXIT=";
                if (line.rfind(kMarker, 0) == 0) {
                    sawExitMarker = true;
                    try {
                        markerCode = std::stoi(line.substr(sizeof(kMarker) - 1));
                    } catch (...) {
                        markerCode = 1;
                    }
                } else {
                    AppendBuildLine(line);
                }
            }
            pending.erase(0, pos + 1);
        }
    }
    if (!pending.empty()) {
        if (pending.back() == '\r') {
            pending.pop_back();
        }
        if (!pending.empty()) {
            static const char kMarker[] = "LEON_BUILD_EXIT=";
            if (pending.rfind(kMarker, 0) == 0) {
                sawExitMarker = true;
                try {
                    markerCode = std::stoi(pending.substr(sizeof(kMarker) - 1));
                } catch (...) {
                    markerCode = 1;
                }
            } else {
                AppendBuildLine(pending);
            }
        }
    }

#if defined(_WIN32)
    const int pcloseCode = _pclose(pipe);
#else
    const int pcloseCode = pclose(pipe);
#endif
    // Prefer script marker; else treat non-zero pclose as failure (may be code<<8 on Windows).
    int code = sawExitMarker ? markerCode : pcloseCode;
    if (!sawExitMarker && code != 0 && (code & 0xff) == 0) {
        code = code >> 8;
    }
    if (code == 0) {
        EditorLogInfo(label + ": succeeded");
        if (!successExtra.empty()) {
            EditorLogInfo(successExtra);
        }
        EditorToast(label + " succeeded", EEditorToastKind::Success, 4.5f);
    } else {
        EditorLogError(label + ": failed (exit " + std::to_string(code) + ")");
        EditorToast(label + " failed", EEditorToastKind::Error, 5.0f);
    }
    g_buildBusy.store(false);
}

[[nodiscard]] bool TryBeginBusy(const char* alreadyMsg) {
    bool expected = false;
    if (!g_buildBusy.compare_exchange_strong(expected, true)) {
        EditorLogWarn(alreadyMsg);
        return false;
    }
    return true;
}

[[nodiscard]] std::string ReadCmakeListsText(const fs::path& projectPath) {
    std::ifstream in(projectPath / "CMakeLists.txt");
    if (!in.is_open()) {
        return {};
    }
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/// True when pack CMakeLists declares `add_executable(leon-*-server …)`.
[[nodiscard]] bool HasDedicatedServerTarget(const std::string& cmakeContents) {
    if (cmakeContents.empty()) {
        return false;
    }
    static const std::regex kServer(R"(add_executable\s*\(\s*leon-[A-Za-z0-9_]+-server)");
    return std::regex_search(cmakeContents, kServer);
}

[[nodiscard]] std::string ResolveCmakeTargetFromContents(const std::string& cmakeContents,
                                                         const std::string& projectPath,
                                                         const std::string& projectName) {
    if (!cmakeContents.empty()) {
        static const std::regex kExe(R"(add_executable\s*\(\s*(leon-[A-Za-z0-9_]+))");
        std::sregex_iterator it(cmakeContents.begin(), cmakeContents.end(), kExe);
        const std::sregex_iterator end;
        for (; it != end; ++it) {
            const std::string name = (*it)[1].str();
            if (name.size() >= 7 && name.compare(name.size() - 7, 7, "-server") == 0) {
                continue;
            }
            return name;
        }
    }
    if (!projectName.empty()) {
        return "leon-" + projectName;
    }
    return "leon-" + fs::path(projectPath).filename().string();
}

} // namespace

bool EditorBuild::IsBusy() {
    return g_buildBusy.load();
}

std::string EditorBuild::ResolveCmakeTarget(const std::string& projectPath,
                                            const std::string& projectName) {
    return ResolveCmakeTargetFromContents(ReadCmakeListsText(fs::path(projectPath)), projectPath,
                                          projectName);
}

bool EditorBuild::StartProjectBuild(const std::string& projectPath, const std::string& projectName,
                                    bool buildDedicatedServer) {
    if (projectPath.empty()) {
        EditorLogError("Build Project: no project open");
        return false;
    }
    if (!TryBeginBusy("Build: already running")) {
        return false;
    }

    const fs::path proj = fs::path(projectPath).lexically_normal();
    if (!fs::is_regular_file(proj / "CMakeLists.txt")) {
        EditorLogError("Build Project: missing CMakeLists.txt in " + proj.string());
        g_buildBusy.store(false);
        return false;
    }
    const std::string cmakeContents = ReadCmakeListsText(proj);

    const fs::path script = FindBuildProjectScript(proj);
    if (script.empty()) {
        EditorLogError(std::string("Build Project: Scripts/") + BuildProjectScriptName() +
                       " not found");
        EditorLogError("  Use Scripts\\package-editor.bat to make a portable Dist\\LeonEditor");
        EditorLogError("  exe: " + ExecutableDirectory().string());
        g_buildBusy.store(false);
        return false;
    }

    const std::string target =
        ResolveCmakeTargetFromContents(cmakeContents, proj.string(), projectName);
    bool withServer = buildDedicatedServer;
    // Blank/ThirdPerson scaffolds have no leon-*-server target — do not fail Build Game.
    if (withServer && !HasDedicatedServerTarget(cmakeContents)) {
        withServer = false;
        EditorLogWarn("Build Game: dedicated server requested but no leon-*-server target — "
                      "building client only");
        EditorToast("No dedicated server target in this pack — building client only",
                    EEditorToastKind::Warning, 5.0f);
    }

    const std::string shipHint = "Game output: " + (proj / "Shipping").lexically_normal().string() +
                                 "  (copy that folder to another PC)";

    EditorLogInfo("Build Game: starting " + target + (withServer ? " (+ dedicated server)" : "") +
                  " …");
    EditorLogInfo("  project: " + proj.string());
    EditorLogInfo("  script:  " + script.string());
    EditorLogInfo("  output:  <project>/Shipping/  (and build/ while compiling)");

#if defined(_WIN32)
    std::string cmd = "cmd.exe /c \"\"";
    cmd += script.string();
    cmd += "\" \"";
    cmd += proj.string();
    cmd += "\" \"";
    cmd += target;
    cmd += "\"";
    if (withServer) {
        cmd += " --with-server";
    }
    cmd += "\" 2>&1";
#else
    std::string cmd = "bash \"";
    cmd += script.string();
    cmd += "\" \"";
    cmd += proj.string();
    cmd += "\" \"";
    cmd += target;
    cmd += "\"";
    if (withServer) {
        cmd += " --with-server";
    }
    cmd += " 2>&1";
#endif

    std::thread(RunScriptProcess, std::string("Build Game"), std::move(cmd), shipHint).detach();
    return true;
}

} // namespace leon::editor
