#include <doctest/doctest.h>

#include "Core/FInput.hpp"
#include "Renderer/FRenderDebugHotkeys.hpp"

#include <set>
#include <span>
#include <string>

TEST_SUITE("Render debug hotkeys") {

    TEST_CASE("shader cycles cover every unique u_DebugMode except duplicates") {
        using Leon::FRenderDebugHotkeys;
        std::set<int> modes;
        auto add = [&](std::span<const Leon::FRenderDebugView> cycle) {
            for (const auto& view : cycle)
                modes.insert(view.Mode);
        };
        add(FRenderDebugHotkeys::MaterialCycle);
        add(FRenderDebugHotkeys::GeometryCycle);
        add(FRenderDebugHotkeys::LightingCycle);
        add(FRenderDebugHotkeys::IBLMapCycle);
        add(FRenderDebugHotkeys::ShadowCycle);
        modes.insert(13); // F9 planar

        const int expected[] = {1,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 18, 19, 20,
                                21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 37, 38, 39, 40};
        for (int mode : expected)
            CHECK(modes.contains(mode));
        CHECK(modes.size() == sizeof(expected) / sizeof(expected[0]));
        CHECK_FALSE(modes.contains(2));  // duplicate of prefilter mip 0
        CHECK_FALSE(modes.contains(17)); // duplicate of world normals
        CHECK_FALSE(modes.contains(36)); // duplicate of Lo
    }

    TEST_CASE("F4 wraps the material cycle and F12 restores defaults") {
        using Leon::FRenderDebugHotkeys;
        Leon::FRenderDebugState state;
        const char* log = "";

        REQUIRE(FRenderDebugHotkeys::ApplyKey(state, Leon::Key::F4, log));
        CHECK(state.ShaderMode == 14);
        REQUIRE(FRenderDebugHotkeys::ApplyKey(state, Leon::Key::F4, log));
        CHECK(state.ShaderMode == 15);

        state.bWireframe = true;
        state.bPostProcessEnabled = false;
        state.PostProcessDebugMode = 5;
        REQUIRE(FRenderDebugHotkeys::ApplyKey(state, Leon::Key::F12, log));
        CHECK(state.ShaderMode == 0);
        CHECK(state.PostProcessDebugMode == 0);
        CHECK_FALSE(state.bWireframe);
        CHECK(state.bPostProcessEnabled);
    }

    TEST_CASE("F10 cycles post-process isolation and clears shader debug") {
        using Leon::FRenderDebugHotkeys;
        Leon::FRenderDebugState state;
        const char* log = "";
        state.ShaderMode = 14;
        REQUIRE(FRenderDebugHotkeys::ApplyKey(state, Leon::Key::F10, log));
        CHECK(state.ShaderMode == 0);
        CHECK(state.PostProcessDebugMode == 1);
        for (int i = 0; i < 5; ++i)
            FRenderDebugHotkeys::ApplyKey(state, Leon::Key::F10, log);
        CHECK(state.PostProcessDebugMode == 0);
    }

    TEST_CASE("shader and post view names match cycle tables") {
        using Leon::FRenderDebugHotkeys;
        CHECK(std::string(FRenderDebugHotkeys::ShaderViewName(0)) == "Lit");
        CHECK(std::string(FRenderDebugHotkeys::ShaderViewName(13)) == "Planar Reflections Buffer");
        CHECK(std::string(FRenderDebugHotkeys::ShaderViewName(40)) == "Point Shadow Factor");
        CHECK(std::string(FRenderDebugHotkeys::CycleNameOrNull(16, FRenderDebugHotkeys::MaterialCycle)) ==
              std::string("Roughness"));
        CHECK(FRenderDebugHotkeys::CycleNameOrNull(11, FRenderDebugHotkeys::MaterialCycle) == nullptr);
        CHECK(std::string(FRenderDebugHotkeys::PostViewName(5)).find("SSAO") != std::string::npos);
    }
}
