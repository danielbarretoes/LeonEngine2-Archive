# Refactoring Progress — LeonEngine2

> Basado en [AUDIT.md](./AUDIT.md). Commit de referencia: `5bad7f5` (pre-refactor snapshot)

---

## Estado actual: ✅ Completado (Fases 1 a 8)

| Fase | Descripción | Estado |
|------|-------------|--------|
| 1 | OpenGL 4.5 Core + Debug Context | ✅ Completado |
| 2 | FSceneRenderer — extraer pipeline de FScene | ✅ Completado |
| 3 | Limpieza de FRenderer + consolidar stats | ✅ Completado |
| 4 | Modelo de luces PBR correcto (Intensity + Radius) | ✅ Completado |
| 5 | BRDF LUT en RG16F (evitar banding) | ✅ Completado |
| 6 | Eliminar HDR Solar Spike hack en texturas | ✅ Completado |
| 7 | Correcciones de estado OpenGL (SetBlendState, ODR, NormalMatrix, SetMat3) | ✅ Completado |
| 8 | FBO tracking CPU-side | ✅ Completado |

---

## Detalle de implementaciones por fase

### Fase 1 — Modern OpenGL 4.5 Core + Debug Context
- `Engine/src/core/Window.cpp`: GLFW hints actualizados a OpenGL 4.5 core (`GLFW_CONTEXT_VERSION_MAJOR 4`, `GLFW_CONTEXT_VERSION_MINOR 5`) y debug context activado para builds de depuración.
- `Plugins/RHI/OpenGL/src/OpenGLContext.cpp`: Registrado `glDebugMessageCallback` direccionando errores, warnings y notas de rendimiento a `LE_CORE_ERROR` y `LE_CORE_WARN`.
- Shaders actualizados a `#version 450 core`:
  - `Engine/Assets/Shaders/PBR_Lit.glsl`
  - `Engine/Assets/Shaders/ShadowDepth.glsl`
  - `Engine/Assets/Shaders/Skybox.glsl`
  - `Engine/Assets/Shaders/PostProcess.glsl`
  - `Engine/Assets/Shaders/DebugLine.glsl`
  - `Engine/Assets/Shaders/DebugFont.glsl`
  - `Engine/Assets/Shaders/WorldText.glsl`

### Fase 2 — FSceneRenderer (Pipeline Orchestrator)
- Creados `Engine/include/renderer/SceneRenderer.hpp` y `Engine/src/renderer/SceneRenderer.cpp`.
- `FScene` reducido a puro contenedor de datos ECS (de ~822 líneas a ~55 líneas).
- `FSceneRenderer` asume la propiedad completa de Framebuffers, UBOs, shaders internos del pipeline, IBL y los 5 render passes:
  1. *Cascaded Shadow Pass* (CSM, 3 splits)
  2. *Spot Light Shadow Pass*
  3. *Planar Reflection Pass* (oblique near-plane clipping)
  4. *Main HDR Geometry Pass*
  5. *Post-Processing Pass* (ACES Tonemapping & Gamma Correction)

### Fase 3 — Limpieza de FRenderer & Stats
- `FRenderer` despojado de métodos no implementados / legacy (`Submit`, `BeginScene`, `EndScene`), transformándose en un tracker de estadísticas de GPU puro (`ResetStats`, `RecordDrawIndexed`, etc.).
- Comandos de dibujo e inicialización unificados en `FRenderCommand`.

### Fase 4 — Modelo de luces físicamente correcto (PBR)
- `Engine/include/renderer/Light.hpp`:
  - Eliminado el split Phong (`AmbientIntensity`, `DiffuseIntensity`, `SpecularIntensity`).
  - Cada luz tiene ahora `Color` + `Intensity` única (radiancia física).
  - Luces puntuales y spots usan `Radius` (atenuación inversa al cuadrado estilo UE4/Filament) en lugar de coeficientes empíricos `Constant/Linear/Quadratic`.
- `Engine/Assets/Shaders/PBR_Lit.glsl`: Adaptado al nuevo modelo de UBO de iluminación sin matrices ni cálculos Phong arcaicos.
- `Engine/src/scene/SceneSerializer.cpp`: Serializador actualizado para el nuevo modelo PBR (`Intensity`, `Radius`).
- `Engine/src/renderer/DebugRenderer.cpp`: Gizmos de luces actualizados para leer directamente `Radius`.

### Fase 5 — BRDF LUT en RG16F
- `Engine/include/renderer/Texture.hpp` e `IRenderDriver`: Añadidos `ETextureFormat` y `CreateWithFormat` + `SetDataFloat`.
- `Engine/src/renderer/IBLGenerator.cpp`: BRDF LUT ahora se genera en formato de coma flotante de media precisión `RG16F`, eliminando el banding de cuantización de 8 bits en rugosidades bajas.

### Fase 6 — Eliminación de hacks en texturas (Single Responsibility)
- `Plugins/RHI/OpenGL/src/OpenGLTexture2D.cpp`: Eliminado el preprocesado destructivo de desenfoque gaussiano para "solar spikes" en mapas HDR. Las texturas se cargan puras respetando el principio de responsabilidad única.
- Soporte para formatos `RG16F`, `RGBA16F`, `RGBA32F` y subida de datos float (`SetDataFloat`).

### Fase 7 — Gestión de estado OpenGL y calidad de código
- `Plugins/RHI/OpenGL/src/OpenGLRenderAPI.cpp`:
  - `SetBlendState` desacoplado: solo conmuta `GL_BLEND` sin sobreescribir la función de mezcla personalizada.
  - Función de mezcla por defecto configurada en `Init()`.
- `Engine/include/renderer/Buffer.hpp`: `ShaderDataTypeSize` cambiado de `static` a `inline` para evitar violaciones de ODR (One Definition Rule).
- `Engine/include/renderer/Shader.hpp` & `Plugins/RHI/OpenGL/src/OpenGLShader.cpp`: Añadido método `SetMat3` para matrices 3x3.
- `Engine/Assets/Shaders/PBR_Lit.glsl`: La matriz normal (`u_NormalMatrix`) se calcula una vez en CPU por entidad (`glm::transpose(glm::inverse(glm::mat3(model)))`) y se envía vía `SetMat3`, evitando el cálculo de inversión matricial por vértice en GPU.

### Fase 8 — FBO State Tracking en CPU
- `FSceneRenderer`: `m_PreviousFBO` rastreado a nivel de miembro en el renderer para restaurar el destino de framebuffer anterior tras el pase de post-procesado.

---

## Log de commits

| Commit | Fase | Descripción |
|--------|------|-------------|
| `5bad7f5` | — | Pre-refactor snapshot (AUDIT.md) |
| *(Actual)* | 1-8 | Implementación integral de las 8 fases del refactor de arquitectura gráfica |
