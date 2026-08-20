# Shared Engine stack: FetchContent deps + ThirdParty + Engine + Plugins.
# Requires LEON_ENGINE_ROOT (absolute path to the LeonEngine2 repo root).
#
# Options:
#   LEON_STACK_WITH_ENET   (default ON)  — Networking/ENet plugin
#   LEON_STACK_WITH_TOOLS  (default OFF) — Tools/LeonAssetTool
#   LEON_STACK_WITH_TESTS  (default OFF) — Tests/

if(NOT DEFINED LEON_ENGINE_ROOT)
    message(FATAL_ERROR "LeonEngineStack: LEON_ENGINE_ROOT is not set")
endif()

include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/LeonCompileOptions.cmake")

if(NOT DEFINED LEON_STACK_WITH_ENET)
    set(LEON_STACK_WITH_ENET ON)
endif()
if(NOT DEFINED LEON_STACK_WITH_TOOLS)
    set(LEON_STACK_WITH_TOOLS OFF)
endif()
if(NOT DEFINED LEON_STACK_WITH_TESTS)
    set(LEON_STACK_WITH_TESTS OFF)
endif()

# --- GLFW ---
if(NOT TARGET glfw)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glfw)
endif()

# --- GLM ---
if(NOT TARGET glm::glm)
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glm)
endif()

# --- EnTT ---
if(NOT TARGET EnTT::EnTT)
    FetchContent_Declare(
        entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v3.14.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(entt)
endif()

# --- Vendored ThirdParty + Engine + Plugins ---
if(NOT TARGET glad)
    add_subdirectory("${LEON_ENGINE_ROOT}/ThirdParty/glad" "${CMAKE_BINARY_DIR}/_leon_tp_glad")
endif()
if(NOT TARGET stb_image)
    add_subdirectory("${LEON_ENGINE_ROOT}/ThirdParty/stb" "${CMAKE_BINARY_DIR}/_leon_tp_stb")
endif()
if(NOT TARGET ufbx)
    add_subdirectory("${LEON_ENGINE_ROOT}/ThirdParty/ufbx" "${CMAKE_BINARY_DIR}/_leon_tp_ufbx")
endif()
if(NOT TARGET doctest AND LEON_STACK_WITH_TESTS)
    add_subdirectory("${LEON_ENGINE_ROOT}/ThirdParty/doctest" "${CMAKE_BINARY_DIR}/_leon_tp_doctest")
endif()
if(NOT TARGET miniaudio)
    add_subdirectory("${LEON_ENGINE_ROOT}/ThirdParty/miniaudio" "${CMAKE_BINARY_DIR}/_leon_tp_miniaudio")
endif()

if(NOT TARGET LeonEngineCore)
    add_subdirectory("${LEON_ENGINE_ROOT}/Engine" "${CMAKE_BINARY_DIR}/_leon_engine")
endif()
if(NOT TARGET Leon::OpenGL)
    add_subdirectory("${LEON_ENGINE_ROOT}/Plugins/RHI/OpenGL" "${CMAKE_BINARY_DIR}/_leon_opengl")
endif()
if(NOT TARGET Leon::Jolt)
    add_subdirectory("${LEON_ENGINE_ROOT}/Plugins/Physics/Jolt" "${CMAKE_BINARY_DIR}/_leon_jolt")
endif()
if(LEON_STACK_WITH_ENET AND NOT TARGET Leon::ENet)
    add_subdirectory("${LEON_ENGINE_ROOT}/Plugins/Networking/ENet" "${CMAKE_BINARY_DIR}/_leon_enet")
endif()

if(LEON_STACK_WITH_TOOLS)
    if(NOT TARGET AssetTool)
        add_subdirectory("${LEON_ENGINE_ROOT}/Tools" "${CMAKE_BINARY_DIR}/_tools")
    endif()
endif()
if(LEON_STACK_WITH_TESTS AND NOT TARGET RendererTests)
    add_subdirectory("${LEON_ENGINE_ROOT}/Tests" "${CMAKE_BINARY_DIR}/_leon_tests")
endif()
