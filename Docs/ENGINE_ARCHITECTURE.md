# LeonEngine2 — Engine Architecture

This document describes the reusable engine layer. Product games live under `Projects/` and must not be referenced from `Engine/`. Companion docs: [GAMEPLAY_FRAMEWORK.md](GAMEPLAY_FRAMEWORK.md), [ENGINE_GAME_BOUNDARY.md](ENGINE_GAME_BOUNDARY.md), [TESTING_ARCHITECTURE.md](TESTING_ARCHITECTURE.md), [ARCHITECTURE.md](ARCHITECTURE.md), [NAMING.md](NAMING.md), [RENDERER.md](RENDERER.md).

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
│   └── UMG/           UWidget hierarchy, FUIRenderer, FUILayout
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
│   ├── UInventoryComponent       (generic actor-slot bag)
│   ├── UFootstepComponent        (cadence SFX; sound path from game)
│   ├── UParticleComponent
│   ├── USkeletalMeshComponent
│   └── USpringArmComponent
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
    ├── APickup
    ├── ALaunchPad
    ├── AProjectile
    ├── AWeaponBase               (owner pawn, magazine, fire stub)
    └── volumes (ABlockingVolume, APhysicsVolume, ANavMeshBoundsVolume)
```

## Dual component model

LeonEngine2 keeps **two** component layers on purpose (see [NAMING.md](NAMING.md) §2):

| Layer | Prefix | Lives in | Used by |
| :--- | :--- | :--- | :--- |
| EnTT POD (map / render ECS) | `F*Component` | `Engine/Components.hpp` | `FMapSerializer`, `FWorldRenderer`, Lightmass |
| Gameplay (`UActorComponent`) | `U*Component` | `Gameplay/` | `AActor` spawn hierarchy, combat, movement |

```text
.lmap load  →  FMapSerializer  →  EnTT FStaticMeshComponent / F*LightComponent / FCameraComponent
GameMode spawn  →  AActor + UBoxComponent / USkeletalMeshComponent / UHealthComponent
FWorldRenderer reads both (EnTT for map meshes/lights; gameplay for skinned pawns)
```

**Rules:** never name an EnTT POD `U*Component`. Never name a non-`UObject` helper `U*`. Net drivers that subclass `UNetDriver` use `U*` (e.g. `ULoopbackNetDriver`).

**Roadmap (not in this pass):** migrate map actors to spawned `AActor` instances so EnTT PODs become an implementation detail behind gameplay components.

EnTT render mirrors (`FTransformComponent`, `FStaticMeshComponent`, `FDirectionalLightComponent`, …) live in `Engine/Components.hpp`. They are not `UActorComponent` subclasses.

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

`AGameModeBase::StartPlay` calls `Login` on Standalone and ListenServer only. Client worlds never spawn a local GameMode during travel; if a leftover GameMode exists on a client world, `StartPlay` does not Login.

Death/respawn uses `RestartPlayer` (destroy pawn, spawn new, same Controller + PlayerState). `UGameInstance` survives `TravelToMap`; the old world is EndPlay + Clear after the new map loads.

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

`FControlInput` is the listen-server control blob (`MoveX/Y`, look yaw/pitch, action bits). Engine bits are Jump/Crouch/Sprint; games OR `CustomBit0+` for held actions. Discrete reload / weapon cycle use ServerRPC.

`UInventoryComponent` stores `AActor*` by slot id (`GiveItem` / `RemoveItem` / `GetActive` / `Cycle`). `AWeaponBase` owns magazine, fire cooldown, and a stub `ServerFire`; traces, VFX, and match rules stay in the game.

`UGameInstance::StartListenServer` / `ConnectToHost` / `ShutdownNetDriver` own IP session setup. Transport is injected with `UIpNetDriver::SetTransportFactory` (ENet lives in the plugin). `UWorld::GetTimerManager()` is the engine countdown list; match FSM stays on the GameMode.

`ACharacter` consumes `CrouchBit` (capsule half-height, crouched walk speed, `FlagCrouched`). `USkeletalMeshComponent::GetBoneLocation` / `GetSocketLocation` expose the evaluated pose. `ACharacter::EnableRagdoll` prefers a `UPhysicsAsset` on the mesh and falls back to capsule simulation.

`FProceduralPrimitiveSpawner` provides `SpawnStaticBox`, `SpawnMeshBox` (optional material path / color), and point/spot lights. Product wrappers map arena surface enums to material paths (`FLeonTournamentArenaBuilder`).

`FGraphicsQuality` / `EGraphicsQuality` / `FGraphicsPreset` live under `Engine/` (Low/Medium/High renderer presets, INI persistence, VRAM estimate). Games call them from menus without duplicating preset tables. Project `DefaultEngine.ini` uses `GraphicsQuality=` (see Sandbox and LeonTournament).

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

`UAIPerceptionComponent` (on `AAIController`) handles sight radius, peripheral FOV, and visibility traces. Affiliation filters and combat blackboard keys (`HasAmmo`, `IsLowHealth`, …) stay in the game.

## Particles

Engine: `FParticleEmitterSettings`, `UParticleComponent`, `FParticleRenderer`. Defaults are neutral (white). Muzzle / tracer / impact colors and spawn sites belong to the game.

## UMG

Engine: `UWidget` tree, `FUIRenderer`, `FUILayout` (boxes / MeasurePadded / Place* / `ResolveViewportSize` / `SyncResolutionScale` / `GamepadEdge`), `UProgressBar`, `UHorizontalBox`, `UVerticalBox`, `USlider`, `UCheckBox`, `UWidgetSwitcher`, `UScrollBox`. Product GI/GM accessors and menu rail widths stay in the game (`FLeonTournamentUILayout`).

## Physics / traces

`UPrimitiveComponent` line traces, sweeps, and overlaps are geometry queries. Damage, teams, and “enemy” filters are gameplay.

```text
Authority (Standalone / ListenServer)
  → UCharacterMovementComponent::PerformMovement
  → ResolvePenetration (WorldStatic SAT MTD, skip walkable floors)
  → MoveAlongFloor / MoveThroughAir
  → kinematic SyncPhysicsTransform → IPhysicsBody

