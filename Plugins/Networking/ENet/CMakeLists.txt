# ENet transport plugin. Gameplay never includes <enet/enet.h>.
# Vendored: ThirdParty/enet (lsalzman/enet v1.3.18).

if(NOT DEFINED LEON_ENGINE_ROOT)
    get_filename_component(LEON_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
endif()

set(ENET_ROOT "${LEON_ENGINE_ROOT}/ThirdParty/enet")
if(NOT EXISTS "${ENET_ROOT}/include/enet/enet.h")
    message(FATAL_ERROR "Missing vendored enet at ${ENET_ROOT}. Expected include/enet/enet.h")
endif()

if(NOT TARGET enet)
    add_subdirectory("${ENET_ROOT}" "${CMAKE_BINARY_DIR}/_deps/enet-build" EXCLUDE_FROM_ALL)
endif()
if(TARGET enet)
    target_include_directories(enet PUBLIC "${ENET_ROOT}/include")
endif()

add_library(LeonEngine_ENet STATIC
    Source/FENetTransport.cpp
)
add_library(Leon::ENet ALIAS LeonEngine_ENet)

target_include_directories(LeonEngine_ENet PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

target_link_libraries(LeonEngine_ENet PUBLIC
    Leon::Engine
    enet
)

if(WIN32)
    target_link_libraries(LeonEngine_ENet PUBLIC ws2_32 winmm)
endif()

target_compile_features(LeonEngine_ENet PUBLIC cxx_std_20)
