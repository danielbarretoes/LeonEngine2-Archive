cmake_minimum_required(VERSION 3.20)

project(LeonEngine2
    VERSION 0.6.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Repo root for Engine/Plugins/ThirdParty paths (also when nested from Editor).
set(LEON_ENGINE_ROOT "${CMAKE_SOURCE_DIR}" CACHE PATH "LeonEngine2 repository root")

# Product split: Engine-only libraries/tools, or one Project game executable.
# Editor is a separate CMake entry point (cmake -S Editor -B out/Editor).
set(LEON_PRODUCT "Project" CACHE STRING "Build product: Engine | Project")
set_property(CACHE LEON_PRODUCT PROPERTY STRINGS Engine Project)

if(NOT LEON_PRODUCT STREQUAL "Engine" AND NOT LEON_PRODUCT STREQUAL "Project")
    message(FATAL_ERROR "LEON_PRODUCT must be Engine or Project (got '${LEON_PRODUCT}'). Use cmake -S Editor for the editor.")
endif()

# Engine product ships tools + tests. Project product is one game (+ asset tool for packaging).
if(LEON_PRODUCT STREQUAL "Engine")
    set(LEON_STACK_WITH_ENET ON)
    set(LEON_STACK_WITH_TOOLS ON)
    set(LEON_STACK_WITH_TESTS ON)
else()
    set(LEON_STACK_WITH_ENET ON)
    set(LEON_STACK_WITH_TOOLS ON)
    set(LEON_STACK_WITH_TESTS OFF)
endif()

include("${LEON_ENGINE_ROOT}/CMake/LeonEngineStack.cmake")

if(LEON_PRODUCT STREQUAL "Project")
    # One game per configure. Absolute paths supported (external .lproject trees).
    # Scripts use -B out/Projects/<Name>; game objects go under _project/ (not nested Projects/).
    set(LEON_PROJECT_DIR "Projects/Sandbox" CACHE PATH "Relative or absolute path to the game project directory")
    if(EXISTS "${LEON_ENGINE_ROOT}/${LEON_PROJECT_DIR}/CMakeLists.txt")
        add_subdirectory("${LEON_ENGINE_ROOT}/${LEON_PROJECT_DIR}" "${CMAKE_BINARY_DIR}/_project")
    elseif(EXISTS "${LEON_PROJECT_DIR}/CMakeLists.txt")
        add_subdirectory("${LEON_PROJECT_DIR}" "${CMAKE_BINARY_DIR}/_project")
    else()
        message(WARNING "LEON_PROJECT_DIR='${LEON_PROJECT_DIR}' not found — skipping game target")
    endif()
endif()
