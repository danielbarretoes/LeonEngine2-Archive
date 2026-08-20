add_executable(Sandbox
    Source/Sandbox/Main.cpp
    Source/Sandbox/Private/ASandboxGameMode.cpp
    Source/Sandbox/Private/ASandboxDemoPickup.cpp
    Source/Sandbox/Private/ASandboxHUD.cpp
    Source/Sandbox/Private/USandboxRenderLabWidget.cpp
)

target_include_directories(Sandbox PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Sandbox/Public"
    "${LEON_ENGINE_ROOT}/Plugins/Physics/Jolt/include"
)

target_link_libraries(Sandbox PRIVATE
    LeonEngineCore
    Leon::Pipeline
    Leon::OpenGL
    Leon::Jolt
)
