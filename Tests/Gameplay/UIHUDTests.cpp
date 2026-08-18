#include <doctest/doctest.h>

#include "Core/FTimestep.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AHUD.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Gameplay/FOnScreenDebugMessage.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "UMG/UButton.hpp"
#include "UMG/UCanvasPanel.hpp"
#include "UMG/UTextBlock.hpp"
#include "UMG/UUserWidget.hpp"
#include "UMG/UWidget.hpp"
#include "UMG/FUILayout.hpp"
#include "UMG/UProgressBar.hpp"
#include "UMG/UHorizontalBox.hpp"
#include "UMG/UVerticalBox.hpp"
#include "Engine/FMapSerializer.hpp"
#include "Engine/UWorld.hpp"

#include <filesystem>

namespace Leon {

    /** Verifies Login wires PlayerController before AHUD::BeginPlay. */
    class ATestWiredHUD : public AHUD {
    public:
        ATestWiredHUD(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "TestWiredHUD")
            : AHUD(InHandle, InWorld, InName) {}

        void BeginPlay() override {
            bBeginPlaySawPlayerController = (PlayerController != nullptr);
            AHUD::BeginPlay();
            if (PlayerController) {
                auto widget = UUserWidget::CreateWidget<UUserWidget>(PlayerController, "TestMenu");
                if (widget) {
                    widget->AddToViewport(0);
                    bAddedWidget = !GetViewportWidgets().empty();
                }
            }
        }

        bool bBeginPlaySawPlayerController = false;
        bool bAddedWidget = false;
    };

