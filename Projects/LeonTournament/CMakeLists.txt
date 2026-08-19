set(LEON_TOURNAMENT_PUBLIC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Source/LeonTournament/Public")

add_executable(LeonTournament
    Source/LeonTournament/Main.cpp
    Source/LeonTournament/Private/Game/ALeonTournamentGameMode.cpp
    Source/LeonTournament/Private/Game/FLeonTournamentArenaBuilder.cpp
    Source/LeonTournament/Private/Game/FLeonTournamentDamageRules.cpp
    Source/LeonTournament/Private/Game/ALeonTournamentGameState.cpp
    Source/LeonTournament/Private/Characters/ALeonTournamentCharacter.cpp
    Source/LeonTournament/Private/Game/ALeonTournamentPlayerController.cpp
    Source/LeonTournament/Private/Game/ALeonTournamentPlayerState.cpp
    Source/LeonTournament/Private/Game/ALeonTournamentBotController.cpp
    Source/LeonTournament/Private/Combat/ALeonTournamentWeapon.cpp
    Source/LeonTournament/Private/Combat/FLeonTournamentWeaponVfx.cpp
    Source/LeonTournament/Private/Combat/ALeonTournamentProjectile.cpp
    Source/LeonTournament/Private/Combat/ALeonTournamentPickup.cpp
    Source/LeonTournament/Private/UI/ALeonTournamentHUD.cpp
    Source/LeonTournament/Private/Characters/ALeonTournamentDummy.cpp
    Source/LeonTournament/Private/Game/ALeonTournamentAnimLabGameMode.cpp
    Source/LeonTournament/Private/Game/ALeonTournamentRenderLabGameMode.cpp
    Source/LeonTournament/Private/Characters/ULeonTournamentAnimInstance.cpp
    Source/LeonTournament/Private/Combat/ULeonTournamentCombatComponent.cpp
    Source/LeonTournament/Private/Game/ULeonTournamentGameInstance.cpp
    Source/LeonTournament/Private/UI/FLeonTournamentUILayout.cpp
    Source/LeonTournament/Private/UI/ULeonTournamentMainMenuWidget.cpp
    Source/LeonTournament/Private/UI/ULeonTournamentLobbyWidget.cpp
    Source/LeonTournament/Private/UI/ULeonTournamentHUDWidget.cpp
    Source/LeonTournament/Private/UI/ULeonTournamentScoreboardWidget.cpp
    Source/LeonTournament/Private/UI/ULeonTournamentPauseWidget.cpp
    Source/LeonTournament/Private/UI/ULeonTournamentMatchEndWidget.cpp
    Source/LeonTournament/Private/UI/ULeonTournamentRenderLabWidget.cpp
)

target_include_directories(LeonTournament PRIVATE
    "${LEON_TOURNAMENT_PUBLIC_ROOT}"
    "${LEON_TOURNAMENT_PUBLIC_ROOT}/Types"
    "${LEON_TOURNAMENT_PUBLIC_ROOT}/UI"
    "${LEON_TOURNAMENT_PUBLIC_ROOT}/Game"
    "${LEON_TOURNAMENT_PUBLIC_ROOT}/Characters"
    "${LEON_TOURNAMENT_PUBLIC_ROOT}/Combat"
    "${CMAKE_SOURCE_DIR}/Plugins/Physics/Jolt/include"
    "${CMAKE_SOURCE_DIR}/Plugins/Networking/ENet/include"
)

target_link_libraries(LeonTournament PRIVATE
    LeonEngineCore
    Leon::Pipeline
    Leon::OpenGL
    Leon::Jolt
    Leon::ENet
)
