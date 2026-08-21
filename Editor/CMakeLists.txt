# Leon Editor — separate product entry point (Docs/NAMING.md skeleton).
#   cmake -S Editor -B out/Editor -G Ninja -DCMAKE_BUILD_TYPE=Debug
#   cmake --build out/Editor --target LeonEditor

cmake_minimum_required(VERSION 3.20)
project(LeonEditor VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── MSVC CRT linkage ─────────────────────────────────────────────────────────
# Must be set BEFORE any include() or add_subdirectory() so that ALL targets
# (Engine, ThirdParty, ImGui, Jolt, GLFW …) share the same CRT.
# /MDd = MultiThreadedDebugDLL (Debug)  → links msvcrtd.lib
# /MD  = MultiThreadedDLL      (Release) → links msvcrt.lib
# Without this, Ninja+MSVC leaves the CRT undefined → LNK2001 on operator delete.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        CACHE STRING "MSVC CRT: DLL runtime (/MDd or /MD)" FORCE
    )
endif()
# ─────────────────────────────────────────────────────────────────────────────

if(CMAKE_CONFIGURATION_TYPES)
    foreach(_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${_cfg}" _cfg_up)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg_up} "${CMAKE_BINARY_DIR}/${_cfg}")
    endforeach()
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

get_filename_component(LEON_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(LEON_ENGINE_ROOT "${LEON_REPO_ROOT}" CACHE PATH "LeonEngine2 repository root")

set(LEON_STACK_WITH_ENET OFF)
set(LEON_STACK_WITH_TOOLS OFF)
set(LEON_STACK_WITH_TESTS OFF)
include("${LEON_ENGINE_ROOT}/CMake/LeonEngineStack.cmake")
include("${LEON_ENGINE_ROOT}/CMake/EditorDependencies.cmake")
include("${LEON_ENGINE_ROOT}/CMake/LeonCompileOptions.cmake")

add_library(LeonEditorCore STATIC
    Source/Private/FEditorApp.cpp
    Source/Private/FEditorLayoutStore.cpp
    Source/Private/Context/FEditorContext.cpp
    Source/Private/Context/FEditorSelection.cpp
    Source/Private/Context/FEditorHistory.cpp
    Source/Private/Commands/FDeleteActorsCommand.cpp
    Source/Private/Commands/FTransformActorsCommand.cpp
    Source/Private/Commands/FSelectActorsCommand.cpp
    Source/Private/Commands/FDuplicateActorsCommand.cpp
    Source/Private/Panels/FViewportPanel.cpp
    Source/Private/Panels/FProjectHubPanel.cpp
    Source/Private/Panels/FOutlinerPanel.cpp
    Source/Private/Panels/FDetailsPanel.cpp
    Source/Private/Panels/FContentBrowserPanel.cpp
    Source/Private/Panels/FToolbarPanel.cpp
    Source/Private/Panels/FOutputLogPanel.cpp
    Source/Private/Panels/FPlaceActorsPanel.cpp
    Source/Private/Panels/FWorldSettingsPanel.cpp
    Source/Private/Panels/FProjectSettingsPanel.cpp
    Source/Private/Gizmos/FTransformGizmo.cpp
    Source/Private/UI/FEditorTheme.cpp
    Source/Private/UI/FLucideIcons.cpp
    Source/Private/UI/FEditorWidgets.cpp
    Source/Private/Utils/FEditorFileDialog.cpp
    Source/Private/Window/FEditorWindow.cpp
)
add_library(Leon::Editor ALIAS LeonEditorCore)

target_include_directories(LeonEditorCore
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/Public"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/Private"
        "${LEON_ENGINE_ROOT}/Plugins/RHI/OpenGL/Include"
        "${LEON_ENGINE_ROOT}/Plugins/Physics/Jolt/Include"
)

target_link_libraries(LeonEditorCore PUBLIC
    LeonEngineCore
    Leon::Pipeline
    Leon::OpenGL
    Leon::Jolt
    imgui
)

if(WIN32)
    target_link_libraries(LeonEditorCore PUBLIC comdlg32 ole32)
endif()

add_executable(LeonEditor
    Source/Private/main.cpp
)
target_link_libraries(LeonEditor PRIVATE LeonEditorCore)
# GNU ld does not rescan static archives; Core references Pipeline (FLightmass).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(LeonEditor PRIVATE
        "-Wl,--start-group"
        LeonEngineCore
        LeonEnginePipeline
        "-Wl,--end-group"
    )
endif()
set_target_properties(LeonEditor PROPERTIES OUTPUT_NAME LeonEditor)

# Windows: embed .ico for Explorer / taskbar before GLFW sets the live icon.
if(WIN32)
    set(LEON_EDITOR_ICO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icons/LeonEditor.ico")
    if(EXISTS "${LEON_EDITOR_ICO_SRC}")
        file(TO_CMAKE_PATH "${LEON_EDITOR_ICO_SRC}" LEON_EDITOR_ICO)
        set(LEON_EDITOR_RC "${CMAKE_CURRENT_BINARY_DIR}/LeonEditor.rc")
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icons/LeonEditor.rc.in"
            "${LEON_EDITOR_RC}"
            @ONLY
        )
        target_sources(LeonEditor PRIVATE "${LEON_EDITOR_RC}")
        set_source_files_properties("${LEON_EDITOR_RC}" PROPERTIES OBJECT_DEPENDS "${LEON_EDITOR_ICO_SRC}")
    endif()
endif()

# Stage Engine/Resources beside the exe so relative shader/font paths resolve.
set(_leon_sync "${LEON_ENGINE_ROOT}/CMake/SyncDirectory.cmake")
set(_editor_exe_dir "$<TARGET_FILE_DIR:LeonEditor>")
set(LEON_ENGINE_RESOURCES "${LEON_ENGINE_ROOT}/Engine/Resources")
add_custom_command(TARGET LeonEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${LEON_ENGINE_RESOURCES}
        -DDST=${_editor_exe_dir}/Engine/Resources
        -DREMOVE_ORPHANS=ON
        -P ${_leon_sync}
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/Resources
        -DDST=${_editor_exe_dir}/Resources
        -DREMOVE_ORPHANS=OFF
        -P ${_leon_sync}
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/Resources
        -DDST=${_editor_exe_dir}/Editor/Resources
        -DREMOVE_ORPHANS=OFF
        -P ${_leon_sync}
)

leon_apply_compile_options(LeonEditorCore LeonEditor)
