#include <doctest/doctest.h>

#include "Core/FTimestep.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/FControlInput.hpp"
#include "Gameplay/FProceduralPrimitiveSpawner.hpp"
#include "Gameplay/UFootstepComponent.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"

#include <vector>

TEST_SUITE("FControlInput") {

    TEST_CASE("serialize roundtrip preserves analog move look and bits") {
        Leon::FControlInput in;
        in.MoveX = 0.5f;
        in.MoveY = -1.0f;
        in.LookYaw = 42.0f;
        in.LookPitch = -12.5f;
        in.SetAction(Leon::FControlInput::JumpBit, true);
        in.SetAction(Leon::FControlInput::CustomBit0, true);

        std::vector<uint8_t> bytes;
        in.Serialize(bytes);
        Leon::FControlInput out;
        REQUIRE(out.Deserialize(bytes.data(), bytes.size()));
        CHECK(out.MoveX == doctest::Approx(0.5f));
        CHECK(out.MoveY == doctest::Approx(-1.0f));
        CHECK(out.LookYaw == doctest::Approx(42.0f));
        CHECK(out.LookPitch == doctest::Approx(-12.5f));
        CHECK(out.HasAction(Leon::FControlInput::JumpBit));
        CHECK(out.HasAction(Leon::FControlInput::CustomBit0));
        CHECK_FALSE(out.HasAction(Leon::FControlInput::SprintBit));
    }

    TEST_CASE("hardware sample with no devices is idle") {
        Leon::FControlInput sampled = Leon::FControlInput::SampleFromHardware();
        CHECK(sampled.MoveX == doctest::Approx(0.0f));
        CHECK(sampled.MoveY == doctest::Approx(0.0f));
        CHECK_FALSE(sampled.HasAction(Leon::FControlInput::JumpBit));
    }
}

TEST_SUITE("ACharacter crouch") {

    TEST_CASE("CrouchBit shrinks capsule and sets anim flag") {
        auto world = Leon::UWorld::Create("CrouchWorld");
        auto* ch = world->SpawnActor<Leon::ACharacter>("Hero");
        REQUIRE(ch);
        const float standing = ch->GetCapsuleHalfHeight();
        Leon::FControlInput in;
        in.SetAction(Leon::FControlInput::CrouchBit, true);
        std::vector<uint8_t> bytes;
        in.Serialize(bytes);
        ch->ApplyControlInput(bytes.data(), bytes.size());
        ch->Tick(0.016f);
        CHECK(ch->IsCrouched());
        CHECK(ch->GetCapsuleHalfHeight() < standing);
        CHECK(ch->GetAnimRepState().IsCrouched());

        in.SetAction(Leon::FControlInput::CrouchBit, false);
        bytes.clear();
        in.Serialize(bytes);
        ch->ApplyControlInput(bytes.data(), bytes.size());
        ch->Tick(0.016f);
        CHECK_FALSE(ch->IsCrouched());
        CHECK(ch->GetCapsuleHalfHeight() == doctest::Approx(standing));
    }
}

TEST_SUITE("ACharacter control schema") {

    TEST_CASE("ApplyControlInput sets look") {
        auto world = Leon::UWorld::Create("ControlWorld");
        auto* ch = world->SpawnActor<Leon::ACharacter>("Hero");
        Leon::FControlInput in;
        in.LookYaw = 33.0f;
        in.LookPitch = 8.0f;
        std::vector<uint8_t> bytes;
        in.Serialize(bytes);
        ch->ApplyControlInput(bytes.data(), bytes.size());
        CHECK(ch->GetControlYaw() == doctest::Approx(33.0f));
        CHECK(ch->GetControlPitch() == doctest::Approx(8.0f));
    }
}

TEST_SUITE("FProceduralPrimitiveSpawner") {

    TEST_CASE("SpawnStaticBox creates a WorldStatic collider") {
        auto world = Leon::UWorld::Create("BoxWorld");
        auto* box = Leon::FProceduralPrimitiveSpawner::SpawnStaticBox(world.get(), "Wall", {0.0f, 1.0f, 0.0f},
                                                                      {2.0f, 2.0f, 2.0f});
        REQUIRE(box);
        auto* prim = box->FindComponentByClass<Leon::UBoxComponent>();
        REQUIRE(prim);
        CHECK(prim->GetCollisionObjectType() == Leon::ECollisionChannel::WorldStatic);
        CHECK(box->GetActorLocation().y == doctest::Approx(1.0f));
    }
}

TEST_SUITE("UFootstepComponent") {

    TEST_CASE("empty path does not throw and cooldown stays zero") {
        auto world = Leon::UWorld::Create("StepWorld");
        auto* ch = world->SpawnActor<Leon::ACharacter>("Walker");
        auto steps = ch->AddActorComponent<Leon::UFootstepComponent>("Footsteps");
        CHECK(steps->GetSoundPath().empty());
        steps->Tick(0.16f);
        CHECK(steps->GetCooldownRemaining() == doctest::Approx(0.0f));
    }
}
