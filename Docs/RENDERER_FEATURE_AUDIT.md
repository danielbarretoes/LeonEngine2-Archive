# AUDITORÍA DE CARACTERÍSTICAS Y ARQUITECTURA DEL RENDERER
# LeonEngine2 — OpenGL 4.5 Core Physical Rendering Pipeline

> **Documento maestro de auditoría forense, inventario de capacidades reales y hoja de ruta arquitectónica para LeonEngine2.**

---

## 1. Resumen Ejecutivo

LeonEngine2 cuenta actualmente con una **base de renderizado rasterizado físicamente correcto (OpenGL 4.5 Core + DSA)** completamente funcional y validada en su escena `MainShowcase`:

* **PBR Cook-Torrance (GGX + Smith + Schlick)** con conservación de energía estricta.
* **Image-Based Lighting (IBL v4)**: BRDF LUT 2D en disco ($1.4\text{ ms}$), Caché binaria `.libl` ($11.5\text{ ms}$), Muestreo Quasi-Monte Carlo ponderado por coseno e integración con filtrado por ángulo sólido de la textura HDR original ($1024 \times 512$).
* **Sombras Dinámicas en Tiempo Real**: Cascaded Shadow Maps (CSM de 4 cascadas sobre `Texture2DArray` con filtrado PCF Poisson) y Spot Shadows con filtrado de penumbra.
* **Reflejos Planares en Tiempo Real**: Paso offscreen con cámara reflejada, perturbación de UVs por normales y composición Fresnel.
* **Sistema de Materiales de Primer Orden**: `FMaterial`, `FMaterialInstance`, `FMaterialComponent` y serialización de assets `.lmat`.
* **Subred RHI OpenGL 4.5 con Direct State Access (DSA)** y caché de estado en CPU (`FOpenGLRenderAPI`).

Sin embargo, el pipeline actual es un **Forward Renderer mono-hilo con envío de comandos objeto a objeto (`1 draw call / mesh`)**, sin *Frustum Culling*, sin *Instancing/Batching*, sin efectos avanzados de post-procesado (Bloom, Anti-Aliasing) y con un límite estático en UBOs de 16 luces puntuales y 4 luces spot.

---

## 2. Inventario Completo de Capacidades del Renderer

### Estados de Clasificación:
1. **COMPLETAMENTE IMPLEMENTADO Y VALIDADO**: Implementación completa, libre de bugs y comprobada en runtime.
2. **IMPLEMENTADO PERO NECESITA VALIDACIÓN**: Implementado en código pero sin pruebas de estrés o cobertura total.
3. **PARCIALMENTE IMPLEMENTADO**: Estructuras o APIs presentes pero incompletas en el shader o pipeline.
4. **NO IMPLEMENTADO**: No existe código funcional en el motor.

---

### A. Geometría y Buffers

