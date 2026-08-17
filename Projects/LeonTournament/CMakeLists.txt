add_executable(LeonTournament
    Source/LeonTournament/Main.cpp
    Source/LeonTournament/Private/AShooterGameMode.cpp
    Source/LeonTournament/Private/AShooterGameState.cpp
    Source/LeonTournament/Private/AShooterCharacter.cpp
    Source/LeonTournament/Private/AShooterPlayerController.cpp
    Source/LeonTournament/Private/AShooterPlayerState.cpp
    Source/LeonTournament/Private/AShooterBotController.cpp
    Source/LeonTournament/Private/AShooterWeapon.cpp
    Source/LeonTournament/Private/AShooterHUD.cpp
    Source/LeonTournament/Private/UShooterAnimInstance.cpp
    Source/LeonTournament/Private/UShooterCombatComponent.cpp
    Source/LeonTournament/Private/UShooterGameInstance.cpp
    Source/LeonTournament/Private/UShooterWidgets.cpp
)

target_include_directories(LeonTournament PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/LeonTournament/Public"
    "${CMAKE_SOURCE_DIR}/Plugins/Physics/Jolt/include"
    "${CMAKE_SOURCE_DIR}/Plugins/Networking/ENet/include"
)

target_link_libraries(LeonTournament PRIVATE
    LeonEngineCore
    Leon::OpenGL
    Leon::Jolt
    Leon::ENet
)