    TEST_SUITE("HUD / UI / PrintString / OpenLevel") {

        TEST_CASE("PrintString creates on-screen message") {
            auto& mgr = FOnScreenDebugMessageManager::Get();
            mgr.Clear();

            PrintString("Hello World", 2.0f);
            CHECK(mgr.GetMessageCount() == 1);
            CHECK(mgr.GetMessages()[0].Text == "Hello World");
            CHECK(mgr.GetMessages()[0].TimeRemaining == doctest::Approx(2.0f));
        }

        TEST_CASE("PrintString duration expires") {
            auto& mgr = FOnScreenDebugMessageManager::Get();
            mgr.Clear();

            PrintString("Temp", 1.0f);
            mgr.Tick(0.4f);
            CHECK(mgr.GetMessageCount() == 1);
            mgr.Tick(0.7f);
            CHECK(mgr.GetMessageCount() == 0);
        }

        TEST_CASE("PrintString multiple messages") {
            auto& mgr = FOnScreenDebugMessageManager::Get();
            mgr.Clear();

            PrintString("A", 2.0f);
            PrintString("B", 3.0f);
            CHECK(mgr.GetMessageCount() == 2);
            CHECK(mgr.GetMessages()[0].Text == "A");
            CHECK(mgr.GetMessages()[1].Text == "B");
        }

        TEST_CASE("PrintString key replacement") {
            auto& mgr = FOnScreenDebugMessageManager::Get();
            mgr.Clear();

            PrintString("First", 5.0f, glm::vec4(1), 42);
            PrintString("Second", 5.0f, glm::vec4(1), 42);
            CHECK(mgr.GetMessageCount() == 1);
            CHECK(mgr.GetMessages()[0].Text == "Second");
        }

        TEST_CASE("UTextBlock SetText / GetText") {
            auto text = std::make_shared<UTextBlock>("Label");
            text->SetText("Change Map");
            CHECK(text->GetText() == "Change Map");
            text->SetText("Updated");
            CHECK(text->GetText() == "Updated");
        }

        TEST_CASE("UUserWidget AddToViewport / RemoveFromParent / visibility") {
            auto world = UWorld::Create("UIWorld");
            auto* pc = world->SpawnActor<APlayerController>("PC");
            auto* hud = world->SpawnActor<AHUD>("HUD");
            REQUIRE(pc);
            REQUIRE(hud);
            hud->SetPlayerController(pc);
            pc->SetHUD(hud);
            world->AddPlayerController(pc);

            auto widget = UUserWidget::CreateWidget<UUserWidget>(pc, "Menu");
            REQUIRE(widget);
            widget->AddToViewport(0);
            CHECK(hud->GetViewportWidgets().size() == 1);
            CHECK(widget->IsVisible());

            widget->SetVisibility(ESlateVisibility::Collapsed);
            CHECK(!widget->IsVisible());

            widget->SetVisibility(ESlateVisibility::Visible);
            widget->RemoveFromParent();
            CHECK(hud->GetViewportWidgets().empty());
        }

        TEST_CASE("UButton click / hover / disabled") {
            auto button = std::make_shared<UButton>("Btn");
            button->SetPosition({10, 10});
            button->SetSize({100, 40});

            FGeometry geom;
            geom.Position = {10, 10};
            geom.Size = {100, 40};
            geom.AbsolutePosition = {10, 10};
            button->Paint(geom);

            bool bClicked = false;
            button->OnClicked.AddLambda([&]() { bClicked = true; });

            CHECK(button->OnMouseMove({50, 25}));
            CHECK(button->IsHovered());
            CHECK(button->GetCurrentState() == EButtonState::Hovered);

            CHECK(button->OnMouseButtonDown(0, {50, 25}));
            CHECK(button->IsPressed());

            CHECK(button->OnMouseButtonUp(0, {50, 25}));
            CHECK(bClicked);

            button->SetIsEnabled(false);
            CHECK(button->GetCurrentState() == EButtonState::Disabled);
            CHECK(!button->OnMouseButtonDown(0, {50, 25}));
        }

        TEST_CASE("AHUD BeginPlay runs after PlayerController wiring") {
            UClassRegistry::Get().RegisterClass<ATestWiredHUD>("ATestWiredHUD");

            auto world = UWorld::Create("HUDWireWorld");
            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            world->SetGameMode(gm);
            gm->HUDClass = "ATestWiredHUD";
            gm->DefaultPawnClass = "None";

            world->InitWorld();
            world->BeginPlay();

            APlayerController* pc = world->GetFirstPlayerController();
            REQUIRE(pc != nullptr);
            auto* hud = pc->GetHUD<ATestWiredHUD>();
            REQUIRE(hud != nullptr);
            CHECK(hud->bBeginPlaySawPlayerController);
            CHECK(hud->bAddedWidget);
            CHECK(hud->GetViewportWidgets().size() == 1);
        }

        TEST_CASE("AHUD spawned from GameMode Login") {
            auto world = UWorld::Create("HUDLoginWorld");
            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            world->SetGameMode(gm);
            gm->HUDClass = "AHUD";
            gm->DefaultPawnClass = "ADefaultPawn";

            world->InitWorld();
            world->BeginPlay();

            APlayerController* pc = world->GetFirstPlayerController();
            REQUIRE(pc != nullptr);
            AHUD* hud = pc->GetHUD();
            REQUIRE(hud != nullptr);
            CHECK(dynamic_cast<AHUD*>(hud) != nullptr);
        }

        TEST_CASE("AHUD class registered") {
            CHECK(UClassRegistry::Get().HasClass("AHUD"));
            CHECK_FALSE(UClassRegistry::Get().HasClass("HUD"));
        }

        TEST_CASE("InputMode GameOnly / UIOnly / GameAndUI") {
            auto world = UWorld::Create("InputModeWorld");
            auto* pc = world->SpawnActor<APlayerController>("PC");

            pc->SetInputModeGameOnly();
            CHECK(pc->GetInputMode() == EInputMode::GameOnly);
            CHECK(pc->IsGameInputAllowed());
            CHECK(!pc->IsUIInputAllowed());
            CHECK(!pc->ShouldShowMouseCursor());

            pc->SetInputModeUIOnly();
            CHECK(pc->GetInputMode() == EInputMode::UIOnly);
            CHECK(!pc->IsGameInputAllowed());
            CHECK(pc->IsUIInputAllowed());
            CHECK(pc->ShouldShowMouseCursor());

            pc->SetInputModeGameAndUI();
            CHECK(pc->GetInputMode() == EInputMode::GameAndUI);
            CHECK(pc->IsGameInputAllowed());
            CHECK(pc->IsUIInputAllowed());
        }

        TEST_CASE("OpenLevel travel rebuilds GameMode / PC / Pawn / HUD") {
            auto world = UWorld::Create("TravelWorld");
            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            gm->HUDClass = "AHUD";
            gm->DefaultPawnClass = "ADefaultPawn";
            world->SetGameMode(gm);
            world->InitWorld();
            world->BeginPlay();

            APlayerController* pcBefore = world->GetFirstPlayerController();
            REQUIRE(pcBefore);
            AHUD* hudBefore = pcBefore->GetHUD();
            REQUIRE(hudBefore);
            APawn* pawnBefore = pcBefore->GetPawn();
            REQUIRE(pawnBefore);

            // Simulate map transition: EndPlay + Clear + new framework spawn
            world->EndPlay();
            world->Clear();

            CHECK(world->GetAllActors().empty());
            CHECK(world->GetFirstPlayerController() == nullptr);

            auto* gm2 = world->SpawnActor<AGameModeBase>("GM");
            gm2->HUDClass = "AHUD";
            gm2->DefaultPawnClass = "ADefaultPawn";
            world->SetGameMode(gm2);
            world->InitWorld();
            world->BeginPlay();

            APlayerController* pcAfter = world->GetFirstPlayerController();
            REQUIRE(pcAfter != nullptr);
            REQUIRE(pcAfter->GetHUD() != nullptr);
            REQUIRE(pcAfter->GetPawn() != nullptr);
            REQUIRE(world->GetGameMode() != nullptr);
            REQUIRE(world->GetGameState() != nullptr);
        }

        TEST_CASE("Travel loads map YAML then rebuilds gameplay framework") {
            auto world = UWorld::Create("MapTravelWorld");
            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            gm->HUDClass = "AHUD";
            gm->DefaultPawnClass = "ADefaultPawn";
            world->SetGameMode(gm);
            world->InitWorld();
            world->BeginPlay();

            auto menu = UUserWidget::CreateWidget<UUserWidget>(world->GetFirstPlayerController());
            menu->AddToViewport(0);
            REQUIRE(world->GetFirstPlayerController()->GetHUD()->GetViewportWidgets().size() >= 1);

            world->EndPlay();
            world->Clear();
            CHECK(world->GetAllActors().empty());

            // Lightweight YAML (no GPU mesh assets) — exercises Clear → deserialize → Login.
            const char* kMapYaml = R"(
Map:
  Name: "TravelTestMap"
Actors:
  - Name: "PropA"
    Transform:
      Translation: [1.0, 2.0, 3.0]
      Rotation: [0.0, 0.0, 0.0]
      Scale: [1.0, 1.0, 1.0]
  - Name: "PropB"
    Transform:
      Translation: [0.0, 0.0, 0.0]
      Rotation: [0.0, 0.0, 0.0]
      Scale: [1.0, 1.0, 1.0]
)";
            FMapSerializer serializer(world);
            REQUIRE(serializer.DeserializeText(kMapYaml));
            CHECK(world->GetAllActors().size() == 2);
            const size_t mapActorCount = world->GetAllActors().size();

            auto* gm2 = world->SpawnActor<AGameModeBase>("GM");
            gm2->HUDClass = "AHUD";
            gm2->DefaultPawnClass = "ADefaultPawn";
            world->SetGameMode(gm2);
            world->InitWorld();
            world->BeginPlay();

            REQUIRE(world->GetFirstPlayerController() != nullptr);
            REQUIRE(world->GetFirstPlayerController()->GetHUD() != nullptr);
            REQUIRE(world->GetFirstPlayerController()->GetPawn() != nullptr);
            CHECK(world->GetFirstPlayerController()->GetHUD()->GetViewportWidgets().empty());
            CHECK(world->GetAllActors().size() > mapActorCount);
        }

