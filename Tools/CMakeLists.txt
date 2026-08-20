if(NOT DEFINED LEON_ENGINE_ROOT)
    get_filename_component(LEON_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
endif()

add_subdirectory(Common)
add_subdirectory(AssetTool)
add_subdirectory(Lightmass)
add_subdirectory(ProjectTool)
