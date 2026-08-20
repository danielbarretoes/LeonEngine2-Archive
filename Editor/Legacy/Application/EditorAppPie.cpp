// Windows before glad: glad defines APIENTRY when unset; windows.h then C4005-redefines it.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <glad/glad.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <leon/core/Ascii.h>
#include <leon/core/Paths.h>
#include <leon/core/Window.h>
#include <leon/editor/EditorApp.h>
#include <leon/editor/EditorBuild.h>
#include <leon/editor/EditorLevelFactory.h>
#include <leon/editor/EditorOutputLog.h>
#include <leon/editor/EditorToast.h>
#include <leon/editor/EditorWindowIcon.h>
#include <leon/editor/PieAspectFit.h>
#include <leon/editor/PiePackRegistry.h>
#include <leon/editor/ShowcaseEditorLog.h>
#include <leon/gameplay/DefaultGameMode.h>
#include <leon/gameplay/GameInstance.h>
#include <leon/gameplay/ThirdPersonGameMode.h>
#include <leon/level/LeonLevelFormat.h>
#include <leon/level/LevelLoader.h>
#include <leon/net/NetProtocol.h>
#include <string>
#include <string_view>
#include <vector>

namespace leon::editor {
namespace fs = std::filesystem;

namespace {

/// Exact open-level catalog key for in-process PIE (includes MainMenu / Lobby).
[[nodiscard]] std::string CurrentEditorLevelKey(const EditorContext& ctx) {
    if (ctx.level != nullptr && !ctx.level->Name().empty()) {
        return ctx.level->Name();
    }
    if (!ctx.levelPath.empty()) {
        return fs::path(ctx.levelPath).stem().string();
    }
    return {};
}

/// Level catalog key for multi Shipping spawn (avoid front-end maps as join targets).
[[nodiscard]] std::string CurrentPiePlayMapKey(const EditorContext& ctx) {
    std::string key = CurrentEditorLevelKey(ctx);
    if (key.empty() || key == "MainMenu" || key == "Lobby") {
        return "Courtyard";
    }
    return key;
}

[[nodiscard]] fs::path FindPackShippingExecutable(const std::string& projectPath,
                                                  const std::string& projectName) {
    if (projectPath.empty()) {
        return {};
    }
    const std::string target = EditorBuild::ResolveCmakeTarget(projectPath, projectName);
    // Prefer product OUTPUT_NAME (<Pack>.exe), then intermediate build/.
    const fs::path candidates[] = {
        fs::path(projectPath) / "Shipping" / (projectName + ".exe"),
        fs::path(projectPath) / "build" / (projectName + ".exe"),
        fs::path(projectPath) / "Shipping" / (target + ".exe"),
        fs::path(projectPath) / "build" / (target + ".exe"),
    };
    std::error_code ec;
    for (const fs::path& c : candidates) {
        if (fs::is_regular_file(c, ec) && !ec) {
            return c;
        }
    }
    // Fallback: first non-server .exe under Shipping/
    const fs::path ship = fs::path(projectPath) / "Shipping";
    if (fs::is_directory(ship, ec)) {
        for (const fs::directory_entry& entry : fs::directory_iterator(ship, ec)) {
            if (ec || !entry.is_regular_file(ec)) {
                continue;
            }
            std::string name = entry.path().filename().string();
            std::string lower = name;
            for (char& ch : lower) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".exe") {
                continue;
            }
            if (lower.find("server") != std::string::npos) {
                continue;
            }
            return entry.path();
        }
    }
    return {};
}

#ifdef _WIN32
[[nodiscard]] bool SpawnDetachedProcess(const fs::path& exe, const std::string& args,
                                        std::uint32_t& outPid) {
    outPid = 0;
    if (exe.empty()) {
        return false;
    }
    std::string cmd = "\"" + exe.lexically_normal().string() + "\"";
    if (!args.empty()) {
        cmd += " ";
        cmd += args;
    }
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> cmdline(cmd.begin(), cmd.end());
    cmdline.push_back('\0');
    const std::string workDir = exe.parent_path().string();
    if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        workDir.empty() ? nullptr : workDir.c_str(), &si, &pi)) {
        return false;
    }
    outPid = static_cast<std::uint32_t>(pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

/// True when something (Shipping --listen) already bound UDP `port` on this machine.
[[nodiscard]] bool IsUdpPortInUse(std::uint16_t port) {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    const bool inUse =
        (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR &&
         WSAGetLastError() == WSAEADDRINUSE);
    closesocket(sock);
    WSACleanup();
    return inUse;
}

#endif

} // namespace

