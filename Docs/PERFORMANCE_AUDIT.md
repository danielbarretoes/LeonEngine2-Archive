# Performance Audit — LeonEngine2 / LeonTournament

Date: 2026-08-18  
Build: Debug (`build/`)  
Hardware: Windows, RTX 5060 Laptop, OpenGL 4.5 NVIDIA 610.62  
Scenario: LeonTournament TDM 2v2, 1280×720, `--offline-match --no-vsync --bots=2,2`

## How to reproduce

```bat
cmake --build build --target LeonTournament RendererTests --parallel
.\build\Projects\LeonTournament\LeonTournament.exe ^
  --project=Projects/LeonTournament/LeonTournament.lproject ^
  --offline-match --validate-seconds=60 --no-vsync --bots=2,2 ^
  --report=Projects/LeonTournament/Saved/perf_report.txt

.\build\Tests\RendererTests.exe --test-suite="Frame profiler"
.\build\Tests\RendererTests.exe --test-suite="Performance scaling benchmarks"
.\build\Tests\RendererTests.exe --test-suite="Jolt physics scene"
```

Reports land under `Projects/LeonTournament/Saved/perf_*.txt` (percentiles, subsystem averages, bound class, RAM/VRAM).

## Baseline (pre-remediation)

| Metric | Value |
|--------|-------|
| Report | `perf_baseline_debug_18s.txt` |
| Frame avg / FPS | 21.9 ms / ~46 FPS |
| Game / Render / GPU | 14.2 / 4.9 / 1.4 ms |
| Characters / AI | ~10.8 / ~2.0 ms |
| Bound class | CPU-bound |

Primary hot path: character movement + **CPU triangle-mesh collision queries** (not GPU/CSM).

## Remediations applied

1. **AI perception** — fewer LOS traces; staggered updates.
2. **Animation** — reuse `ComponentSpaceTransforms` (less per-frame alloc).
3. **WorldStatic** — disable tick on static primitives when appropriate.
4. **Overlaps** — `AsPrimitiveComponent()` instead of `dynamic_cast`.
5. **Jolt owns gameplay queries** — CastRay / CastShape / CollideShape; shared MeshShape BVH for static meshes; channel + broadphase filters; one hit per body; `OptimizeBroadPhase` after BeginPlay; larger pools; single-threaded job system.
6. **Ragdoll stability** — SwingTwist/Hinge build an orthonormal twist/plane frame (humanoid assets use Axis=Y, which collided with Jolt’s default PlaneAxis=Y and produced NaNs → Debug abort ~5s into combat). GroupFilter table enlarged; `IgnoreCollision` skips identical subgroup IDs.

## After (stable)

| Metric | 18s (`perf_after_ragdoll_fix_18s.txt`) | 60s (`perf_after_jolt_debug_60s.txt`) |
|--------|------------------------------------------|----------------------------------------|
| Frame avg / FPS | 8.4 ms / ~119 FPS | 7.9 ms / ~126 FPS |
| p95 / p99 frame | 9.2 / 9.9 ms | 9.1 / 9.9 ms |
| Spikes | 0 | 0 |
| Game / Render / GPU | 4.3 / 3.7 / 0.9 ms | 3.8 / 3.7 / 1.1 ms |
| Characters / Overlaps / Physics / AI | 1.3 / 2.2 / 0.30 / 0.22 ms | 1.3 / 1.7 / 0.34 / 0.17 ms |
| Bound class | CPU-bound | CPU-bound |

Approx. **2.6×** frame-time reduction vs baseline (21.9 → 7.9 ms) on the same Debug TDM scenario. Combat deaths/respawns complete without abort.

## Remaining cost (P1+)

| Area | ~avg (60s) | Notes |
|------|------------|--------|
| Overlaps | 1.7 ms | Still the largest game-side slice after movement |
| Opaque + planar | 1.7 + 0.9 ms | Render CPU; GPU remains ~1 ms |
| Characters | 1.3 ms | Movement/queries much healthier than baseline |
| Animation | 1.0 ms | Acceptable for 5 skinned pawns in Debug |
| Shadows | 0.86 ms | Light-frustum caster cull is in (P1); still not the primary TDM bottleneck |

GPU remains ~1 ms on the measured TDM scene. See [RENDERER_REMEDIATION.md](RENDERER_REMEDIATION.md).

## Validation checklist

- [x] Jolt suite 9/9
- [x] Frame profiler suite
- [x] Performance scaling benchmarks
- [x] PhysicsAsset / Ragdoll suite
- [x] Offline match 18s + 60s exit 0, report written, deaths during run
- [ ] Release RelWithDebInfo/Release re-measure (optional; Debug was the audit target)

## Instrumentation reference

- `FFrameProfiler` / `FFrameStatsCollector` — percentiles, histogram, bound class, memory, GPU slots
- `FPerformanceTimer` / `FScopedTimer`
- OpenGL GPU timer queries in `FOpenGLRenderAPI`
- CLI: `--offline-match` / `--benchmark`, `--validate-seconds`, `--report`, `--bots=`, `--no-vsync`
