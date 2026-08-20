# LeonEngine2 — Testing Architecture

## Binaries

| CMake target | Role | Links LeonTournament? |
| :--- | :--- | :--- |
| `RendererTests` | Engine unit/integration/GPU suite (historical name kept for mutation scripts and CI) | No |
| `LeonTournamentTests` | Product gameplay, combat, AI, UI, match, Mixamo import | Yes |

`python Scripts/run_tests.py` builds and runs **both**. Working directory is the repository root.

CTest names: `EngineTests` → `RendererTests`, `LeonTournamentTests` → `LeonTournamentTests`.

## Layout

```text
Tests/
  Main.cpp                 Shared doctest entry
  Math/ IBL/ PBR/ HDR/ Cache/ GPU/ Shader/ Asset/ Lightmass/ Renderer/
  Gameplay/                Engine gameplay framework (not the product)
  LeonTournament/          Product tests only
```

Engine tests must not include `Projects/LeonTournament` headers or compile game `.cpp` files.

## Skip policy

- **Zero skipped tests** in the default suite on a machine with OpenGL 4.5.
- This vendored doctest has no runtime `SKIP()` macro. GPU tests call `REQUIRE(gl.IsValid())`.
- If a headless OpenGL 4.5 context cannot be created, those tests **fail** instead of silently returning (which previously counted as PASS).
- That failure is the only honest result without a skip API. Do not reintroduce `MESSAGE` + `return`.
- Missing fixtures `REQUIRE` the file. Obsolete fixtures are deleted or retargeted (`DaySky1k.lhdr` replaced the missing `AutumnField1k.lhdr`).

`Scripts/run_tests.py` treats `skipped > 0` as failure.

## GPU / renderer tests

`Tests/GPU/HeadlessGLContext.hpp` creates a hidden GLFW 4.5 core context. Shader suites under `Tests/Shader/` compile production GLSL from `Engine/Assets/Shaders/` (PBR, shadows, post-process). CPU math / IBL / cache suites live under `Tests/Math`, `Tests/IBL`, `Tests/PBR`, `Tests/Cache`, `Tests/HDR`. Mutation scripts still invoke `out/Engine/_leon_tests/RendererTests.exe` (or the path resolved by `Scripts/run_tests.py`).

Renderer contracts: [RENDERER.md](RENDERER.md). Known coverage gaps (see [RENDERER_REMEDIATION.md](RENDERER_REMEDIATION.md) P2):

- GL state after UI / debug overlays
- `FStaticMeshComponent.bVisible` (code fixed; add regression test)

## Behavioral bar

Tests must exercise behavior (movement, interpolation, spawn of the configured pawn, INI class resolution), not only “pointer is non-null”. Existence-only asserts are not added as new coverage.
