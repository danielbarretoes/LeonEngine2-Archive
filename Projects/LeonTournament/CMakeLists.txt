add_executable(LeonTournament
    Source/LeonTournament/Main.cpp
    Source/LeonTournament/Private/ALeonTournamentGameMode.cpp
    Source/LeonTournament/Private/ALeonTournamentGameState.cpp
    Source/LeonTournament/Private/ALeonTournamentCharacter.cpp
    Source/LeonTournament/Private/ALeonTournamentPlayerController.cpp
    Source/LeonTournament/Private/ALeonTournamentPlayerState.cpp
    Source/LeonTournament/Private/ALeonTournamentBotController.cpp
    Source/LeonTournament/Private/ALeonTournamentWeapon.cpp
    Source/LeonTournament/Private/ALeonTournamentProjectile.cpp
    Source/LeonTournament/Private/ALeonTournamentPickup.cpp
    Source/LeonTournament/Private/ALeonTournamentHUD.cpp
    Source/LeonTournament/Private/ALeonTournamentDummy.cpp
    Source/LeonTournament/Private/ALeonTournamentAnimLabGameMode.cpp
    Source/LeonTournament/Private/ULeonTournamentAnimInstance.cpp
    Source/LeonTournament/Private/ULeonTournamentCombatComponent.cpp
    Source/LeonTournament/Private/ULeonTournamentGameInstance.cpp
    Source/LeonTournament/Private/ULeonTournamentWidgets.cpp
)

target_include_directories(LeonTournament PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/LeonTournament/Public"
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
