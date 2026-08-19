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

## P1 — Important (next pass)

| Item | Location (approx.) | Why |
| :--- | :--- | :--- |
| Frustum / light-space cull for shadow casters | `RenderCascadedShadowPass`, `RenderSpotShadowPass` | Cost ≈ cascades × all casters today |
| Dual-key `UAssetManager` cache blocks `UnloadUnused` | `UAssetManager.cpp` | Virtual + resolved keys inflate `use_count`; travel leaks VRAM |
| Invalidate `GBoundProgram` on travel / recreate | `FOpenGLShader.cpp` | Skip-bind can reuse a recycled GL program ID |
| Alpha-mask on static / skinned shadow casters | CSM/spot static & skinned paths force `u_AlphaMode=0` | Foliage / fences cast solid shadows |
| Warn when truncating >16 point / >8 spot lights | `FWorldRenderer::Render` gather | Silent drop |
| Transparent sort by AABB center (not model origin) | `FWorldRendererGeometry.cpp` | Large pivots sort wrong |

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
