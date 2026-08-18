# LeonEngine2 — Gameplay Framework

Unreal-aligned responsibilities. There is one canonical type per concept; no compatibility aliases.

## Responsibility matrix

| Type | Owns | Must not own |
| :--- | :--- | :--- |
| `UGameInstance` | Application/session lifetime, travel URL, net mode, project services that survive map changes | Match score, kills, current pawn, match timer, combat state |
| `AGameModeBase` | Authority rules, login, `RestartPlayer`, spawn/respawn, default class selection, match start/end | HUD widgets, camera, input, animation, replicated per-player stats |
| `AGameStateBase` | Replicated world/match facts (phase, timer, team scores, winner) | Input, camera, weapon impl, local UI, spawn algorithms |
| `APlayerState` | Persistent per-player identity (name, id, team, score, kills/deaths) across pawn replacement | Movement, mesh, weapons |
| `APlayerController` | Input, possession, camera, local HUD interaction, commands to authority | Health, score, weapon implementation, match rules |
| `APawn` / `ACharacter` | Capsule, movement, mesh, jump/fall/land, camera attach | Rifle/ammo, TDM, team assignment, kill scoring |
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
  → Match start (GameMode writes rules; GameState exposes phase)
  → Gameplay
  → Death (Health on authority) → GameMode.NotifyDeath writes PlayerState/GameState + respawn timer
  → RestartPlayer: destroy pawn, spawn new pawn, Possess (same Controller + PlayerState)
  → Match end (GameMode) → GameState winner/scores
```

`AGameModeBase::RestartPlayer` is the single respawn path. If `DefaultPawnClass` is empty or `"None"`, the base implementation UnPossesses and destroys the old pawn without spawning. Game subclasses that keep `DefaultPawnClass = "None"` (menu-first) override `RestartPlayer` to spawn their character.

`AGameStateBase` / game `GameState` mutators and `PlayerState` score setters no-op unless `AActor::IsNetworkAuthority()` (not `ENetMode::Client`). Replication writes fields in `DeserializeReplication` and does not go through those setters.

`UHealthComponent::ApplyDamage` / `Heal` run only on network authority. `EndPlay` clears `OnDeath` / `OnDamage` delegates.

`APawn::GetPlayerState()` prefers the possessing Controller, then the PlayerState cached at `PossessedBy`.

Only `AGameModeBase` (and game subclasses) decides when a match starts or ends. `UGameInstance` may request travel; it does not increment kills.

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

Engine generic fallbacks: `AGameModeBase`, `AGameStateBase`, `APlayerController`, `APlayerState`, `ADefaultPawn`, `AHUD`.

## LeonTournament mapping

| Engine base | Project class | Role |
| :--- | :--- | :--- |
| `UGameInstance` | `ULeonTournamentGameInstance` | Session mode, LAN host/join, auto-offline match CLI |
| `AGameModeBase` | `ALeonTournamentGameMode` | TDM rules, team assign, spawn, kill limit, duration |
| `AGameStateBase` | `ALeonTournamentGameState` | TeamScores, MatchState, MatchTime, Winner |
| `APlayerState` | `ALeonTournamentPlayerState` | Team, kills, deaths |
| `APlayerController` | `ALeonTournamentPlayerController` | Look/fire commands, local HUD |
| `ACharacter` | `ALeonTournamentCharacter` | First-person shooter pawn; arsenal slots via Engine `UInventoryComponent` |
| `UAnimInstance` | `ULeonTournamentAnimInstance` | Reads locomotion; no TDM rules |
| `AAIController` | `ALeonTournamentBotController` | TDM enemy filter + BT asset; sight via Engine `UAIPerceptionComponent` |
| `AHUD` | `ALeonTournamentHUD` | Crosshair, scores, hit marker |
| `AWeaponBase` | `ALeonTournamentWeapon` | Arsenal, ammo, traces → damage |
| — | `FLeonTournamentArenaBuilder` | Shared procedural arena/lab box spawn |
| — | `FLeonTournamentDamageRules` | Friendly-fire / self-damage checks |
| — | `FLeonTournamentWeaponVfx` | Muzzle/tracer/flame particle helpers |
| — | `FLeonTournamentUILayout` | Product GI/GM accessors wrapping Engine `FUILayout` |

`UCombatComponent` in Engine is a cooldown/attack gate. Fire/reload facade lives on `ULeonTournamentCombatComponent`; magazine and cooldown live on Engine `AWeaponBase`; traces and presets live on `ALeonTournamentWeapon`.

### LeonTournament source layout

```text
Projects/LeonTournament/Source/LeonTournament/
├── Public/{Types,UI,Game,Characters,Combat}/
└── Private/{UI,Game,Characters,Combat}/
```

Include roots are those Public subfolders (flat `#include "ALeonTournamentGameMode.hpp"`).

## File naming

One public type per canonical file (`ACharacter.hpp`, `ALeonTournamentGameMode.hpp`). Aggregation exceptions: `Engine/Components.hpp` (EnTT POD registry) and `ULeonTournamentWidgets.hpp` (project UI pack).