void EditorApp::StartPie(Engine& engine) {
    if (ctx_.piePlaying) {
        return;
    }

    // Unreal-like: Play tests what is on disk. Require a path; auto-save dirty levels.
    if (ctx_.levelPath.empty()) {
        if (!layout_.SaveLevel(ctx_, true)) {
            EditorToast("Play cancelled — save the level first", EEditorToastKind::Warning, 4.5f);
            ctx_.requestPieStart = false;
            return;
        }
    } else if (ctx_.dirty) {
        if (!layout_.SaveLevel(ctx_, false)) {
            EditorToast("Play cancelled — failed to save level", EEditorToastKind::Error, 4.5f);
            ctx_.requestPieStart = false;
            return;
        }
    }

    if (ctx_.lightingOutOfDate) {
        EditorToast("Lighting out of date — build lighting before shipping",
                    EEditorToastKind::Warning, 4.5f);
    }

    ctx_.pieNumberOfPlayers = std::clamp(ctx_.pieNumberOfPlayers, 1, leon::net::kMaxPlayers);
    ctx_.piePaused = false;
    ctx_.requestPiePauseToggle = false;
    ctx_.requestPieStart = false;
    pieShippingSessionOnly_ = false;

    // Unreal Number of Players = total game instances.
    // Listen Server / Client with N>1 → exactly N Shipping windows (editor does not add a Pie
    // pawn).
    if ((ctx_.pieNetMode == EEditorPlayNetMode::ListenServer ||
         ctx_.pieNetMode == EEditorPlayNetMode::Client) &&
        ctx_.pieNumberOfPlayers > 1) {
        pieShippingSessionOnly_ = true;
        pieMode_ = nullptr;
        ctx_.pieNewWindow = false;
        ctx_.editorViewCamera = nullptr;
        engine.SetPlayInputWindow(nullptr);
        engine.SetPlayMouseLookActive(false);
        engine.SetCursorCaptured(false);
        ctx_.world = &world_;
        SpawnPieExtraInstances();
        ctx_.piePlaying = true;
        ctx_.ClearSelection();
        EditorLogInfo("PIE multiplayer session: " + std::to_string(ctx_.pieNumberOfPlayers) +
                      " Shipping window(s), editor stays in edit mode (Stop kills them)");
        EditorToast("Playing " + std::to_string(ctx_.pieNumberOfPlayers) +
                        " shipping instances — editor stays editable; stop closes them",
                    EEditorToastKind::Info, 5.0f);
        return;
    }

    SaveEditorCamera(engine.GetCamera());
    ctx_.pieNewWindow = ctx_.playMode == EEditorPlayMode::NewEditorWindow;

    if (ctx_.pieNewWindow) {
        if (!pieWindow_.CreateShared(engine.GetWindow().Handle(), 1280, 720, "Leon Play")) {
            std::cerr << "Editor: failed to open Play window; falling back to Selected Viewport\n";
            ctx_.pieNewWindow = false;
        } else {
            ApplyLeonWindowIcon(pieWindow_);
            engine.SetPlayInputWindow(&pieWindow_);
            ctx_.editorViewCamera = &editorCameraBackup_;
        }
    } else {
        engine.SetPlayInputWindow(nullptr);
        ctx_.editorViewCamera = nullptr;
    }

    pieMode_ = nullptr;
    pieUsesHostSession_ = false;
    pieRestoreLevelPath_ = ctx_.levelPath;
    ctx_.piePaintPlayOverlay = {};

    // Level override → project globalDefaultGameMode → Default (Blank fly).
    std::string gm = "Default";
    if (ctx_.level != nullptr && !ctx_.level->GameMode().empty()) {
        gm = ctx_.level->GameMode();
    } else if (!ctx_.projectGlobalDefaultGameMode.empty()) {
        gm = ctx_.projectGlobalDefaultGameMode;
    }
    // ThirdPerson spawn contract: PlayerStart + Characters/Bot/Bot.lchar (GM spawns Character).
    if (gm == "third-person" && ctx_.level != nullptr) {
        if (ctx_.level->PlayerStarts().empty()) {
            EditorToast("Third person: place a player start (Place Actors → Basic) before play",
                        EEditorToastKind::Warning, 6.0f);
        }
        const std::string botAbs = ResolveAssetPath("Characters/Bot/Bot.lchar");
        if (botAbs.empty() || !fs::is_regular_file(botAbs)) {
            EditorToast("Third person: missing bot character — import or cook Bot, or use "
                        "New Project → Third person",
                        EEditorToastKind::Warning, 7.0f);
        }
    }

    // Flow: PIE GameMode selection (Unreal-like)
    // 1. Explicit Default → free-look DefaultGameMode (no pack session)
    // 2. Known packs with RegisterModes (ThirdPerson / Showcase / Furytoon) → GameHostSession
    // 3. Blank → DefaultGameMode via session when no custom modes registered
    // 4. third-person without pack session → Engine ThirdPersonGameMode
    // 5. Unknown GM ids on non-pack projects → toast → Default
    // Prefer folder name; New Project copies keep fixed gameplay via templateId.
    // Prefer templateId when it links RegisterModes (e.g. folder named Blank, stamped ThirdPerson).
    PiePackInfo packInfo = FindPiePack(ctx_.projectName);
    if (!ctx_.projectTemplateId.empty()) {
        PiePackInfo fromTemplate = FindPiePack(ctx_.projectTemplateId);
        if (fromTemplate.registerModes || !packInfo.known) {
            packInfo = std::move(fromTemplate);
        }
    }
    const bool usePackSession = packInfo.known && static_cast<bool>(packInfo.registerModes);

    auto bindLocalLevelTravel = [this, &engine]() {
        engine.GetGameInstance().SetLevelTravelFn([this](Engine& e,
                                                         std::string_view levelKey) -> bool {
            fs::path levelsDir;
            if (!ctx_.projectPath.empty()) {
                levelsDir = ProjectContentDirectory(ctx_.projectPath) / "Levels";
            } else if (!ctx_.levelPath.empty()) {
                levelsDir = fs::path(ctx_.levelPath).parent_path();
            }
            if (levelsDir.empty()) {
                return false;
            }
            const std::string needle = AsciiToLower(levelKey);
            std::error_code ec;
            for (const auto& entry : fs::directory_iterator(levelsDir, ec)) {
                if (ec || !entry.is_regular_file()) {
                    continue;
                }
                const fs::path path = entry.path();
                if (AsciiToLower(path.extension().string()) != ".llev") {
                    continue;
                }
                bool match = AsciiToLower(path.stem().string()) == needle;
                if (!match) {
                    LevelDocument doc;
                    if (LoadLeonLevelFile(path.string(), doc) && AsciiToLower(doc.name) == needle) {
                        match = true;
                    }
                }
                if (!match) {
                    continue;
                }
                const std::string resolved = path.lexically_normal().string();
                if (!LoadLevelFile(e, resolved)) {
                    return false;
                }
                ctx_.levelPath = resolved;
                return true;
            }
            return false;
        });
    };

    auto enterLocalMode = [&](std::unique_ptr<GameMode> mode) {
        pieMode_ = std::move(mode);
        ctx_.world = &pieMode_->GetWorld();
        bindLocalLevelTravel();
        pieMode_->OnEnter(engine, ctx_.levelPath);
    };

    // Queue listen/join before OnEnter so template Match/Menu modes spawn as net host/client.
    PreparePieNetPending(engine);

    if (gm == "Default" || gm == "default") {
        enterLocalMode(std::make_unique<DefaultGameMode>());
        if (IsShowcaseProject(ctx_.projectName)) {
            LogShowcaseEditorManifest(ctx_, engine.GetLevel());
        }
    } else if (usePackSession) {
        const std::string levelKey = CurrentEditorLevelKey(ctx_);
        if (!pieSession_.Start(engine, ctx_.projectName.c_str(), packInfo.registerModes, levelKey,
                               ctx_.projectPath)) {
            EditorToast("Play in editor: failed to start pack runtime session", EEditorToastKind::Error, 5.0f);
            pieSession_.Stop();
            pieUsesHostSession_ = false;
            pieRestoreLevelPath_.clear();
            ctx_.piePaintPlayOverlay = {};
            ctx_.pieNewWindow = false;
            ctx_.editorViewCamera = nullptr;
            engine.SetPlayInputWindow(nullptr);
            engine.SetPlayMouseLookActive(false);
            engine.SetCursorCaptured(false);
            DestroyPiePresentResources();
            pieTarget_.Destroy();
            pieWindow_.Destroy();
            RestoreEditorCamera(engine.GetCamera());
            if (!ctx_.projectPath.empty()) {
                SetActiveContentRoot(ctx_.projectPath);
            }
            return;
        }
        pieUsesHostSession_ = true;
        if (GameMode* active = pieSession_.Router().GetActive()) {
            ctx_.world = &active->GetWorld();
        } else {
            ctx_.world = &world_;
        }
        ctx_.piePaintPlayOverlay = [this](int fbW, int fbH) {
            if (ctx_.engine == nullptr) {
                return;
            }
            ctx_.engine->PaintHudAndOverlay(fbW, fbH);
            pieSession_.DrawUi(fbW, fbH);
        };
        EditorLogInfo("PIE: pack Runtime session '" + ctx_.projectName + "' @ " +
                      (levelKey.empty() ? std::string("(default)") : levelKey));
        if (IsShowcaseProject(ctx_.projectName)) {
            LogShowcaseEditorManifest(ctx_, engine.GetLevel());
        }
    } else if (gm == "third-person") {
        enterLocalMode(std::make_unique<ThirdPersonGameMode>());
    } else {
        std::cerr << "Editor PIE: GameMode '" << gm
                  << "' has no Editor-linked gameplay - using Default.\n";
        EditorToast("Play in editor: '" + gm + "' → default (unknown GameMode for this project)",
                    EEditorToastKind::Warning, 6.0f);
        enterLocalMode(std::make_unique<DefaultGameMode>());
    }

    ApplyPieNetMode(engine);
    // Single-instance PIE only (multi Shipping already returned above).
    ctx_.piePlaying = true;
    ctx_.ClearSelection();

    if (ctx_.pieNewWindow) {
        // Capture look on the play window only; keep the editor cursor free.
        engine.SetCursorCaptured(true);
        engine.SetPlayMouseLookActive(true);
        pieWindow_.Show();
        pieWindow_.Focus();
        engine.GetWindow().SetCursorCaptured(false);
        engine.GetWindow().MakeContextCurrent();
    } else {
        // Selected Viewport: hide + lock cursor (GLFW_CURSOR_DISABLED) so look isn't
        // clamped by screen edges. Esc / Pause restores the OS cursor.
        engine.SetCursorCaptured(true);
        engine.SetPlayMouseLookActive(true);
    }
}