        TEST_CASE("UGameplayStatics accessors") {
            auto world = UWorld::Create("StaticsWorld");
            auto* gm = world->SpawnActor<AGameModeBase>("GM");
            gm->HUDClass = "AHUD";
            world->SetGameMode(gm);
            world->InitWorld();
            world->BeginPlay();

            CHECK(UGameplayStatics::GetGameMode(world.get()) == gm);
            CHECK(UGameplayStatics::GetGameState(world.get()) != nullptr);
            CHECK(UGameplayStatics::GetPlayerController(world.get()) != nullptr);
            CHECK(UGameplayStatics::GetPlayerPawn(world.get()) != nullptr);
        }

        TEST_CASE("UCanvasPanel hierarchy") {
            auto canvas = std::make_shared<UCanvasPanel>("Canvas");
            auto child = std::make_shared<UTextBlock>("Child");
            child->SetText("Hi");
            canvas->AddChild(child);
            CHECK(canvas->GetChildrenCount() == 1);
            canvas->ClearChildren();
            CHECK(canvas->GetChildrenCount() == 0);
        }

        TEST_CASE("UCanvasPanel center anchors follow parent resize") {
            auto canvas = std::make_shared<UCanvasPanel>("Canvas");
            canvas->SetSize({1280.0f, 720.0f});

            auto panel = std::make_shared<UCanvasPanel>("Panel");
            constexpr float HalfW = 210.0f;
            constexpr float HalfH = 90.0f;
            canvas->AddChild(panel, FAnchors::Center(), FMargin(-HalfW, -HalfH, -HalfW, -HalfH));

            canvas->PerformLayout({1280.0f, 720.0f});
            CHECK(panel->GetPosition().x == doctest::Approx(640.0f - HalfW));
            CHECK(panel->GetPosition().y == doctest::Approx(360.0f - HalfH));
            CHECK(panel->GetSize().x == doctest::Approx(420.0f));
            CHECK(panel->GetSize().y == doctest::Approx(180.0f));

            canvas->PerformLayout({1920.0f, 1080.0f});
            CHECK(panel->GetPosition().x == doctest::Approx(960.0f - HalfW));
            CHECK(panel->GetPosition().y == doctest::Approx(540.0f - HalfH));
            CHECK(panel->GetSize().x == doctest::Approx(420.0f));
            CHECK(panel->GetSize().y == doctest::Approx(180.0f));
        }

