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
    Source/LeonTournament/Private/Game/FLeonTournamentKillFeed.cpp
    Source/LeonTournament/Private/Combat/ALeonTournamentWeapon.cpp
    Source/LeonTournament/Private/Combat/FLeonTournamentWeaponVfx.cpp
    Source/LeonTournament/Private/Combat/ALeonTournamentProjectile.cpp
    Source/LeonTournament/Private/Combat/ALeonTournamentPickup.cpp
    Source/LeonTournament/Private/Game/ALeonTournamentFlag.cpp
    Source/LeonTournament/Private/Game/ALeonTournamentFlagBase.cpp
    Source/LeonTournament/Private/Game/ALeonTournamentCaptureTheFlagGameMode.cpp
    Source/LeonTournament/Private/UI/ALeonTournamentHUD.cpp
    Source/LeonTournament/Private/Characters/ALeonTournamentDummy.cpp
    Source/LeonTournament/Private/Game/ALeonTournamentAnimLabGameMode.cpp
    Source/LeonTournament/Private/Game/ALeonTournamentRenderLabGameMode.cpp
    Source/LeonTournament/Private/Game/ALeonTournamentTransitionGameMode.cpp
    Source/LeonTournament/Private/Characters/ULeonTournamentAnimInstance.cpp
    Source/LeonTournament/Private/Combat/ULeonTournamentCombatComponent.cpp
    Source/LeonTournament/Private/Game/ULeonTournamentGameInstance.cpp
    Source/LeonTournament/Private/UI/FLeonTournamentUILayout.cpp
    Source/LeonTournament/Private/UI/FLeonTournamentCrosshairTextures.cpp
    Source/LeonTournament/Private/UI/ULeonTournamentMainMenuWidget.cpp
    Source/LeonTournament/Private/UI/ULeonTournamentLobbyWidget.cpp
    Source/LeonTournament/Private/UI/ULeonTournamentHUDWidget.cpp
    Source/LeonTournament/Private/UI/ULeonTournamentScoreboardWidget.cpp
    Source/LeonTournament/Private/UI/ULeonTournamentPauseWidget.cpp
    Source/LeonTournament/Private/UI/ULeonTournamentMatchEndWidget.cpp
    Source/LeonTournament/Private/UI/ULeonTournamentLoadingOverlayWidget.cpp
    Source/LeonTournament/Private/UI/ULeonTournamentRenderLabWidget.cpp
    Source/LeonTournament/Private/UI/ALeonTournamentTransitionHUD.cpp
)

target_include_directories(LeonTournament PRIVATE
    "${LEON_TOURNAMENT_PUBLIC_ROOT}"
    "${LEON_TOURNAMENT_PUBLIC_ROOT}/Types"
    "${LEON_TOURNAMENT_PUBLIC_ROOT}/UI"
    "${LEON_TOURNAMENT_PUBLIC_ROOT}/Game"
    "${LEON_TOURNAMENT_PUBLIC_ROOT}/Characters"
    "${LEON_TOURNAMENT_PUBLIC_ROOT}/Combat"
    "${LEON_ENGINE_ROOT}/Plugins/Physics/Jolt/include"
    "${LEON_ENGINE_ROOT}/Plugins/Networking/ENet/include"
)

target_link_libraries(LeonTournament PRIVATE
    LeonEngineCore
    Leon::Pipeline
    Leon::OpenGL
    Leon::Jolt
    Leon::ENet
)

if(NOT TARGET doctest)
    add_subdirectory("${LEON_ENGINE_ROOT}/ThirdParty/doctest" "${CMAKE_BINARY_DIR}/_leon_tp_doctest")
endif()

