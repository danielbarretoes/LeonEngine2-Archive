# Optional in-process game module for Editor PIE (static link — shared engine CRT).
# Keeps UClassRegistry singleton consistent with LeonEditor.

set(LEON_TOURNAMENT_ROOT "${LEON_ENGINE_ROOT}/Projects/LeonTournament")
set(LEON_TOURNAMENT_PUBLIC "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Public")

set(LEON_TOURNAMENT_GAME_SOURCES
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Game/ALeonTournamentGameMode.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Game/FLeonTournamentArenaBuilder.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Game/FLeonTournamentDamageRules.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Game/ALeonTournamentGameState.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Characters/ALeonTournamentCharacter.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Game/ALeonTournamentPlayerController.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Game/ALeonTournamentPlayerState.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Game/ALeonTournamentBotController.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Game/FLeonTournamentKillFeed.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Combat/ALeonTournamentWeapon.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Combat/FLeonTournamentWeaponVfx.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Combat/ALeonTournamentProjectile.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Combat/ALeonTournamentPickup.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Game/ALeonTournamentFlag.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Game/ALeonTournamentFlagBase.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Game/ALeonTournamentCaptureTheFlagGameMode.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/UI/ALeonTournamentHUD.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Characters/ALeonTournamentDummy.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Game/ALeonTournamentAnimLabGameMode.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Game/ALeonTournamentRenderLabGameMode.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Game/ALeonTournamentTransitionGameMode.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Characters/ULeonTournamentAnimInstance.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Combat/ULeonTournamentCombatComponent.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Game/ULeonTournamentGameInstance.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/Game/FLeonTournamentGameModule.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/UI/FLeonTournamentUILayout.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/UI/FLeonTournamentCrosshairTextures.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/UI/ULeonTournamentMainMenuWidget.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/UI/ULeonTournamentLobbyWidget.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/UI/ULeonTournamentHUDWidget.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/UI/ULeonTournamentScoreboardWidget.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/UI/ULeonTournamentPauseWidget.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/UI/ULeonTournamentMatchEndWidget.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/UI/ULeonTournamentLoadingOverlayWidget.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/UI/ULeonTournamentRenderLabWidget.cpp"
    "${LEON_TOURNAMENT_ROOT}/Source/LeonTournament/Private/UI/ALeonTournamentTransitionHUD.cpp"
)

add_library(LeonTournamentGame STATIC ${LEON_TOURNAMENT_GAME_SOURCES})
target_include_directories(LeonTournamentGame PUBLIC
    "${LEON_TOURNAMENT_PUBLIC}"
    "${LEON_TOURNAMENT_PUBLIC}/Types"
    "${LEON_TOURNAMENT_PUBLIC}/UI"
    "${LEON_TOURNAMENT_PUBLIC}/Game"
    "${LEON_TOURNAMENT_PUBLIC}/Characters"
    "${LEON_TOURNAMENT_PUBLIC}/Combat"
)
target_include_directories(LeonTournamentGame PRIVATE
    "${LEON_ENGINE_ROOT}/Plugins/Physics/Jolt/include"
    "${LEON_ENGINE_ROOT}/Plugins/Networking/ENet/include"
)
target_link_libraries(LeonTournamentGame PUBLIC
    LeonEngineCore
    Leon::Pipeline
    Leon::OpenGL
    Leon::Jolt
    Leon::ENet
)
target_compile_definitions(LeonTournamentGame PUBLIC LEON_EDITOR_HAS_TOURNAMENT_MODULE=1)
leon_apply_compile_options(LeonTournamentGame)
