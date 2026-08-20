# Dear ImGui (docking) for the LeonEditor product.
# GLFW / glad come from the Engine stack — do not FetchContent them here.
# ImGuizmo is deferred until a compatible tag for ImGui 1.91+ is wired in.

include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/LeonCompileOptions.cmake")

if(NOT TARGET glfw)
    message(FATAL_ERROR "EditorDependencies: glfw must exist before including this file")
endif()
if(NOT TARGET glad)
    message(FATAL_ERROR "EditorDependencies: glad must exist before including this file")
endif()

if(NOT TARGET imgui)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.8-docking
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC glfw glad)
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
    if(MSVC)
        target_compile_options(imgui PRIVATE /W0)
    else()
        target_compile_options(imgui PRIVATE -w)
    endif()
    leon_enable_msvc_mp(imgui)
endif()