file(GLOB LEON_TOURNAMENT_TEST_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.cpp")
if(LEON_TOURNAMENT_TEST_SRCS)
        add_executable(LeonTournamentTests
            ${LEON_TOURNAMENT_TEST_SRCS}
            Source/LeonTournament/Private/Game/ALeonTournamentGameMode.cpp
            Source/LeonTournament/Private/Game/FLeonTournamentArenaBuilder.cpp
            Source/LeonTournament/Private/Game/FLeonTournamentDamageRules.cpp
            Source/LeonTournament/Private/Game/FLeonTournamentKillFeed.cpp
            Source/LeonTournament/Private/Game/ALeonTournamentGameState.cpp
            Source/LeonTournament/Private/Characters/ALeonTournamentCharacter.cpp
            Source/LeonTournament/Private/Game/ALeonTournamentPlayerController.cpp
            Source/LeonTournament/Private/Game/ALeonTournamentPlayerState.cpp
            Source/LeonTournament/Private/Game/ALeonTournamentBotController.cpp
            Source/LeonTournament/Private/Combat/ALeonTournamentWeapon.cpp
            Source/LeonTournament/Private/Combat/FLeonTournamentWeaponVfx.cpp
            Source/LeonTournament/Private/Combat/ALeonTournamentProjectile.cpp
            Source/LeonTournament/Private/Combat/ALeonTournamentPickup.cpp
            Source/LeonTournament/Private/Game/ALeonTournamentFlag.cpp
            Source/LeonTournament/Private/Game/ALeonTournamentFlagBase.cpp
            Source/LeonTournament/Private/Game/ALeonTournamentCaptureTheFlagGameMode.cpp
            Source/LeonTournament/Private/UI/ALeonTournamentHUD.cpp
            Source/LeonTournament/Private/Characters/ALeonTournamentDummy.cpp
            Source/LeonTournament/Private/Game/ALeonTournamentAnimLabGameMode.cpp
            Source/LeonTournament/Private/Game/ALeonTournamentRenderLabGameMode.cpp
            Source/LeonTournament/Private/Game/ALeonTournamentTransitionGameMode.cpp
            Source/LeonTournament/Private/Characters/ULeonTournamentAnimInstance.cpp
            Source/LeonTournament/Private/Combat/ULeonTournamentCombatComponent.cpp
            Source/LeonTournament/Private/Game/ULeonTournamentGameInstance.cpp
            Source/LeonTournament/Private/UI/FLeonTournamentUILayout.cpp
            Source/LeonTournament/Private/UI/FLeonTournamentCrosshairTextures.cpp
            Source/LeonTournament/Private/UI/ULeonTournamentMainMenuWidget.cpp
            Source/LeonTournament/Private/UI/ULeonTournamentLobbyWidget.cpp
            Source/LeonTournament/Private/UI/ULeonTournamentHUDWidget.cpp
            Source/LeonTournament/Private/UI/ULeonTournamentScoreboardWidget.cpp
            Source/LeonTournament/Private/UI/ULeonTournamentPauseWidget.cpp
            Source/LeonTournament/Private/UI/ULeonTournamentMatchEndWidget.cpp
            Source/LeonTournament/Private/UI/ULeonTournamentLoadingOverlayWidget.cpp
            Source/LeonTournament/Private/UI/ULeonTournamentRenderLabWidget.cpp
            Source/LeonTournament/Private/UI/ALeonTournamentTransitionHUD.cpp
        )
        target_include_directories(LeonTournamentTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Tests"
            "${LEON_TOURNAMENT_PUBLIC_ROOT}"
            "${LEON_TOURNAMENT_PUBLIC_ROOT}/Types"
            "${LEON_TOURNAMENT_PUBLIC_ROOT}/UI"
            "${LEON_TOURNAMENT_PUBLIC_ROOT}/Game"
            "${LEON_TOURNAMENT_PUBLIC_ROOT}/Characters"
            "${LEON_TOURNAMENT_PUBLIC_ROOT}/Combat"
            "${LEON_ENGINE_ROOT}/Plugins/RHI/OpenGL/include"
            "${LEON_ENGINE_ROOT}/Plugins/Physics/Jolt/include"
            "${LEON_ENGINE_ROOT}/Plugins/Networking/ENet/include"
            "${LEON_ENGINE_ROOT}/ThirdParty/glad/include"
            "${LEON_ENGINE_ROOT}/ThirdParty/stb"
            "${LEON_ENGINE_ROOT}/ThirdParty/doctest"
            "${LEON_ENGINE_ROOT}/ThirdParty"
        )
        target_link_libraries(LeonTournamentTests PRIVATE
            LeonEngineCore
            Leon::Pipeline
            Leon::OpenGL
            Leon::Jolt
            Leon::ENet
            doctest
            glad
            glfw
            glm
            stb_image
        )
    endif()