SimulatedProxy
  → pose from net snapshot only
  → no PerformMovement, no dynamics write-back, no Jolt System::Update on Client worlds
```

Gameplay pose is owned by CharacterMovement (pawns) and SimplePhysics write-back (simulating bodies) on authority. The Jolt plugin ticks the native world on authority only; it does not currently map Jolt body poses onto actors. `ACharacter::EnableRagdoll` uses `UPhysicsAsset` bodies + distance constraints when assigned, otherwise the capsule. Ragdoll on SimulatedProxy is visual/net pose, not local dynamics.

## Asset flow

```text
Source (FBX, HDR, PNG, …)
  → Importer (AssetTool / FMeshImporter / FHDRImporter)
  → Native asset (.lmesh, .lskeletalmesh, .lhdr, .lblend, …)
  → UAssetManager / virtual paths `/Game` and `/Engine`
  → Runtime UObject
```

Engine never hardcodes `Projects/<Product>/…` paths.

## World / level

A level stores geometry, lights, volumes, navigation, and placed actors. Match rules live on `AGameModeBase`. Replicated match facts live on `AGameStateBase`. `UWorld` does not own TDM scoring.

## Networking (listen-server lite)

```text
ListenServer  →  BuildSnapshot (NET2)  →  client ApplySnapshot
Client        →  SerializeControlInput →  server ApplyControlInput
Either        →  Call*RPC (RPC1 batch) →  HandleServerRPC / HandleClientRPC
```

`UNetConnection` keeps three channels: snapshot (`Incoming`/`Outgoing`), control input, and framed RPCs. `ULoopbackNetDriver` shuttles all three; `UIpNetDriver` demuxes RPC batches by magic. Actor GUID is the net id.

Snapshots replicate GameState, PlayerStates, possessed pawns, then other `bReplicates` actors (spawn by class name, cull when missing). Relevancy v1: `bAlwaysRelevant` or within `NetCullDistanceSquared` of a viewer pawn. No prediction yet.