void EditorApp::PreparePieNetPending(Engine& engine) {
    // Flow: queue CLI-equivalent pending BEFORE GameMode OnEnter (same as GameApplication).
    ctx_.pieNumberOfPlayers = std::clamp(ctx_.pieNumberOfPlayers, 1, leon::net::kMaxPlayers);
    GameInstance& gi = engine.GetGameInstance();
    gi.CloseNetSession();
    switch (ctx_.pieNetMode) {
    case EEditorPlayNetMode::ListenServer:
        if (ctx_.pieNumberOfPlayers <= 1) {
            gi.RequestListenStart(static_cast<std::uint16_t>(leon::net::kDefaultPort));
            const std::string mapKey = CurrentPiePlayMapKey(ctx_);
            if (!mapKey.empty()) {
                gi.SetPendingPlayMap(mapKey);
            }
        }
        break;
    case EEditorPlayNetMode::Client: {
        const std::string addr =
            ctx_.pieClientAddress.empty() ? "127.0.0.1" : ctx_.pieClientAddress;
        gi.SetPendingJoinAddress(addr);
        const std::string mapKey = CurrentPiePlayMapKey(ctx_);
        if (!mapKey.empty()) {
            gi.SetPendingPlayMap(mapKey);
        }
        break;
    }
    case EEditorPlayNetMode::Standalone:
    default:
        break;
    }
}

