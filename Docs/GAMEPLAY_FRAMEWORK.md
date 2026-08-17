# LeonEngine2 — Gameplay Framework

Unreal-aligned responsibilities. There is one canonical type per concept; no compatibility aliases.

## Responsibility matrix

| Type | Owns | Must not own |
| :--- | :--- | :--- |
| `UGameInstance` | Application/session lifetime, travel URL, net mode, project services that survive map changes | Match score, kills, current pawn, match timer, combat state |
| `AGameModeBase` | Authority rules, login, spawn/respawn, default class selection, match start/end | HUD widgets, camera, input, animation, replicated per-player stats |
| `AGameStateBase` | Replicated world/match facts (phase, timer, team scores, winner) | Input, camera, weapon impl, local UI, spawn algorithms |
| `APlayerState` | Persistent per-player identity (name, id, team, score, kills/deaths) across pawn replacement | Movement, mesh, weapons |
| `APlayerController` | Input, possession, camera, local HUD interaction, commands to authority | Health, score, weapon implementation, match rules |
| `APawn` / `ACharacter` | Capsule, movement, mesh, jump/fall/land, camera attach | Rifle/ammo, TDM, team assignment, kill scoring |
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
  → Death (Character/Health) → GameMode respawn timer → new Pawn, same PlayerState
  → Match end (GameMode) → GameState winner/scores
```

Only `AGameModeBase` (and game subclasses) decides when a match starts or ends. `UGameInstance` may request travel; it does not increment kills.

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
| `ACharacter` | `ALeonTournamentCharacter` | First-person shooter pawn |
| `UAnimInstance` | `ULeonTournamentAnimInstance` | Reads locomotion; no TDM rules |
| `AAIController` | `ALeonTournamentBotController` | TDM enemy selection + BT asset |
| `AHUD` | `ALeonTournamentHUD` | Crosshair, scores, hit marker |
| — | `ALeonTournamentWeapon` | Rifle, ammo, traces → damage |

`UCombatComponent` in Engine is a cooldown/attack gate. Rifle fire, magazine, and tracers live on `ALeonTournamentWeapon` / `ULeonTournamentCombatComponent`.

## File naming

One public type per canonical file (`ACharacter.hpp`, `ALeonTournamentGameMode.hpp`). Aggregation exceptions: `Engine/Components.hpp` (EnTT POD registry) and `ULeonTournamentWidgets.hpp` (project UI pack).
