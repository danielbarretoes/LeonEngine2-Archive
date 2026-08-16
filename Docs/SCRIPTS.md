# LeonEngine2 — Engine Scripts

Source of truth for Engine CLI tooling. Scripts live next to the Engine (like Unreal’s Build/RunUAT) and target a **`.lproject` that may live outside this repository**.

## Environment

| Variable | Meaning |
| :--- | :--- |
| `LEON_ENGINE_ROOT` | Engine repo root (`Scripts/`, `Engine/`, `CMakeLists.txt`). If unset, inferred as the parent of `Scripts/`. |
| `LEON_PROJECT` | Absolute or relative path to the game `.lproject`. |

Shared helpers: [`Scripts/_leon_paths.py`](../Scripts/_leon_paths.py) (not a CLI).

## Canonical Engine CLI

| Script | Purpose |
| :--- | :--- |
| `build_project.py` | Configure/build game target (`--project`, `--config`, `--run`, `--clean`, `--rebuild`) |
| `run_project.py` | `build_project.py --run` |
| `validate_project.py` | LeonAssetTool `validate_project` |
| `bake_lightmaps.py` | LeonAssetTool bake/validate lightmaps (`--map` or DefaultMap) |
| `import_assets.py` | Import Raw → Content for `--project` |
| `validate_assets.py` | Validate Content for `--project` |
| `create_project.py` | Scaffold a blank game (`--name`, `--output`) |
| `clean_rebuild.py` | Wipe `build/` + rebuild (`--project` required) |
| `run_tests.py` | Engine `RendererTests` suite (not project-specific) |
| `verify_ue_naming.py` | Naming / Engine isolation CI guard |
| `format_code.py` | clang-format Engine (+ optional `--project` tree) |
| `build_incremental.py` | Thin alias of `build_project.py` |

## QA (Engine renderer)

| Script | Purpose |
| :--- | :--- |
| `run_shader_mutations.py` | Mutation testing of PBR shaders |
| `run_mutation_audit.py` | Mutation audit helper |

## Not Engine tooling

- **No** `*sandbox*` scripts under `Scripts/` — product shortcuts live in the game, e.g. [`Projects/Sandbox/Scripts/run.py`](../Projects/Sandbox/Scripts/run.py).
- Content one-shots (e.g. normal map generators) belong under the **project** (`Projects/Sandbox/Tools/`), not Engine Scripts.

## Examples

### Monorepo Sandbox

```bat
python Scripts/build_project.py --project Projects/Sandbox/Sandbox.lproject
python Scripts/run_project.py --project Projects/Sandbox/Sandbox.lproject
python Scripts/validate_project.py --project Projects/Sandbox/Sandbox.lproject
python Scripts/bake_lightmaps.py --project Projects/Sandbox/Sandbox.lproject --force
```

Or:

```bat
python Projects/Sandbox/Scripts/run.py
python Projects/Sandbox/Scripts/validate.py
```

### External game

```bat
set LEON_ENGINE_ROOT=C:\LeonEngine2
set LEON_PROJECT=D:\Games\MyGame\MyGame.lproject

python %LEON_ENGINE_ROOT%\Scripts\create_project.py --name MyGame --output D:\Games\MyGame
python %LEON_ENGINE_ROOT%\Scripts\build_project.py --project %LEON_PROJECT%
python %LEON_ENGINE_ROOT%\Scripts\run_project.py --project %LEON_PROJECT%
```

CMake receives an **absolute** `LEON_PROJECT_DIR` so the game target can sit outside the Engine tree. Out-of-tree projects compile into `build/Projects/<FolderName>/`.

## PowerShell

`CleanRebuild.ps1` / `FormatCode.ps1` only forward to the Python scripts above (require `--Project` / `LEON_PROJECT` for rebuild).