void EditorApp::ApplyPieNetMode(Engine& engine) {
    // Flow: Unreal Play Net Mode (after OnEnter consumed pending when supported)
    // 1. Standalone → ensure offline
    // 2. Listen N>1 → editor offline (Shipping hosts)
    // 3. Listen N=1 → toast if already listen; else fallback HostListen (Default/ThirdPerson)
    // 4. Client → toast if already client; else fallback Join
    ctx_.pieNumberOfPlayers = std::clamp(ctx_.pieNumberOfPlayers, 1, leon::net::kMaxPlayers);
    GameInstance& gi = engine.GetGameInstance();
    const auto toastListenOk = [&]() {
        EditorToast("Play in editor listen server on port " + std::to_string(leon::net::kDefaultPort),
                    EEditorToastKind::Success, 3.0f);
        engine.AddOnScreenDebugMessage("PIE Listen Server :" +
                                           std::to_string(leon::net::kDefaultPort),
                                       5.0f, {0.45f, 0.9f, 0.55f});
    };
    const auto toastClientOk = [&](const std::string& addr) {
        EditorToast("Play in editor client connecting to " + addr, EEditorToastKind::Info, 3.0f);
        engine.AddOnScreenDebugMessage("PIE Client → " + addr + ":" +
                                           std::to_string(leon::net::kDefaultPort),
                                       5.0f, {0.55f, 0.8f, 1.0f});
    };

    switch (ctx_.pieNetMode) {
    case EEditorPlayNetMode::ListenServer:
        if (ctx_.pieNumberOfPlayers > 1) {
            gi.CloseNetSession();
            EditorLogInfo("PIE preview local — Shipping Listen Server will host multiplayer");
            EditorToast("Play preview is local — shipping listen server hosts multiplayer",
                        EEditorToastKind::Info, 4.0f);
            break;
        }
        if (gi.IsListenServer()) {
            toastListenOk();
            break;
        }
        gi.ClearPendingListenStart();
        if (gi.HostListen(static_cast<std::uint16_t>(leon::net::kDefaultPort))) {
            toastListenOk();
        } else {
            EditorToast("Play in editor: failed to start listen server", EEditorToastKind::Error, 4.0f);
        }
        break;
    case EEditorPlayNetMode::Client: {
        const std::string addr =
            ctx_.pieClientAddress.empty() ? "127.0.0.1" : ctx_.pieClientAddress;
        if (gi.IsClient()) {
            toastClientOk(addr);
            break;
        }
        (void)gi.ConsumePendingJoinAddress();
        if (gi.Join(addr, static_cast<std::uint16_t>(leon::net::kDefaultPort))) {
            toastClientOk(addr);
        } else {
            EditorToast("Play in editor: join failed (" + addr + ")", EEditorToastKind::Error, 4.0f);
        }
        break;
    }
    case EEditorPlayNetMode::Standalone:
    default:
        gi.CloseNetSession();
        break;
    }
}

