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
| `build_engine.py` | Configure/build Engine product → `out/Engine` (`LEON_PRODUCT=Engine`) |
| `build_editor.py` | Configure/build LeonEditor → `out/Editor` |
| `build_project.py` | Configure/build one game → `out/Projects/<Name>` (`--project`, `--config`, `--run`, `--clean`, `--rebuild`) |
| `run_project.py` | `build_project.py --run` |
| `validate_project.py` | ProjectTool `validate_project` (descriptor, config INIs, map integrity) |
| `bake_lightmaps.py` | LightmassTool bake/validate lightmaps (`--map` or DefaultMap) |
| `import_assets.py` | Import Raw → Content for `--project` via AssetTool |
| `validate_assets.py` | Validate Content for `--project` via AssetTool |
| `create_project.py` | Scaffold a blank game (`--name`, `--output`) |
| `clean_rebuild.py` | Wipe project out dir + rebuild (`--project` required) |
| `run_tests.py` | Engine `RendererTests` / `LeonTournamentTests` under `out/Engine` |
| `verify_ue_naming.py` | Naming / Engine isolation CI guard (scans `Editor/Source`, skips `Legacy`) |
| `format_code.py` | clang-format Engine (+ optional `--project` tree) |
| `build_incremental.py` | Thin alias of `build_project.py` |

## QA (Engine renderer)

| Script | Purpose |
| :--- | :--- |
| `run_shader_mutations.py` | Mutation testing of PBR shaders |
| `run_mutation_audit.py` | Mutation audit helper |

## Project Scripts (Per-Game Shortcuts)

Each project under `Projects/<Name>/Scripts/` provides fast shortcuts in Python and PowerShell:

### Sandbox (`Projects/Sandbox/Scripts/`)
- `bake_draft.py` / `BakeDraft.ps1`: Bake lightmaps with Draft quality
- `bake_production.py` / `BakeProduction.ps1`: Bake lightmaps with Production quality
- `run.py` / `Run.ps1`: Build and run Sandbox
- `build.py` / `Build.ps1`: Build Sandbox executable
- `package.py` / `Package.ps1`: Produce Shipping release ZIP
- `validate.py`: Validate project assets

### LeonTournament (`Projects/LeonTournament/Scripts/`)
- `bake_draft.py` / `BakeDraft.ps1`: Bake all arena lightmaps with Draft quality
- `bake_production.py` / `BakeProduction.ps1`: Bake all arena lightmaps with Production quality
- `run.py` / `Run.ps1`: Build and run LeonTournament
- `build.py` / `Build.ps1`: Build LeonTournament executable
- `test.py` / `Test.ps1`: Compile and run LeonTournament gameplay test suite
- `package.py` / `Package.ps1`: Produce Shipping release ZIP

### External game

```bat
set LEON_ENGINE_ROOT=C:\LeonEngine2
set LEON_PROJECT=D:\Games\MyGame\MyGame.lproject

python %LEON_ENGINE_ROOT%\Scripts\create_project.py --name MyGame --output D:\Games\MyGame
python %LEON_ENGINE_ROOT%\Scripts\build_project.py --project %LEON_PROJECT%
python %LEON_ENGINE_ROOT%\Scripts\run_project.py --project %LEON_PROJECT%
```

CMake receives an **absolute** `LEON_PROJECT_DIR` so the game target can sit outside the Engine tree. Out-of-tree projects compile into `out/Projects/<FolderName>/`.

## PowerShell

`CleanRebuild.ps1` / `FormatCode.ps1` only forward to the Python scripts above (require `--Project` / `LEON_PROJECT` for rebuild).
