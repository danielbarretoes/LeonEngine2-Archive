# LeonEngine2 — Product Builds

Three separate CMake products. Artifacts live under `out/` (never in-source `build/` next to shared CMake helpers — on Windows those collide).

## Products

| Product | Configure | Output | Contents |
| :--- | :--- | :--- | :--- |
| **Engine** | `cmake -S . -B out/Engine -DLEON_PRODUCT=Engine` | `out/Engine/` | ThirdParty, `LeonEngineCore`, plugins, tools, tests |
| **Editor** | `cmake -S Editor -B out/Editor` | `out/Editor/` | Engine stack + ImGui + `LeonEditor` |
| **Project** | `cmake -S . -B out/Projects/<Name> -DLEON_PRODUCT=Project -DLEON_PROJECT_DIR=…` | `out/Projects/<Name>/` | Engine stack + **one** game (`_project/<Game>.exe`) |

Shared helpers: [`CMake/`](../CMake/) (`LeonEngineStack.cmake`, `EditorDependencies.cmake`, `SyncDirectory.cmake`, `LeonCompileOptions.cmake`).

`LEON_ENGINE_ROOT` is the repo root. When the Editor entry point nests Engine, paths resolve via that variable (not `CMAKE_SOURCE_DIR` alone).

## Scripts (preferred)

```bat
python Scripts/build_engine.py --config Debug
python Scripts/build_editor.py --config Debug
python Scripts/build_editor.py --run
python Scripts/build_project.py --project Projects/Sandbox/Sandbox.lproject --config Debug
python Scripts/run_tests.py
```

## Editor layout

| Path | Role |
| :--- | :--- |
| `Editor/Source/Public/Editor/` | NAMING-compliant public API (`FEditorApp`, `FViewportPanel`) |
| `Editor/Source/Private/` | Implementation + `main.cpp` |
| `Editor/Resources/` | Icons / brand staged beside the exe |
| `Editor/Legacy/` | Pre-NAMING ImGui editor — **not built**; port later |

Contract and roadmap: [EDITOR_ROADMAP.md](EDITOR_ROADMAP.md), [NAMING.md](NAMING.md).

## Rules

1. Engine must not reference `Projects/` or product names.
2. One Project configure builds one game (`LEON_PROJECT_DIR`).
3. Editor is never `add_subdirectory`'d from the root.
4. CI builds Engine tests under `out/Engine` and the Editor under `out/Editor` ([`.github/workflows/validate.yml`](../.github/workflows/validate.yml)).