void EditorApp::SpawnPieExtraInstances() {
    // Flow: Number of Players (Unreal multi-PIE via Shipping pack processes)
    // Listen Server + N: 1x --listen host now; (N-1)x --join after port bind (async tick)
    // Client + N: N-1 extra --join immediately
    pieSpawnedPids_.clear();
    piePendingClientSpawn_ = false;
    piePendingHostPortSeen_ = false;
    piePendingClientCount_ = 0;
    piePendingClientExe_.clear();
    piePendingJoinAddr_.clear();
    piePendingPlayMap_.clear();
    piePendingClientWaitSeconds_ = 0.0f;
    piePendingHostSettleSeconds_ = 0.0f;

    const std::string playMap = CurrentPiePlayMapKey(ctx_);
    EditorLogInfo("PIE Play settings: net=" +
                  std::string(ctx_.pieNetMode == EEditorPlayNetMode::ListenServer ? "ListenServer"
                              : ctx_.pieNetMode == EEditorPlayNetMode::Client     ? "Client"
                                                                                  : "Standalone") +
                  " players=" + std::to_string(ctx_.pieNumberOfPlayers) + " map=" + playMap);

    if (ctx_.pieNetMode == EEditorPlayNetMode::Standalone) {
        if (ctx_.pieNumberOfPlayers > 1) {
            EditorLogWarn("Standalone multi-instance: use Listen Server, or run Shipping copies");
            EditorToast("Standalone multi-instance: use listen server, or run shipping copies "
                        "manually",
                        EEditorToastKind::Warning, 5.0f);
        }
        return;
    }

#ifndef _WIN32
    if (ctx_.pieNumberOfPlayers > 1) {
        EditorToast("Multiplayer play-in-editor spawn is Windows-only for now", EEditorToastKind::Warning,
                    4.0f);
    }
    return;
#else
    const int want = ctx_.pieNumberOfPlayers;
    if (want <= 1 && ctx_.pieNetMode != EEditorPlayNetMode::ListenServer) {
        return;
    }
    // Listen Server with 1 player: editor hosts only (no Shipping).
    if (ctx_.pieNetMode == EEditorPlayNetMode::ListenServer && want <= 1) {
        EditorLogInfo("PIE Listen Server x1: editor NetDriver only (no Shipping processes)");
        return;
    }

    const fs::path exe = FindPackShippingExecutable(ctx_.projectPath, ctx_.projectName);
    if (exe.empty()) {
        EditorLogError("Multiplayer PIE: Shipping exe not found under " + ctx_.projectPath +
                       "/Shipping (Build Game)");
        EditorToast("Multiplayer play-in-editor needs a built shipping game (Build Game)",
                    EEditorToastKind::Warning, 6.0f);
        return;
    }
    EditorLogInfo("PIE Shipping exe: " + exe.lexically_normal().string());

    const std::string joinAddr =
        ctx_.pieClientAddress.empty() ? "127.0.0.1" : ctx_.pieClientAddress;

    if (ctx_.pieNetMode == EEditorPlayNetMode::ListenServer) {
        // Unreal PIE: host opens the current map GameMode; clients join that map.
        const std::string hostArgs = "--listen --map " + playMap;
        std::uint32_t hostPid = 0;
        if (!SpawnDetachedProcess(exe, hostArgs, hostPid)) {
            EditorLogError("Failed to CreateProcess Shipping " + hostArgs);
            EditorToast("Failed to launch shipping listen server", EEditorToastKind::Error, 4.5f);
            return;
        }
        pieSpawnedPids_.push_back(hostPid);
        EditorLogInfo("Launched Shipping Listen Server pid=" + std::to_string(hostPid) + " " +
                      hostArgs + " — waiting for UDP :" + std::to_string(leon::net::kDefaultPort) +
                      " before clients");
        EditorToast("Hosting '" + playMap + "' — clients join when ready…", EEditorToastKind::Info,
                    4.0f);

        piePendingClientSpawn_ = true;
        piePendingHostPortSeen_ = false;
        piePendingClientCount_ = want - 1;
        piePendingClientExe_ = exe.lexically_normal().string();
        piePendingJoinAddr_ = joinAddr;
        piePendingPlayMap_ = playMap;
        piePendingClientWaitSeconds_ = 0.0f;
        piePendingHostSettleSeconds_ = 0.0f;
        return;
    }

    // Client mode: Number of Players = total Shipping clients joining the address/map.
    const std::string joinArgs = "--join " + joinAddr + " --map " + playMap;
    int launched = 0;
    for (int i = 0; i < want; ++i) {
        std::uint32_t pid = 0;
        if (SpawnDetachedProcess(exe, joinArgs, pid)) {
            pieSpawnedPids_.push_back(pid);
            ++launched;
            EditorLogInfo("Launched Shipping client pid=" + std::to_string(pid) + " " + joinArgs);
        }
    }
    EditorToast("Launched " + std::to_string(launched) + " shipping client(s) → '" + playMap + "'",
                launched > 0 ? EEditorToastKind::Success : EEditorToastKind::Error, 3.5f);
#endif
}

