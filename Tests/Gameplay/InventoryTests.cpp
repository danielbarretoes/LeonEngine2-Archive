#include <doctest/doctest.h>

#include "Engine/UWorld.hpp"
#include "Engine/ENetTypes.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/UInventoryComponent.hpp"
#include "Gameplay/AWeaponBase.hpp"

TEST_SUITE("UInventoryComponent") {

    TEST_CASE("give, occupy, active slot, and cycle") {
        auto world = Leon::UWorld::Create("InvWorld");
        auto* owner = world->SpawnActor<Leon::AActor>("Owner");
        auto inv = owner->AddActorComponent<Leon::UInventoryComponent>("Inventory");
        inv->SetSlotCount(4);
        inv->SetDestroyItemsOnEndPlay(false);

        auto* a = world->SpawnActor<Leon::AActor>("ItemA");
        auto* b = world->SpawnActor<Leon::AActor>("ItemB");
        auto* c = world->SpawnActor<Leon::AActor>("ItemC");

        CHECK(inv->GiveItem(0, a));
        CHECK(inv->GiveItem(2, b));
        CHECK_FALSE(inv->GiveItem(0, c));
        CHECK_FALSE(inv->GiveItem(9, c));
        CHECK(inv->HasItem(0));
        CHECK_FALSE(inv->HasItem(1));
        CHECK(inv->GetItem(2) == b);

        CHECK(inv->SetActiveSlot(0));
        CHECK_FALSE(inv->SetActiveSlot(1));
        CHECK(inv->GetActiveItem() == a);

        CHECK(inv->Cycle(1) == 2);
        CHECK(inv->GetActiveItem() == b);
        CHECK(inv->Cycle(1) == 0);
        CHECK(inv->Cycle(-1) == 2);
        CHECK(inv->GetItems().size() == 2);
    }

    TEST_CASE("remove and end play destroy occupants") {
        auto world = Leon::UWorld::Create("InvDestroy");
        auto* owner = world->SpawnActor<Leon::AActor>("Owner");
        auto inv = owner->AddActorComponent<Leon::UInventoryComponent>("Inventory");
        inv->SetSlotCount(2);

        auto* a = world->SpawnActor<Leon::AActor>("KeepOrKill");
        auto* b = world->SpawnActor<Leon::AActor>("Gone");
        CHECK(inv->GiveItem(0, a));
        CHECK(inv->GiveItem(1, b));
        CHECK(inv->RemoveItem(1, true));
        CHECK(b->IsPendingKill());
        CHECK_FALSE(inv->HasItem(1));
        CHECK(inv->HasItem(0));

        inv->EndPlay();
        CHECK(a->IsPendingKill());
        CHECK_FALSE(inv->HasItem(0));
    }
}

TEST_SUITE("AWeaponBase") {

    TEST_CASE("magazine fire gate and reload tick") {
        auto world = Leon::UWorld::Create("WeaponWorld");
        auto* pawn = world->SpawnActor<Leon::ACharacter>("Owner");
        auto* weap = world->SpawnActor<Leon::AWeaponBase>("Weapon");
        weap->SetAmmoCapacity(3);
        weap->SetFireRate(10.0f);
        weap->SetReloadTime(0.25f);
        weap->ResetMagazine();

        CHECK_FALSE(weap->CanFire());
        weap->SetOwnerPawn(pawn);
        CHECK(weap->CanFire());
        CHECK(weap->GetCurrentAmmo() == 3);

        CHECK(weap->ServerFire());
        CHECK(weap->GetCurrentAmmo() == 2);
        CHECK_FALSE(weap->CanFire());
        weap->Tick(0.2f);
        CHECK(weap->CanFire());
        CHECK(weap->ServerFire());
        CHECK(weap->ServerFire() == false);
        weap->Tick(0.2f);
        CHECK(weap->ServerFire());
        CHECK(weap->GetCurrentAmmo() == 0);
        CHECK(weap->IsReloadInProgress());

        weap->Tick(0.3f);
        CHECK_FALSE(weap->IsReloadInProgress());
        CHECK(weap->GetCurrentAmmo() == 3);
    }

    TEST_CASE("client net mode does not fire or reload") {
        auto world = Leon::UWorld::Create("WeaponClient");
        world->SetNetMode(Leon::ENetMode::Client);
        auto* pawn = world->SpawnActor<Leon::ACharacter>("Owner");
        auto* weap = world->SpawnActor<Leon::AWeaponBase>("Weapon");
        weap->SetOwnerPawn(pawn);
        weap->SetAmmoCapacity(2);
        weap->ResetMagazine();
        CHECK_FALSE(weap->ServerFire());
        CHECK(weap->GetCurrentAmmo() == 2);
        CHECK_FALSE(weap->StartReload());
        CHECK_FALSE(weap->IsReloadInProgress());
    }
}
