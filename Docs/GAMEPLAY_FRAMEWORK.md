# LeonEngine2 — Gameplay Framework

Unreal-aligned responsibilities. There is one canonical type per concept; no compatibility aliases.

## Responsibility matrix

| Type | Owns | Must not own |
| :--- | :--- | :--- |
| `UGameInstance` | Application/session lifetime, travel URL, net mode, listen/join via injected `INetTransport`, project services that survive map changes | Match score, kills, current pawn, match timer, combat state |
| `AGameModeBase` | Authority rules, login, `Logout`, `RestartPlayer`, spawn/respawn, default class selection | HUD widgets, camera, input, animation, replicated per-player stats |
| `AGameMode` | Match start/end (`SetMatchState`, `StartMatch` / `EndMatch` / `RestartGame`), `PlayerCanRestart` | Team scores, HUD, pawn movement |
| `AGameStateBase` | Player array, elapsed time, scoreboard rank (`GetPlayerArraySortedByScore`) | Input, camera, spawn algorithms |
| `AGameState` | Replicated match phase (`EMatchState`) and remaining clock | Weapon impl, local UI |
| `APlayerState` | Persistent per-player identity (name, id, team, score, kills/deaths) across pawn replacement; scoreboard row | Movement, mesh, weapons |
| `APlayerController` | Input, possession, camera, local HUD interaction, commands to authority | Health, score, weapon implementation, match rules |
| `APawn` / `ACharacter` | Capsule, movement, mesh, jump/crouch/fall/land, camera attach, `FControlInput` serialize, ragdoll helper | Rifle/ammo, TDM, team assignment, kill scoring |
| `FControlInput` | Analog move, look, Jump/Crouch/Sprint bits | Fire, reload, weapon cycle, teams |
| `FTimerManager` | World one-shot / looping delegates | Match FSM, kill scoring |
| `UFootstepComponent` | Grounded cadence SFX from a game-supplied path | Product audio asset names hardcoded |
| `UInventoryComponent` | Actor slot bag (`GiveItem` / `RemoveItem` / `Cycle`) | Weapon traces, ammo refill rules, teams |
| `AWeaponBase` | Owner pawn, magazine, fire cooldown, reload timer | TDM, presets, VFX, damage traces |
| `UActorComponent` | One explicit job (health, movement, particles, …) | Secret product-class dependencies |

## Match flow (canonical owners)

```text
Menu (GameInstance + HUD widgets)
  → OpenLevel (UGameplayStatics → UEngine travel)
  → GameMode created on authority
  → GameState spawned by GameMode/World
  → Player login → PlayerState + PlayerController
  → Spawn pawn/character → Possess
  → Match start (`AGameMode::StartMatch` → `SetMatchState(InProgress)` → `HandleMatchHasStarted`; remaining clock ticks on GameState)
  → Gameplay
  → Death (Health on authority) → `NotifyActorKilled` on GameMode; game subclasses score / respawn
  → `PlayerCanRestart` then RestartPlayer: destroy pawn, spawn new pawn, Possess (same Controller + PlayerState)
  → Logout (optional): UnPossess, unregister PlayerState, destroy HUD / PC / PlayerState
  → Match end (`AGameMode::EndMatch` → `SetMatchState(WaitingPostMatch)` → `HandleMatchHasEnded`)
  → Optional rematch (`RestartGame` resets to WaitingToStart then StartMatch)
```

`AGameModeBase::RestartPlayer` is the single respawn path. If `DefaultPawnClass` is empty or `"None"`, the base implementation UnPossesses and destroys the old pawn without spawning. Game subclasses that keep `DefaultPawnClass = "None"` (menu-first) override `RestartPlayer` to spawn their character.

`AController::Possess` UnPossesses any previous owner of the target pawn before binding, so Controller→Pawn pointers stay consistent.

Scoreboard HUD reads `UWorld::GetGameState()` → `PlayerArray` / `GetPlayerArraySortedByScore()`. Each row is an `APlayerState` (name, `Score`, game K/D). `AGameMode` does not exist on clients and must not own the board. Tournament `ALeonTournamentGameState::GetSortedScoreboard()` is that query filtered to `ALeonTournamentPlayerState`. Local highlight uses `APlayerController::GetPlayerState()`.

`AGameModeBase::ChoosePlayerStart(AController* InPlayer)` returns the first enabled `APlayerStart` that is not tagged `"Dummy"`. Pass the Controller so games can pick team / named starts. `FindPlayerStart(InPlayer, InIncomingName)` still matches tag or actor name first.

`AGameModeBase::Logout` is the rules-side exit path (does not destroy the World or GameInstance).

`AGameStateBase` / game `GameState` mutators and `PlayerState` score setters no-op unless `AActor::IsNetworkAuthority()` (not `ENetMode::Client`). Replication writes fields in `DeserializeReplication` and does not go through those setters.

