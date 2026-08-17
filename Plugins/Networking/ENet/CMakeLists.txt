# ENet transport plugin. Gameplay never includes <enet/enet.h>.

include(FetchContent)
FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG v1.3.18
    GIT_SHALLOW TRUE
)
set(ENET_STATIC TRUE CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(enet)
if(TARGET enet AND DEFINED enet_SOURCE_DIR)
    target_include_directories(enet PUBLIC "${enet_SOURCE_DIR}/include")
endif()

add_library(LeonEngine_ENet STATIC
    src/FENetTransport.cpp
)
add_library(Leon::ENet ALIAS LeonEngine_ENet)

target_include_directories(LeonEngine_ENet PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(LeonEngine_ENet PUBLIC
    Leon::Engine
    enet
)

if(WIN32)
    target_link_libraries(LeonEngine_ENet PUBLIC ws2_32 winmm)
endif()

target_compile_features(LeonEngine_ENet PUBLIC cxx_std_20)