| Feature | Estado | Evidencia en Código | Archivo | Nivel | Dependencia |
| :--- | :---: | :--- | :--- | :---: | :--- |
| **Vertex & Index Buffers (DSA)** | **1** | `glCreateBuffers`, `glNamedBufferData`, `glNamedBufferSubData` | [`OpenGLBuffer.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Plugins/RHI/OpenGL/src/OpenGLBuffer.cpp) | L1 | Core RHI |
| **Vertex Array Objects (VAO DSA)** | **1** | `glCreateVertexArrays`, `glEnableVertexArrayAttrib`, `glVertexArrayAttribFormat` | [`OpenGLVertexArray.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Plugins/RHI/OpenGL/src/OpenGLVertexArray.cpp) | L1 | Buffers |
| **Primitiva Esfera** | **1** | Generación matemática UV, normales radiales, tangentes ortonormales | [`MeshPrimitives.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/MeshPrimitives.cpp#L107) | L1 | VAO |
| **Primitiva Cubo** | **1** | 24 vértices únicos, normales planas exactas por cara, tangentes | [`MeshPrimitives.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/MeshPrimitives.cpp#L15) | L1 | VAO |
| **Primitiva Cilindro** | **1** | Winding CCW corregido, paredes continuas, tapas superior e inferior | [`MeshPrimitives.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/MeshPrimitives.cpp#L205) | L1 | VAO |
| **Primitiva Plano Subdividido** | **1** | Malla en cuadrícula $X \times Z$ con normales $+Y$ y coordenadas UV | [`MeshPrimitives.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/MeshPrimitives.cpp#L320) | L1 | VAO |
| **Primitiva Pirámide y Rampa** | **1** | Geometría indexada con normales por vértice y cálculo de tangentes | [`MeshPrimitives.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/MeshPrimitives.cpp#L440) | L1 | VAO |
| **Back-Face Culling (Caché CPU)** | **1** | `glEnable(GL_CULL_FACE)`, `glCullFace(GL_BACK)` con caché de estado | [`OpenGLRenderAPI.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Plugins/RHI/OpenGL/src/OpenGLRenderAPI.cpp#L84) | L1 | Core RHI |
| **Depth Testing & Depth Mask** | **1** | `glEnable(GL_DEPTH_TEST)`, `glDepthFunc`, `glDepthMask` con estado cacheado | [`OpenGLRenderAPI.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Plugins/RHI/OpenGL/src/OpenGLRenderAPI.cpp#L50) | L1 | Core RHI |

---

### B. Materiales y Texturas

| Feature | Estado | Evidencia en Código | Archivo | Nivel | Dependencia |
| :--- | :---: | :--- | :--- | :---: | :--- |
| **Albedo (Color & Textura)** | **1** | Uniform `u_AlbedoColor`, Sampler `u_AlbedoMap` (Slot 0), decodificación gamma | [`PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl#L295) | L2 | Shaders |
| **Metallic & Roughness Maps** | **1** | Slots 2 y 4, canal rojo, clamping de seguridad $\ge 0.04$ | [`PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl#L310) | L2 | Shaders |
| **Ambient Occlusion (Map & Scalar)**| **1** | Slot 3 (`u_AOMap`), modulación de radiancia difusa y especular indirecta | [`PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl#L322) | L2 | Shaders |
| **Normal Mapping (TBN Ortonormal)** | **1** | Slot 1 (`u_NormalMap`), base de Gram-Schmidt, decodificación $[-1, 1]$ | [`PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl#L302) | L2 | Shaders |
| **Emissive Radiance** | **1** | Slot 9 (`u_EmissiveMap`), color y multiplicador de intensidad HDR | [`PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl#L481) | L2 | Shaders |
| **Material Instances (`FMaterialInstance`)**| **1** | Parámetros escalares, texturas, flags y Pipeline State Object (`FPipelineState`) | [`MaterialInstance.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/MaterialInstance.cpp) | L2 | Material Core |
| **Generación de Mipmaps 2D (DSA)** | **1** | `glGenerateTextureMipmap(m_RendererID)`, filtro `GL_LINEAR_MIPMAP_LINEAR` | [`OpenGLTexture2D.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Plugins/RHI/OpenGL/src/OpenGLTexture2D.cpp#L165) | L2 | Texture RHI |
| **Anisotropic Filtering** | **4** | No se configura `GL_TEXTURE_MAX_ANISOTROPY` en ningún sampler 2D | [`OpenGLTexture2D.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Plugins/RHI/OpenGL/src/OpenGLTexture2D.cpp) | L2 | Texture RHI |
| **Hardware sRGB Texture Decode** | **3** | Se utiliza `GL_RGBA8` en GPU y `pow(x, 2.2)` en fragment shader | [`OpenGLTexture2D.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Plugins/RHI/OpenGL/src/OpenGLTexture2D.cpp#L133) | L2 | Texture RHI |

---

### C. Modelo de Iluminación Físico (PBR Cook-Torrance)

| Feature | Estado | Evidencia en Código | Archivo | Nivel | Dependencia |
| :--- | :---: | :--- | :--- | :---: | :--- |
| **NDF (GGX / Trowbridge-Reitz)** | **1** | $D(h) = \frac{\alpha^2}{\pi ((n \cdot h)^2 (\alpha^2 - 1) + 1)^2}$ | [`PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl#L148) | L3 | Shaders |
| **Geometry Shadowing (Smith GGX)** | **1** | $G(n, v, l) = G_1(v) \cdot G_1(l)$, Schlick-GGX para directas e IBL | [`PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl#L162) | L3 | Shaders |
| **Fresnel (Schlick & Schlick-Roughness)**| **1** | $F(\theta) = F_0 + (1 - F_0)(1 - \cos\theta)^5$, modulación por rugosidad | [`PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl#L156) | L3 | Shaders |
| **Conservación de Energía ($k_D + k_S \le 1$)**| **1** | $k_S = F$, $k_D = (1 - k_S)(1 - \text{metallic})$ estricto | [`PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl#L354) | L3 | Shaders |
| **Flujo Dieléctrico vs Metálico** | **1** | $F_0 = \text{mix}(0.04, \text{albedo}, \text{metallic})$, respuesta física continua | [`PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl#L332) | L3 | Shaders |

---

### D. Luces y Sombras

| Feature | Estado | Evidencia en Código | Archivo | Nivel | Dependencia |
| :--- | :---: | :--- | :--- | :---: | :--- |
| **Directional Sunlight (PBR)** | **1** | Radiance $= \text{Color} \times \text{Intensity}$ física única, sin split Phong | [`PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl#L343) | L4 | UBO Lighting |
| **Point Lights (UE4 Radius Falloff)**| **1** | Atenuación física inversa cuadrática con radio y ventana suave | [`PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl#L374) | L4 | UBO Lighting |
| **Spot Lights (Cutoff + Attenuation)**| **1** | Conos interior/exterior suaves con atenuación de radio | [`PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl#L407) | L4 | UBO Lighting |
| **Cascaded Shadow Maps (CSM)** | **1** | 4 divisiones de cascada, `Texture2DArray`, cálculo de frustum ajustado | [`SceneRenderer.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/SceneRenderer.cpp#L293) | L4 | FBO Depth |
| **Spot Shadows (Depth 2D)** | **1** | Matriz de proyección perspectiva de spot, depth buffer $1024 \times 1024$ | [`SceneRenderer.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/SceneRenderer.cpp#L450) | L4 | FBO Depth |
| **Shadow Filtering (PCF 16-tap Poisson)**| **1** | `sampler2DArrayShadow` y `sampler2DShadow` con sesgo adaptativo al ángulo | [`PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl#L180) | L4 | Shaders |
| **Point Light Shadows (Omni Cubemap)**| **4** | No existe paso de shadow cubemap para luces puntuales | [`SceneRenderer.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/SceneRenderer.cpp) | L4 | FBO Cubemap |
| **Planar Reflections en Tiempo Real** | **1** | Cámara reflejada, plano de recorte oblicuo, FBO $1280 \times 720$ | [`SceneRenderer.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/SceneRenderer.cpp#L500) | L4 | FBO Color |

---

### E. Image-Based Lighting & Atmósfera

| Feature | Estado | Evidencia en Código | Archivo | Nivel | Dependencia |
| :--- | :---: | :--- | :--- | :---: | :--- |
| **2D BRDF LUT Precocinada** | **1** | $256 \times 256$ `RG16F`, integración Split-Sum, carga en $1.4\text{ ms}$ | [`IBLGenerator.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/IBLGenerator.cpp#L85) | L5 | Texture2D |
| **Diffuse Irradiance Cubemap (v4)** | **1** | Muestreo Quasi-Monte Carlo ponderado por coseno + filtrado Mip en CPU | [`IBLGenerator.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/IBLGenerator.cpp#L542) | L5 | TextureCube |
| **Specular Prefiltered Cubemap (v4)**| **1** | Muestreo GGX con ángulo sólido fuente Karis + sesgo $+1.0$ | [`IBLGenerator.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/IBLGenerator.cpp#L592) | L5 | TextureCube |
| **Caché Binaria IBL (`.libl` v4)** | **1** | Serialización binaria directa, hash FNV-1a, carga en $\approx 11\text{ ms}$ | [`IBLGenerator.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/IBLGenerator.cpp#L375) | L5 | File I/O |
| **Atmospheric Skybox Procedural** | **1** | Dispersión Rayleigh/Mie analítica con disco solar direccional | [`Skybox.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/Skybox.glsl) | L5 | Shaders |

---

### F. Post-Procesado

| Feature | Estado | Evidencia en Código | Archivo | Nivel | Dependencia |
| :--- | :---: | :--- | :--- | :---: | :--- |
| **HDR Framebuffer (`GL_RGBA16F`)** | **1** | FBO flotante con color `GL_RGBA16F` y profundidad `GL_DEPTH24_STENCIL8` | [`SceneRenderer.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/SceneRenderer.cpp#L45) | L6 | FBO |
| **Tone Mapping (ACES Filmic)** | **1** | Curva analítica $f(x) = \frac{x(ax+b)}{x(cx+d)+e}$ en espacio lineal | [`PostProcess.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PostProcess.glsl#L15) | L6 | Shaders |
| **Gamma Correction (sRGB $1/2.2$)** | **1** | Conversión lineal a sRGB en paso final de post-proceso | [`PostProcess.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PostProcess.glsl#L30) | L6 | Shaders |
| **Control de Exposición Dinámica** | **1** | Uniform `u_Exposure` aplicado en el shader de post-proceso | [`PostProcess.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PostProcess.glsl#L22) | L6 | Shaders |
| **Bloom (Pirámide Down/Up)** | **4** | No existe paso de threshold ni blur piramidal | [`SceneRenderer.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/SceneRenderer.cpp) | L6 | Multi-FBO |
| **Anti-Aliasing (FXAA / TAA / MSAA)**| **4** | El FBO de escena no tiene MSAA ni existe shader de FXAA | [`SceneRenderer.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/SceneRenderer.cpp) | L6 | Shaders |
| **Depth of Field (DoF)** | **4** | No implementado | - | L6 | Depth Buffer |
| **Motion Blur** | **4** | No implementado (requiere buffer de velocidad $2\text{D}$) | - | L6 | Velocity Buffer |
| **Color Grading (Color LUT 3D)** | **4** | No implementado | - | L6 | Texture3D |

---

### G. Arquitectura de Renderizado y Escalabilidad

| Feature | Estado | Evidencia en Código | Archivo | Nivel | Dependencia |
| :--- | :---: | :--- | :--- | :---: | :--- |
| **3D In-World Text Rendering** | **1** | Batching dinámico en espacio de mundo, atlas TTF de alta resolución | [`TextRenderer.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/TextRenderer.cpp) | L7 | Batching |
| **HUD de Diagnóstico en Tiempo Real**| **1** | FPS, Frametime CPU/GPU, VRAM dedicada/usada, Tris, Draw Calls | [`DebugOverlay.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/DebugOverlay.cpp) | L7 | TextRenderer |
| **Gizmos 3D de Depuración** | **1** | Conos de luz spot, esferas de radio de luz puntual, vector del sol | [`DebugRenderer.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/DebugRenderer.cpp) | L7 | Lines |
| **Vistas de Depuración Interactivas** | **1** | Atajos `Shift + F1..F12, N, R` para aislar cualquier término o textura | [`SandboxApp.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Projects/Sandbox/src/SandboxApp.cpp#L185) | L7 | SceneRenderer |
| **Frustum Culling (CPU AABB / Sphere)**| **4** | Cada entidad en la vista ECS se dibuja incondicionalmente | [`SceneRenderer.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/SceneRenderer.cpp#L590) | L7 | Math/Bounding |
| **Instancing Dinámico (GPU)** | **4** | No se utiliza `glDrawElementsInstanced` ni buffers de instancia | [`SceneRenderer.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/SceneRenderer.cpp#L650) | L7 | SSBO / VBO |
| **Batching Geométrico Estático/Dinámico**| **4** | Sin combinación de geometrías contiguas con mismo material | [`SceneRenderer.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/SceneRenderer.cpp) | L7 | Mesh Combine |
| **Colas de Render y Ordenación (Sorting)**| **4** | Las entidades se dibujan en orden aleatorio de la vista ECS | [`SceneRenderer.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/SceneRenderer.cpp#L590) | L7 | Render Queue |
| **Transparencias y Mezcla Alfa** | **3** | `FPipelineState` tiene flags de `bBlend` y `Src/DstBlend`, pero no hay pase separado | [`MaterialInstance.hpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/include/renderer/MaterialInstance.hpp) | L7 | Sorting |
| **Indirect Rendering (`MultiDrawIndirect`)**| **4** | No implementado | - | L8 | SSBO / Indirect |
| **Forward+ / Clustered Light Culling**| **4** | Límite fijo de 16 Point / 4 Spot lights evaluadas en todos los fragmentos | [`PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl#L363) | L8 | Compute Shader |
| **RenderGraph / FrameGraph Dinámico**| **3** | Secuencia lineal fija de 6 pases estáticos en `SceneRenderer::Render` | [`SceneRenderer.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/SceneRenderer.cpp#L235) | L8 | Architecture |

---

## 3. Próximos Milestones de Renderizado (Ordenados por Dependencia)

```mermaid
flowchart TD
    M1["Milestone 1: Post-Processing & Anti-Aliasing (Bloom + FXAA)"] --> M2["Milestone 2: Frustum Culling (AABB vs Camera Frustum)"]
    M2 --> M3["Milestone 3: Transparencias & Render Queue Sorting"]
    M3 --> M4["Milestone 4: GPU Instancing & SSBOs"]
    M4 --> M5["Milestone 5: Forward+ Clustered Lighting"]
    M5 --> M6["Milestone 6: RenderGraph Dinámico"]
```

### Detalle de los Milestones

#### Milestone 1: Post-Processing & Anti-Aliasing (Bloom Dual-Kawase + FXAA)
* **Objetivo**: Elevar drásticamente la calidad visual del showcase sin modificar el bucle de geometría.
* **Componentes**:
  1. **Dual-Kawase / Downsample-Upsample Bloom**: Extracción de altas luces ($L > 1.0$), pirámide de 5 niveles con interpolación bilineal ponderada, mezcla aditiva con la imagen HDR.
  2. **Fast Approximate Anti-Aliasing (FXAA 3.11)**: Eliminación de dientes de sierra en bordes geométricos y reflejos especulares de alto contraste.
* **Dependencia**: Usa el `m_HDRSceneFramebuffer` existente.

#### Milestone 2: Frustum Culling & Volúmenes de Envolvente (AABB / Bounding Sphere)
* **Objetivo**: Evitar enviar a la GPU mallas que caen fuera del campo de visión de la cámara o de las cascadas de sombras.
* **Componentes**:
  1. Extracción de los 6 planos del frustum desde la matriz `ViewProjection`.
  2. Cálculo de AABB local para cada primitiva (`MeshPrimitives`) y transformación al espacio de mundo.
  3. Descarte en CPU antes del `glDrawElements`.
* **Dependencia**: Requiere componente de Bounds y matemáticas de Frustum.

#### Milestone 3: Transparencias, Pasos Separados y Ordenación (Render Queue Sorting)
* **Objetivo**: Soporte formal para materiales transparentes (cristal, agua, partículas, UI 3D).
* **Componentes**:
  1. Clasificación en 2 colas: **Opaque Queue** y **Transparent Queue**.
  2. Ordenación: Opacos de frente hacia atrás (*front-to-back* para optimizar *Early-Z*), Transparentes de atrás hacia adelante (*back-to-front*).
  3. Paso transparente con `glDepthMask(GL_FALSE)` y mezcla alfa.
* **Dependencia**: Requiere Render Queue y cálculo de distancias al plano de la cámara.

#### Milestone 4: GPU Instancing & SSBOs (`glDrawElementsInstanced`)
* **Objetivo**: Renderizar miles de objetos repetidos (árboles, piedras, proyectiles, mallas modulares) en una única llamada de dibujo.
* **Componentes**:
  1. Buffer de instancias (`FVertexBuffer` por instancia o `Shader Storage Buffer Object - SSBO`).
  2. Matriz de modelo `u_InstanceModels[]` e identificadores de material por instancia.
* **Dependencia**: Requiere agrupar entidades ECS por par `(Mesh, Material)`.

#### Milestone 5: Forward+ / Clustered Light Culling (Compute Shader)
* **Objetivo**: Soportar cientos de luces dinámicas en tiempo real sin degradar el rendimiento.
* **Componentes**:
  1. División del frustum de la cámara en celdas 3D (*Clusters* $16 \times 9 \times 24$).
  2. Compute Shader que asigna qué luces intersectan cada celda y escribe índices en un SSBO.
  3. `PBR_Lit.glsl` evalúa solo las luces que afectan al cluster del fragmento.
* **Dependencia**: Requiere Compute Shaders y SSBOs.

#### Milestone 6: RenderGraph / FrameGraph
* **Objetivo**: Gestión desacoplada y automática de dependencias de pases, sincronización de barreras y reutilización de memoria de texturas transitorias.
* **Componentes**:
  1. Grafo acíclico dirigido (DAG) de pases de render.
  2. Creación y destrucción automática de render targets transitorios.
* **Dependencia**: Requiere que todos los pases anteriores estén modularizados.

---

## 4. Análisis de Validación del `MainShowcase.llevel`

| Actor en `MainShowcase` | Geometría | Material | Feature Validada | ¿Demuestra una Capacidad Real? |
| :--- | :--- | :--- | :--- | :--- |
| `PBR Ground Plane` | Plane $24\times 24$ | `M_FloorTiles` | Normal Maps, AO Maps, Planar Reflection | **SÍ**: Demuestra detalle de normales tangenciales y reflexión en suelo. |
| `PBR Polished Gold Sphere` | Sphere $R=0.5$ | `M_PolishedGold` | PBR Metálico ($\text{Met}=1.0, \text{Rough}=0.05$) | **SÍ**: Demuestra lóbulo especular IBL limpio y reflejos de entorno nítidos. |
| `PBR Glossy Ruby Sphere` | Sphere $R=0.5$ | `M_RubyDielectric` | PBR Dieléctrico ($\text{Met}=0.0, F_0=0.04$) | **SÍ**: Demuestra conservación de energía y Fresnel dieléctrico. |
| `PBR White Plastic Sphere`| Sphere $R=0.45$ | `M_WhitePlastic` | Dieléctrico Blanco ($\text{Rough}=0.25$) | **SÍ**: Demuestra dispersión difusa IBL pura. |
| `PBR Red Plastic Cube` | Cube $S=0.9$ | `M_RedPlastic` | Cubo con sombreado plano y aristas vivas | **SÍ**: Demuestra normales de cubo y sombras proyectadas. |
| `PBR Brushed Iron Cylinder`| Cylinder $R=0.5$ | `M_BrushedIron` | Cilindro metálico de rugosidad media | **SÍ**: Demuestra curvatura de cilindro y tapas superior/inferior. |
| `PBR Gold Metal Cylinder` | Cylinder $R=0.45$ | `M_GoldMetal` | Cilindro de oro brillante | **SÍ**: Demuestra anisotropía visual por curvatura geométrica. |
| `PBR Cobalt Pyramid` | Pyramid $1\times 1$ | `M_CobaltPyramid` | Caras triangulares inclinadas | **SÍ**: Demuestra sombreado con gradientes de luz solar oblicuos. |
| `PBR Rough Metal Pyramid` | Pyramid $0.9\times 0.9$ | `M_RoughMetal` | Metálico rugoso ($\text{Rough}=0.75$) | **SÍ**: Demuestra transición de lóbulo especular disperso. |
| `PBR Emerald Ramp` | Ramp $1\times 1$ | `M_EmeraldRamp` | Prisma triangular con hipotenusa | **SÍ**: Demuestra proyección de sombras rasantes en rampa. |
| `PBR Textured Cube` | Cube $S=1.0$ | `M_ContainerCube`| Texturas de albedo, normal y AO | **SÍ**: Demuestra mapeo UV de texturas en cubo. |
| `PBR Emissive Cyan Cube` | Cube $S=0.9$ | `M_Emissive` | Emisión pura ($L = 5.0$) | **SÍ**: Demuestra término emisivo HDR (resaltará con Bloom). |
| `Directional Sunlight` | - | Directional | CSM de 4 cascadas | **SÍ**: Demuestra sombras cascadas en todo el escenario. |
| `Dramatic Spotlight` | - | Spot | Cono de luz cian + Spot Shadows | **SÍ**: Demuestra sombra proyectada cónica individual. |
| `Orbiting Point Light` | - | Point | Luz puntual animada con atenuación | **SÍ**: Demuestra atenuación inversa cuadrática dinámica. |

---

## 5. Recomendación y Próximo Paso Lógico

### Próxima Feature Recomendada: **Milestone 1 — Post-Processing & Anti-Aliasing (Bloom Dual-Kawase + FXAA)**

#### ¿Por qué es el siguiente paso lógico?
1. **Completitud del Pipeline HDR**: El renderer ya escribe radiancias HDR flotantes en `m_HDRSceneFramebuffer` (`GL_RGBA16F`), pero actualmente solo aplica un tone-mapping directo sin explotar las altas luces. El cubo `PBR Emissive Cyan Cube` y los reflejos solares están listos para brillar con un lóbulo de **Bloom natural**.
2. **Eliminación de Aliasing Geométrico**: Con **FXAA 3.11** en el paso final, todas las aristas de los cilindros, rampas y cubos perderán el dentado de píxeles, logrando una imagen de calidad de producción.
3. **Cero regresiones en CPU/ECS**: No toca el sistema de componentes ni rompe la arquitectura de materiales.

#### Showcase a Diseñar:
* Visualización directa del efecto Bloom en el cubo emisivo y en los reflejos especulares de las esferas de oro y rubí.

#### Debug Views Asociadas:
* `Shift + F1`: Full Composite PBR Lit (con Post-Process completo).
* `Shift + B`: Visualización aislada del buffer de Bloom (*Bloom Only*).
* `Shift + A`: Toggle de Anti-Aliasing (FXAA ON/OFF en caliente).

#### Criterios Objetivos de Aceptación:
1. Las fuentes emisivas con radiancia $> 1.0$ generan un resplandor óptico suave y progresivo (*Dual-Kawase* sin artefactos de bloque).
2. Las geometrías con radiancia normal $\le 1.0$ no presentan sangrado de luz indeseado.
3. Las líneas de contraste alto muestran suavizado de bordes subpíxel con FXAA activo sin emborronar el texto 3D.
4. El coste del post-proceso completo se mantiene por debajo de **$0.8\text{ ms}$** a $1280 \times 720$ en la GPU.
5. El tiempo de arranque se mantiene inalterado ($\sim 70\text{ ms}$).
