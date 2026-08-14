cmake_minimum_required(VERSION 3.20)

project(LeonEngine2 
    VERSION 0.2.0 
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


# Subdirectories (Strict Dependency Order: ThirdParty -> Engine -> Plugins -> Projects)
add_subdirectory(ThirdParty/glad)
add_subdirectory(ThirdParty/stb)
add_subdirectory(Engine)
add_subdirectory(Plugins/RHI/OpenGL)
add_subdirectory(Projects/Sandbox)
