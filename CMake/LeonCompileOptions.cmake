# Shared MSVC / GCC compile options for Leon targets.
# Usage: include(...) then leon_apply_compile_options(tgt1 tgt2 ...)

# MSVC CRT policy: use the dynamic (DLL) CRT to avoid LNK2001 on operator delete / _Lockit.
# MultiThreadedDebugDLL = /MDd  (Debug)
# MultiThreadedDLL      = /MD   (Release, RelWithDebInfo, MinSizeRel)
# This MUST be set BEFORE any target is created; set it globally here so all
# add_subdirectory calls that follow inherit the same runtime linkage.
if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        CACHE STRING "MSVC CRT linkage" FORCE
    )
endif()

function(leon_apply_compile_options)
    foreach(tgt IN LISTS ARGN)
        if(TARGET ${tgt})
            if(MSVC)
                target_compile_options(${tgt} PRIVATE
                    /W4
                    /permissive-
                    /Zc:__cplusplus
                    /MP
                    $<$<CONFIG:Debug,RelWithDebInfo>:/FS>
                )
                # Explicitly set per-target CRT to match the global policy.
                # /MDd links msvcrtd.lib (Debug DLL CRT); /MD links msvcrt.lib.
                set_target_properties(${tgt} PROPERTIES
                    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
                )
                target_compile_definitions(${tgt} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
            else()
                target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
            endif()
        endif()
    endforeach()
endfunction()

# /MP on third-party STATIC libs (warnings stay local to each dep).
function(leon_enable_msvc_mp)
    foreach(tgt IN LISTS ARGN)
        if(TARGET ${tgt} AND MSVC)
            target_compile_options(${tgt} PRIVATE /MP $<$<CONFIG:Debug,RelWithDebInfo>:/FS>)
            set_target_properties(${tgt} PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
            )
        endif()
    endforeach()
endfunction()