void EditorApp::TickPendingPieClientSpawns() {
#ifndef _WIN32
    return;
#else
    if (!piePendingClientSpawn_ || !ctx_.piePlaying) {
        return;
    }
    piePendingClientWaitSeconds_ += std::max(ctx_.deltaTime, 1.0f / 120.0f);
    constexpr float kHostWaitTimeout = 25.0f;
    // Host needs time to ServerTravel into the match map after binding.
    constexpr float kSettleAfterBind = 1.75f;
    const auto port = static_cast<std::uint16_t>(leon::net::kDefaultPort);

    if (!piePendingHostPortSeen_) {
        if (!IsUdpPortInUse(port)) {
            if (piePendingClientWaitSeconds_ >= kHostWaitTimeout) {
                EditorLogError("Shipping host did not bind :" + std::to_string(port) + " within " +
                               std::to_string(static_cast<int>(kHostWaitTimeout)) + "s");
                EditorToast("Shipping host did not bind port " + std::to_string(port) + " in time",
                            EEditorToastKind::Error, 6.0f);
                piePendingClientSpawn_ = false;
            }
            return;
        }
        piePendingHostPortSeen_ = true;
        piePendingHostSettleSeconds_ = 0.0f;
        EditorLogInfo("Shipping host bound UDP :" + std::to_string(port) +
                      " — spawning clients shortly");
        return;
    }

    piePendingHostSettleSeconds_ += std::max(ctx_.deltaTime, 1.0f / 120.0f);
    if (piePendingHostSettleSeconds_ < kSettleAfterBind) {
        return;
    }

    std::string joinArgs = "--join " + piePendingJoinAddr_;
    if (!piePendingPlayMap_.empty()) {
        joinArgs += " --map " + piePendingPlayMap_;
    }
    int launched = 0;
    for (int i = 0; i < piePendingClientCount_; ++i) {
        std::uint32_t pid = 0;
        if (SpawnDetachedProcess(fs::path(piePendingClientExe_), joinArgs, pid)) {
            pieSpawnedPids_.push_back(pid);
            ++launched;
            EditorLogInfo("Launched Shipping client pid=" + std::to_string(pid) + " " + joinArgs);
        } else {
            EditorLogError("Failed to CreateProcess Shipping client " + joinArgs);
        }
    }
    EditorLogInfo("PIE multiplayer: Listen Server + " + std::to_string(launched) +
                  " client(s) → '" + piePendingPlayMap_ + "'");
    EditorToast("Launched host + " + std::to_string(launched) + " client(s) → '" +
                    piePendingPlayMap_ + "'",
                launched > 0 ? EEditorToastKind::Success : EEditorToastKind::Error, 4.5f);
    piePendingClientSpawn_ = false;
    piePendingClientCount_ = 0;
#endif
}

