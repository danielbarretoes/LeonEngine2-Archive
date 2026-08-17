# LeonEngine2 — Engine Architecture

This document describes the reusable engine layer. Product games live under `Projects/` and must not be referenced from `Engine/`. Companion docs: [GAMEPLAY_FRAMEWORK.md](GAMEPLAY_FRAMEWORK.md), [ENGINE_GAME_BOUNDARY.md](ENGINE_GAME_BOUNDARY.md), [TESTING_ARCHITECTURE.md](TESTING_ARCHITECTURE.md), [ARCHITECTURE.md](ARCHITECTURE.md), [NAMING.md](NAMING.md).

## High-level tree

```text
LeonEngine2/
├── Engine/Source/Runtime/
│   ├── Core/          FApplication, FWindow, FInput, FConfigFile, FLog
│   ├── Engine/        UEngine, UWorld, UGameInstance, serializers, ECS PODs
│   ├── Gameplay/      AActor … ACharacter, components, UGameplayStatics
│   ├── AI/            AAIController, UBehaviorTree, UBlackboard*
│   ├── Assets/        UAssetManager, meshes, skeleton, BlendSpace, HDR
│   ├── Renderer/      FWorldRenderer, PBR, IBL, particles, post
│   ├── RHI/           IGraphicsContext, IRenderAPI, FShader, FTexture
│   ├── Physics/       IPhysicsScene (Jolt behind a plugin)
│   ├── Lightmass/     static lighting bake types
│   └── UMG/           UWidget hierarchy, FUIRenderer
├── Plugins/           OpenGL RHI, Jolt, ENet
├── Projects/          Sandbox (sample), LeonTournament (product)
└── Tests/             Engine suite + per-project suites
```

## Class hierarchy (gameplay)

```text
UObject
├── UGameInstance
├── UActorComponent
│   ├── USceneComponent → UPrimitiveComponent → UShapeComponent
│   │                                         → UBox/Sphere/CapsuleComponent
│   ├── UCharacterMovementComponent
│   ├── UNavMovementComponent
│   ├── UPathFollowingComponent
│   ├── UHealthComponent          (generic hit points)
│   ├── UCombatComponent          (generic attack-gate / cooldown)
│   ├── UParticleComponent
│   ├── USkeletalMeshComponent
│   └── USpringArmComponent / UCameraComponent
└── AActor
    ├── APawn → ADefaultPawn
    │        → ACharacter
    ├── AController → APlayerController
    │              → AAIController
    ├── APlayerState
    ├── AGameModeBase
    ├── AGameStateBase
    ├── AHUD
    ├── ACameraActor
    └── volumes (ABlockingVolume, APhysicsVolume, ANavMeshBoundsVolume)
```

EnTT render mirrors (`FTransformComponent`, `FStaticMeshComponent`, …) live in `Engine/Components.hpp`. They are not `UActorComponent` subclasses. That aggregation file is the ECS registry, not a UObject module dump.

## Startup lifecycle

Owned by `UEngine::InternalRun`:

```text
Process
  → FApplication / FWindow
  → .lproject + Multi-INI
  → UGameInstance (project factory, else UGameInstance)
  → UWorld
  → Load map (.lmap via virtual path)
  → AGameModeBase (class from FGameModeConfig)
  → InitWorld / Login / GameState / PlayerController / PlayerState / Pawn
  → BeginPlay
  → Tick (input → controller → pawn/movement → world → render)
```

Default classes are **not** compiled into the engine as product types. `FGameModeConfig` is filled from:

1. `DefaultEngine.ini` `/Script/EngineSettings.GameMapsSettings` `GlobalDefaultGameMode`
2. `DefaultGame.ini` `/Script/<ProjectName>.GameMode`
3. `DefaultGame.ini` `/Script/Engine.GameModeBase`
4. `.lproject` `DefaultGameMode` / `DefaultMap`

The project `Main` registers classes on `UClassRegistry` and may call `UEngine::SetGameInstanceFactory` before `UEngine::Run`.

## Input flow

```text
Hardware → FInput / FInputSettings → APlayerController → APawn / ACharacter → UCharacterMovementComponent
```

Engine mappings are generic (`MoveForward`, `Look`, `Jump`, `Sprint`). Fire / Reload and any weapon action are bound in the game project.

## Animation flow

```text
ACharacter
  → USkeletalMeshComponent
  → UAnimInstance (reads Speed, Direction, bIsFalling, VerticalSpeed)
  → FAnimStateMachine
  → UBlendSpace / UAnimSequence
```

`UBlendSpace` is an animation asset. Gameplay rules do not live in the blend asset.

## AI / navigation flow

```text
AAIController decision
  → MoveTo
  → UNavigationSystem query
  → path
  → UPathFollowingComponent
  → UCharacterMovementComponent
  → ACharacter
```

AI must not teleport the pawn to the destination. BehaviorTree / Blackboard **framework** is engine; TDM keys and combat trees are project assets / project controllers.

## Particles

Engine: `FParticleEmitterSettings`, `UParticleComponent`, `FParticleRenderer`. Defaults are neutral (white). Muzzle / tracer / impact colors and spawn sites belong to the game.

## Physics / traces

`UPrimitiveComponent` line traces, sweeps, and overlaps are geometry queries. Damage, teams, and “enemy” filters are gameplay.

## Asset flow

```text
Source (FBX, HDR, PNG, …)
  → Importer (LeonAssetTool / FMeshImporter / FHDRImporter)
  → Native asset (.lmesh, .lskeletalmesh, .lhdr, .lblend, …)
  → UAssetManager / virtual paths `/Game` and `/Engine`
  → Runtime UObject
```

Engine never hardcodes `Projects/<Product>/…` paths.

## World / level

A level stores geometry, lights, volumes, navigation, and placed actors. Match rules live on `AGameModeBase`. Replicated match facts live on `AGameStateBase`. `UWorld` does not own TDM scoring.
