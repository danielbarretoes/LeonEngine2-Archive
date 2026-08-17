#include "doctest.h"

#include "Audio/FAudioDevice.hpp"
#include "Audio/USoundWave.hpp"
#include "Audio/UAudioComponent.hpp"
#include "Core/FProjectPaths.hpp"

#include <cstdlib>
#include <filesystem>

#if defined(_WIN32)
#include <stdlib.h>
#endif

using namespace Leon;

TEST_CASE("Audio null device init and play no-crash") {
#if defined(_WIN32)
    _putenv_s("LEON_AUDIO_NULL", "1");
#else
    setenv("LEON_AUDIO_NULL", "1", 1);
#endif

    FAudioDevice::Get().Shutdown();
    CHECK(FAudioDevice::Get().Init());
    CHECK(FAudioDevice::Get().IsNullDevice());

    auto wave = CreateRef<USoundWave>("Empty");
    FAudioDevice::Get().PlaySound2D(wave, 1.0f);
    FAudioDevice::Get().Tick(0.016f);
    FAudioDevice::Get().Shutdown();
}

TEST_CASE("UAudioComponent play stop on null device") {
#if defined(_WIN32)
    _putenv_s("LEON_AUDIO_NULL", "1");
#else
    setenv("LEON_AUDIO_NULL", "1", 1);
#endif
    FAudioDevice::Get().Shutdown();
    REQUIRE(FAudioDevice::Get().Init());

    UAudioComponent comp("TestAudio");
    comp.Play();
    comp.Stop();
    FAudioDevice::Get().Shutdown();
}

TEST_CASE("USoundWave resolves /Game/Audio SFX without extension") {
    const std::string project = FProjectPaths::LocateProjectFile("Projects/LeonTournament/LeonTournament.lproject");
    REQUIRE_FALSE(project.empty());
    FProjectPaths::SetProjectRoot(project);

    const std::string resolved = FProjectPaths::ResolveVirtualPath("/Game/Audio/SFX_RifleFire");
    CHECK(resolved.find(".wav") != std::string::npos);
    CHECK(std::filesystem::exists(resolved));

    auto wave = USoundWave::Load("/Game/Audio/SFX_RifleFire");
    REQUIRE(wave != nullptr);
    CHECK(wave->IsValid());
    CHECK(wave->GetAssetPath().find(".wav") != std::string::npos);

    auto again = USoundWave::Load("/Game/Audio/SFX_RifleFire");
    CHECK(again.get() == wave.get());
}