void EditorApp::TerminatePieSpawnedProcesses() {
#ifdef _WIN32
    for (std::uint32_t pid : pieSpawnedPids_) {
        if (pid == 0) {
            continue;
        }
        HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
        if (proc != nullptr) {
            (void)TerminateProcess(proc, 0);
            CloseHandle(proc);
        }
    }
#endif
    pieSpawnedPids_.clear();
    piePendingClientSpawn_ = false;
    piePendingHostPortSeen_ = false;
    piePendingClientCount_ = 0;
    piePendingClientExe_.clear();
    piePendingJoinAddr_.clear();
    piePendingPlayMap_.clear();
    piePendingClientWaitSeconds_ = 0.0f;
    piePendingHostSettleSeconds_ = 0.0f;
}

void EditorApp::StopPie(Engine& engine) {
    TerminatePieSpawnedProcesses();
    engine.GetGameInstance().CloseNetSession();
    const bool shippingOnly = pieShippingSessionOnly_;
    pieShippingSessionOnly_ = false;

    const bool hadLocalMode = pieMode_ != nullptr;
    const bool hadSession = pieUsesHostSession_;
    if (!ctx_.piePlaying || (!hadLocalMode && !hadSession)) {
        ctx_.piePlaying = false;
        ctx_.piePaused = false;
        ctx_.requestPieStop = false;
        ctx_.requestPiePauseToggle = false;
        ctx_.pieNewWindow = false;
        ctx_.editorViewCamera = nullptr;
        ctx_.piePaintPlayOverlay = {};
        ctx_.world = &world_;
        pieUsesHostSession_ = false;
        pieRestoreLevelPath_.clear();
        engine.SetPlayInputWindow(nullptr);
        engine.SetPlayMouseLookActive(false);
        engine.SetCursorCaptured(false);
        DestroyPiePresentResources();
        pieTarget_.Destroy();
        pieWindow_.Destroy();
        if (shippingOnly) {
            EditorLogInfo("PIE multiplayer session stopped (Shipping processes terminated)");
        }
        return;
    }

    if (hadSession) {
        pieSession_.Stop();
        pieUsesHostSession_ = false;
    }
    if (hadLocalMode) {
        pieMode_->OnExit(engine);
        pieMode_.reset();
        engine.GetGameInstance().SetLevelTravelFn({});
    }

    ctx_.piePaintPlayOverlay = {};
    ctx_.world = &world_;
    world_.Clear();
    ctx_.piePlaying = false;
    ctx_.piePaused = false;
    ctx_.requestPieStop = false;
    ctx_.requestPiePauseToggle = false;
    ctx_.pieNewWindow = false;
    ctx_.editorViewCamera = nullptr;
    engine.SetPlayInputWindow(nullptr);
    engine.SetPlayMouseLookActive(false);
    DestroyPiePresentResources();
    pieTarget_.Destroy();
    pieWindow_.Destroy();
    engine.GetWindow().SetCursorCaptured(false);
    engine.GetWindow().MakeContextCurrent();

    // Restore edit-time content root + level (travel may have replaced engine.GetLevel()).
    if (!ctx_.projectPath.empty()) {
        SetActiveContentRoot(ctx_.projectPath);
    } else {
        SetActiveContentRoot({});
    }
    const std::string restorePath =
        !pieRestoreLevelPath_.empty() ? pieRestoreLevelPath_ : ctx_.levelPath;
    pieRestoreLevelPath_.clear();
    bool restored = false;
    if (!restorePath.empty() && LoadLevelFile(engine, restorePath)) {
        ctx_.levelPath = restorePath;
        ctx_.level = &engine.GetLevel();
        restored = true;
    } else if (hadSession || hadLocalMode) {
        // Session/local play cleared the edit world — do not leave a silent empty level.
        EditorToast("Stop: could not restore level — loading Blank template",
                    EEditorToastKind::Warning, 5.0f);
        std::string tmplErr;
        if (CreateLevelFromTemplate(ENewLevelTemplate::Blank, engine, tmplErr)) {
            ctx_.levelPath.clear();
            restored = true;
        } else {
            EditorToast("Stop: Blank fallback failed — " + tmplErr, EEditorToastKind::Error, 6.0f);
        }
    }

    if (restored) {
        ctx_.dirty = false;
        // Drop edit history from the pre-Play document; next Capture starts a clean baseline.
        // Do not Capture here — Undo would re-apply the same snapshot and mark dirty (*).
        history_.Clear();
    }

    RestoreEditorCamera(engine.GetCamera());
    // Prefer FreeLook for editor navigation after PIE.
    if (engine.GetCamera().Mode() != ECameraMode::FreeLook) {
        const glm::vec3 eye = engine.GetCamera().GetCameraLocation();
        engine.GetCamera().SetMode(ECameraMode::FreeLook);
        engine.GetCamera().SetEyeLocation(eye);
    }
}

