# LeonTournament — Unreal Tournament MVP Plan

Roadmap to reach a credible UT-style MVP. Naming follows [NAMING.md](NAMING.md): `A*` actors, `F*` structs, `E*` enums, `U*` components, English identifiers.

---

## Current baseline (~75%)

| Area | Status |
| :--- | :--- |
| TDM / FFA | Playable with bots, score limit, timer |
| Combat | 5 weapons, headshots, assists, kill feed |
| Movement | Double jump, dodge, jump pads |
| UI | Menu → lobby → HUD → scoreboard → match end |
| LAN | Listen-server snapshots + client prediction + hitscan lag comp |
| Engine | Gameplay framework, AI, asset pipeline, particles |

---

## Phase 1 — UT feel ✅

**Goal:** Match flow and polish that reads as UT without new game modes.

| Task | Status |
| :--- | :--- |
| Replace broken `OrbitalPrism` with `TournamentArena` | Done |
| Warmup: fire/move during `Starting`, no score/kill feed | Done |
| Rematch from match-end screen | Done |
| Per-weapon fire/reload audio paths | Done |
| `SFX_Dodge`, wire `SFX_Announce` | Done |
| Default warmup 5s | Done |

---

## Phase 2 — Content ✅

**Goal:** Two authored arenas and distinct weapon identity.

| Task | Status |
| :--- | :--- |
| Ship `TournamentArena.lmap` in lobby flow | Done — Arena travels to `/Game/Maps/TournamentArena` |
| Second map `TournamentArenaNight` | Done (existing) |
| Weapon mesh silhouettes per id | Done — `FLeonTournamentWeaponVisual` |
| Dedicated SFX per weapon (WAV files) | Done — placeholder copies; swap for real assets |
| Weapon FP meshes / menu music | Done — FP presets + `BGM_Menu` loop |

---

## Phase 3 — Multiplayer LAN ✅

**Goal:** 2–4 player LAN feels fair.

| Task | Owner | Status |
| :--- | :--- | :--- |
| Client movement prediction | Engine | Done — AutonomousProxy skips snapshot teleport; `SmoothClientPosition` |
| Hitscan lag compensation | Engine + LeonTournament | Done — pose history + RTT/2 rewind in `ApplyHitscanDamage` |
| SimulatedProxy interpolation | Engine | Done — `SmoothSimulatedProxy` |
| Per-connection ping for lag comp | Engine | Done — `UNetConnection::PingMs`, `GetPeerPingMs` |
| Replicate projectiles/pickups reliably | Engine | Existing snapshot path (unchanged) |
| Dedicated headless server | Engine | Optional; listen-server OK for MVP |

---

## Phase 4 — UT game modes ✅

**Goal:** One objective mode beyond TDM/FFA.

| Task | Status |
| :--- | :--- |
| CTF flag actor | Done — `ALeonTournamentFlag` |
| CTF flag base | Done — `ALeonTournamentFlagBase` |
| CTF game mode | Done — `ALeonTournamentCaptureTheFlagGameMode` + lobby `CaptureTheFlag` |
| Flag carry state | Done — `FLeonTournamentFlagState`, `ELeonTournamentFlagStatus` |
| Bot CTF behaviors | Done — `TickCtfObjective`, BT `CtfMove` |
| Domination / Instagib | Post-MVP |

---

## Phase 5 — Engine polish (cross-project) ✅

| Task | Type | Status |
| :--- | :--- | :--- |
| Lightweight gameplay tags | `FGameplayTag`, `FGameplayTagContainer` in Engine/Gameplay | Done |
| Spectator mode post-death | `APlayerController` + LeonTournament HUD | Done |
| `PrintString` font scale → `FUITypeScale::P` | `FOnScreenDebugMessage.cpp` | Done |

---

## Out of scope (v1.1+)

- Full GAS / ability system
- Matchmaking / NAT
- Anti-cheat
- Wall-run / translocator
- Mod SDK

---

## Test checklist

- Warmup: damage applies, kills/deaths/score unchanged until `Playing`
- Rematch: `Finished` → `Starting` → `Playing` without travel
- Map cycle: Arena, NightArena, TournamentArena (no missing map path)
- Weapon audio: each id uses distinct path/volume from `FLeonTournamentWeaponAudio`
