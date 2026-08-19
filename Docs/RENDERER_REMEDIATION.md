# Renderer Remediation Plan

Follow-up from the static / architectural rendering audit. Scope: solo/indie OpenGL engine — **not** Unreal feature parity.

Index: [RENDERER.md](RENDERER.md). Contract: [RENDERER_CONTRACT.md](RENDERER_CONTRACT.md).

## Priority legend

| Priority | Meaning |
| :--- | :--- |
| **P0** | Correctness / lifetime / state — must fix for production robustness |
| **P1** | Important correctness or scalability |
| **P2** | Meaningful improvement |
| **P3** | Future / optional |

---

## P0 — Done (this change set)

| Item | Status | Notes |
| :--- | :---: | :--- |
| Honor `FStaticMeshComponent.bVisible` in opaque, transparent, CSM, and spot paths | Done | Also early-out in `IsStaticMeshCulled` |
| Tear down world / static GPU caches before destroying the GL context | Done | `UEngine::Run` clears world; `FIBLGenerator` / `FMeshPrimitives` release; `FRenderCommand::Shutdown` via `FRenderer::Shutdown` |
| Explicit raster reset at CSM/spot/HDR; restore blend/cull after UI/debug/sky | Done | `ResetDefaultMeshRasterState()` |

---

## P1 — Done

| Item | Status | Notes |
| :--- | :---: | :--- |
| Frustum / light-space cull for shadow casters | Done | AABB vs cascade / spot VP in `FWorldRendererLighting.cpp` |
| Dual-key `UAssetManager` cache blocks `UnloadUnused` | Done | Drop when `use_count() <=` alias count in the same map |
| Invalidate `GBoundProgram` on travel | Done | `FRenderCommand::InvalidateShaderBindingCache` after `UnloadUnused` |
| Alpha-mask on static / skinned shadow casters | Done | `BindShadowCasterAlpha` for Mask; Blend writes opaque depth |
| Warn when truncating >16 point / >8 spot lights | Done | Per-frame `LE_CORE_WARN` in `FWorldRenderer::Render` |
| Transparent sort by AABB center | Done | `TransparentSortDistanceSq` |

---

## P2 — Improvement

| Item | Notes |
| :--- | :--- |
| Align PNG runtime color-space heuristics with `.ltex` / `_nmap` | Prefer `.ltex`; fix `IsLinearDataTexturePath` tags |
| Do not cache failed shaders (`RendererID == 0`) | `UAssetManager::GetShader` |
| Tests: GL state after UI/debug; static `bVisible`; travel VRAM | Extend `Tests/Renderer` / GPU suites |
| Front-to-back opaque sort | Only if GPU-bound (current TDM is CPU-bound) |

---

## P3 — Future

| Item | Classification |
| :--- | :--- |
| Shader hot reload | USEFUL |
| Additional shadowed spots / atlas | OPTIONAL — only if the game needs it |
| Opaque FBO handle type (vs `uint32_t`) | OPTIONAL second-backend prep |
| Z-prepass / reverse-Z | NOT NEEDED at meter-scale scenes |
| Ray tracing, Nanite, VSM, bindless, GPU-driven | NOT NEEDED |

---

## Explicit non-goals

Do not treat missing Unreal features as defects. Do not add a render thread, full RHI enterprise layer, or render graph unless a concrete product requirement appears.