void EditorApp::DestroyPiePresentResources() {
    if (piePresentFbo_ == 0 && piePresentAttachedTex_ == 0) {
        return;
    }
    if (pieWindow_.Handle() != nullptr) {
        pieWindow_.MakeContextCurrent();
        if (piePresentFbo_ != 0) {
            glDeleteFramebuffers(1, &piePresentFbo_);
            piePresentFbo_ = 0;
        }
        piePresentAttachedTex_ = 0;
        if (ctx_.window != nullptr) {
            ctx_.window->MakeContextCurrent();
        }
    } else {
        piePresentFbo_ = 0;
        piePresentAttachedTex_ = 0;
    }
}

void EditorApp::PresentPieColorTexture(unsigned int colorTexture, int srcW, int srcH, int dstW,
                                       int dstH, int destX, int destY, int destW, int destH) {
    if (colorTexture == 0 || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0 || destW <= 0 ||
        destH <= 0) {
        return;
    }

    pieWindow_.MakeContextCurrent();
    if (piePresentFbo_ == 0) {
        glGenFramebuffers(1, &piePresentFbo_);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, piePresentFbo_);
    if (piePresentAttachedTex_ != colorTexture) {
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               colorTexture, 0);
        piePresentAttachedTex_ = colorTexture;
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, dstW, dstH);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // FitPieViewport uses top-left origin; OpenGL blit uses bottom-left.
    const int glY = dstH - destY - destH;
    glBlitFramebuffer(0, 0, srcW, srcH, destX, glY, destX + destW, glY + destH, GL_COLOR_BUFFER_BIT,
                      GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    pieWindow_.SwapBuffers();
    if (ctx_.window != nullptr) {
        ctx_.window->MakeContextCurrent();
    }
}

void EditorApp::RenderPieWindow(Engine& engine) {
    if (!ctx_.piePlaying || !ctx_.pieNewWindow || pieWindow_.Handle() == nullptr) {
        return;
    }
    if (pieWindow_.ShouldClose()) {
        ctx_.requestPieStop = true;
        return;
    }

    int fbW = 0;
    int fbH = 0;
    pieWindow_.GetFramebufferSize(fbW, fbH);
    if (fbW <= 0 || fbH <= 0) {
        return;
    }

    int drawX = 0;
    int drawY = 0;
    int drawW = fbW;
    int drawH = fbH;
    FitPieViewport(fbW, fbH, ctx_.pieAspect, drawX, drawY, drawW, drawH);

    // Play window owns look capture; keep editor chrome unlocked.
    if (!ctx_.piePaused && pieWindow_.IsFocused() && !pieWindow_.IsCursorCaptured()) {
        pieWindow_.SetCursorCaptured(true);
    }
    engine.GetWindow().SetCursorCaptured(false);

    // Flow: New Window PIE present
    // 1. Render the scene on the editor GL context into a shared color texture (FBOs/VAOs
    //    are not shareable — drawing on the play context left a black window).
    // 2. On the play context, blit that texture into the default framebuffer and swap.
    engine.GetWindow().MakeContextCurrent();
    if (!pieTarget_.EnsureSize(drawW, drawH)) {
        return;
    }
    const float aspect = static_cast<float>(drawW) / static_cast<float>(std::max(drawH, 1));
    engine.GetCamera().SetPerspective(engine.GetCamera().FieldOfView(), aspect, 0.1f, 100.0f);
    engine.GetRenderer().SetDrawFramebuffer(pieTarget_.fbo());
    engine.GetRenderer().BeginFrame(drawW, drawH);
    // Skeletal draws were queued by gameplay Tick immediately before this call.
    engine.GetRenderer().DrawScene(engine.GetLevel(), engine.GetCamera());
    if (ctx_.piePaintPlayOverlay) {
        ctx_.piePaintPlayOverlay(drawW, drawH);
    }
    engine.GetRenderer().SetDrawFramebuffer(0);
    PresentPieColorTexture(pieTarget_.colorTexture(), pieTarget_.width(), pieTarget_.height(), fbW,
                           fbH, drawX, drawY, drawW, drawH);
}

} // namespace leon::editor
