add_executable(Sandbox
    Source/Sandbox/Main.cpp
    Source/Sandbox/Private/ASandboxGameMode.cpp
    Source/Sandbox/Private/ASandboxHUD.cpp
    Source/Sandbox/Private/USandboxMainMenuWidget.cpp
)

target_include_directories(Sandbox PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Sandbox/Public"
    "${CMAKE_SOURCE_DIR}/Plugins/Physics/Jolt/include"
)

target_link_libraries(Sandbox PRIVATE
    LeonEngineCore
    Leon::OpenGL
    Leon::Jolt
)