        TEST_CASE("FUILayout MeasurePadded and PlaceText do not require GL") {
            auto canvas = std::make_shared<UCanvasPanel>("LayoutRoot");
            canvas->SetSize({1280.0f, 720.0f});
            const glm::vec2 padded = FUILayout::MeasurePadded("HP", FUILayout::kFsBody);
            CHECK(padded.y > 0.0f);

            auto text = std::make_shared<UTextBlock>("Label");
            text->SetText("Hello");
            text->SetFontScale(FUILayout::kFsBody);
            FUILayout::PlaceTextTL(*canvas, text, 16.0f, 12.0f);
            CHECK(canvas->GetChildrenCount() == 1);
            CHECK(canvas->GetSlots().size() == 1);
        }

        TEST_CASE("UProgressBar clamps percent") {
            UProgressBar bar("HP");
            bar.SetPercent(1.5f);
            CHECK(bar.GetPercent() == doctest::Approx(1.0f));
            bar.SetPercent(-0.2f);
            CHECK(bar.GetPercent() == doctest::Approx(0.0f));
            bar.SetPercent(0.4f);
            CHECK(bar.GetPercent() == doctest::Approx(0.4f));
        }

        TEST_CASE("UHorizontalBox and UVerticalBox stack child sizes") {
            auto h = std::make_shared<UHorizontalBox>("H");
            auto a = std::make_shared<UWidget>("A");
            auto b = std::make_shared<UWidget>("B");
            a->SetSize({40.0f, 10.0f});
            b->SetSize({20.0f, 16.0f});
            h->SetSlotPadding(4.0f);
            h->AddChild(a);
            h->AddChild(b);
            h->PerformLayout();
            CHECK(a->GetPosition().x == doctest::Approx(0.0f));
            CHECK(b->GetPosition().x == doctest::Approx(44.0f));
            CHECK(h->GetSize().x == doctest::Approx(64.0f));
            CHECK(h->GetSize().y == doctest::Approx(16.0f));

            auto v = std::make_shared<UVerticalBox>("V");
            auto c = std::make_shared<UWidget>("C");
            auto d = std::make_shared<UWidget>("D");
            c->SetSize({8.0f, 12.0f});
            d->SetSize({30.0f, 5.0f});
            v->SetSlotPadding(2.0f);
            v->AddChild(c);
            v->AddChild(d);
            v->PerformLayout();
            CHECK(d->GetPosition().y == doctest::Approx(14.0f));
            CHECK(v->GetSize().x == doctest::Approx(30.0f));
            CHECK(v->GetSize().y == doctest::Approx(19.0f));
        }
    }

} // namespace Leon