`UHealthComponent::ApplyDamage` / `Heal` run only on network authority. `BecomeDead` notifies `AGameModeBase::NotifyActorKilled` when a GameMode is set. `UGameplayStatics::ApplyPointDamage` / `ApplyRadialDamage` call `NotifyActorDamaged` only (kill notification is not duplicated). `EndPlay` clears `OnDeath` / `OnDamage` delegates.

`APawn::GetPlayerState()` prefers the possessing Controller, then the PlayerState cached at `PossessedBy`. `UnPossessed` clears that cache. `APlayerState::GetPlayerController()` scans `UWorld` player controllers.

Only `AGameMode` / `AGameModeBase` (and game subclasses) decide when a match starts or ends. `UGameInstance` may request travel; it does not increment kills.

## Character component hierarchy (Unreal)

```text
ACharacter
└── CapsuleComponent = RootComponent   // collision + actor location (capsule center)
    └── Mesh (USkeletalMeshComponent)  // SetupAttachment(Capsule); relative feet offset
```

- `FTransformComponent` is the single world-pose source for the actor (capsule center). Root relative transform is identity.
- `GetActorLocation()` reads EnTT, not a separate root transform. Children resolve via `USceneComponent` attach.
- First-person camera uses `GetPawnViewLocation()` = center + (EyeHeight − HalfHeight).
- Mesh is visual-only; movement and traces use the capsule.
- Capsule half-height is Unreal-style: half of total height including hemispheres (`cylinderHalf = HalfHeight − Radius`).

## Default class chain

```text
Project configuration (.lproject + DefaultEngine.ini + DefaultGame.ini)
  → UGameInstance (factory from project Main)
  → AGameModeBase (GlobalDefaultGameMode / GameModeClass)
  → GameState / PlayerState / PlayerController / Pawn class names on the GameMode
  → UClassRegistry::CreateActorOfClass
```

Engine generic fallbacks: `AGameModeBase`, `AGameMode`, `AGameStateBase`, `AGameState`, `APlayerController`, `APlayerState`, `ADefaultPawn`, `AHUD`.

## LeonTournament mapping

| Engine base | Project class | Role |
| :--- | :--- | :--- |
| `UGameInstance` | `ULeonTournamentGameInstance` | Session mode, LAN host/join wrappers, auto-offline match CLI |
| `AGameMode` | `ALeonTournamentGameMode` | TDM rules, team assign, spawn, kill limit, duration |
| `AGameState` | `ALeonTournamentGameState` | TeamScores, MatchState, remaining clock (replicated), Winner |
| `APlayerState` | `ALeonTournamentPlayerState` | Team, kills, deaths |
| `APlayerController` | `ALeonTournamentPlayerController` | Look/fire commands, local HUD |
| `ACharacter` | `ALeonTournamentCharacter` | First-person shooter pawn; arsenal slots via Engine `UInventoryComponent` |
| `UAnimInstance` | `ULeonTournamentAnimInstance` | Reads locomotion; no TDM rules |
| `AAIController` | `ALeonTournamentBotController` | TDM enemy filter + BT asset; sight via Engine `UAIPerceptionComponent` |
| `AHUD` | `ALeonTournamentHUD` | Crosshair, scores, hit marker |
| `AWeaponBase` | `ALeonTournamentWeapon` | Arsenal, ammo, traces → damage |
| — | `FLeonTournamentArenaBuilder` | Thin wrapper: arena surface → material path → Engine `FProceduralPrimitiveSpawner` |
| — | `FLeonTournamentDamageRules` | Friendly-fire / self-damage checks |
| — | `FLeonTournamentWeaponVfx` | Muzzle/tracer/flame particle helpers |
| — | `FLeonTournamentUILayout` | Product GI/GM accessors wrapping Engine `FUILayout` |
| `FGraphicsQuality` | (menu / GI) | Engine Low/Medium/High presets; game stores `EGraphicsQuality` |
| — | `FUITypeScale` | HTML-like font tokens (`H1`/`H2`/`H3`/`P`); `FUILayout::kFs*` aliases |

`UCombatComponent` in Engine is a cooldown/attack gate. Fire/reload facade lives on `ULeonTournamentCombatComponent`; magazine and cooldown live on Engine `AWeaponBase`; traces and presets live on `ALeonTournamentWeapon`. Held fire is `FControlInput::CustomBit0`; reload stays a ServerRPC.

### LeonTournament source layout

```text
Projects/LeonTournament/Source/LeonTournament/
├── Public/{Types,UI,Game,Characters,Combat}/
└── Private/{UI,Game,Characters,Combat}/
```

Include roots are those Public subfolders (flat `#include "ALeonTournamentGameMode.hpp"`).

## Sandbox mapping

`ASandboxGameMode` subclasses `AGameMode` and uses engine `AGameState`. `StartPlay` calls `StartMatch()` so the remaining clock is in `InProgress` (Sandbox does not enforce a TDM duration).

## File naming

One public type per canonical file (`ACharacter.hpp`, `ALeonTournamentGameMode.hpp`). Aggregation exceptions: `Engine/Components.hpp` (EnTT POD registry) and `ULeonTournamentWidgets.hpp` (project UI pack).
