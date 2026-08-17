cmake_minimum_required(VERSION 3.20)

project(LeonEngine2 
    VERSION 0.6.0 
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include FetchContent module
include(FetchContent)

# GLFW Configuration and Download
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

# GLM (OpenGL Mathematics) Configuration and Download
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

# EnTT (Entity Component System) Configuration and Download
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.14.0
)
FetchContent_MakeAvailable(entt)


# Subdirectories (Strict Dependency Order: ThirdParty -> Engine -> Plugins -> Tools -> Project -> Tests)
add_subdirectory(ThirdParty/glad)
add_subdirectory(ThirdParty/stb)
add_subdirectory(ThirdParty/ufbx)
add_subdirectory(ThirdParty/doctest)
add_subdirectory(Engine)
add_subdirectory(Plugins/RHI/OpenGL)
add_subdirectory(Plugins/Physics/Jolt)
add_subdirectory(Plugins/Networking/ENet)
add_subdirectory(Tools/LeonAssetTool)

# Game project (monorepo default). Override: -DLEON_PROJECT_DIR=Projects/MyGame
# Absolute paths outside the Engine tree are supported (Unreal-like external .uproject).
# Engine CMake never names a product; only this root entry point selects the client.
set(LEON_PROJECT_DIR "Projects/Sandbox" CACHE PATH "Relative or absolute path to the game project directory")
if(EXISTS "${CMAKE_SOURCE_DIR}/${LEON_PROJECT_DIR}/CMakeLists.txt")
    add_subdirectory("${LEON_PROJECT_DIR}")
elseif(EXISTS "${LEON_PROJECT_DIR}/CMakeLists.txt")
    get_filename_component(_LEON_PROJECT_FOLDER_NAME "${LEON_PROJECT_DIR}" NAME)
    add_subdirectory("${LEON_PROJECT_DIR}" "${CMAKE_BINARY_DIR}/Projects/${_LEON_PROJECT_FOLDER_NAME}")
else()
    message(WARNING "LEON_PROJECT_DIR='${LEON_PROJECT_DIR}' not found — skipping game target")
endif()

# Extra monorepo game (engine does not depend on it). Skip if it is already the selected product.
if(EXISTS "${CMAKE_SOURCE_DIR}/Projects/LeonTournament/CMakeLists.txt")
    get_filename_component(_LEON_SELECTED_PROJECT "${LEON_PROJECT_DIR}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    get_filename_component(_LEON_TOURNAMENT_DIR "${CMAKE_SOURCE_DIR}/Projects/LeonTournament" ABSOLUTE)
    if(NOT _LEON_SELECTED_PROJECT STREQUAL _LEON_TOURNAMENT_DIR)
        add_subdirectory(Projects/LeonTournament)
    endif()
endif()

add_subdirectory(Tests)
