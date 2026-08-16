# LeonEngine2 — GPU Headless Shaders & Direct Pipeline Testing Architecture

Este documento detalla la arquitectura de pruebas de integración de **shaders reales en hardware GPU offscreen**, la infraestructura [`HeadlessGLContext.hpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Tests/GPU/HeadlessGLContext.hpp) y la suite automatizada de **GLSL Shader Mutation Testing** implementada en LeonEngine2.

---

## 1. Motivación y Cierre de Brecha Arquitectónica

Antes de esta fase, existía una discrepancia entre:
* **Modelo Matemático CPU ([`FPBRMath.hpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Source/Runtime/Renderer/Public/Renderer/FFPBRMath.hpp)):** Validado mediante pruebas unitarias en C++.
* **Shader Real de Producción ([`PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl)):** Ejecutado exclusivamente por el hardware gráfico durante el renderizado.

Para eliminar cualquier "falsa confianza", se ha construido un arnés de pruebas que compila el código GLSL real desde disco, inicializa un contexto OpenGL 4.5 Core invisible, dibuja fragmentos en FBOs de punto flotante de alta precisión (`GL_RGBA32F` / `GL_RGBA16F`) y compara los píxeles leídos de vuelta vía `glReadPixels` contra formulaciones físicas independientes.

```text
  [ Disco Binario ]
  Engine/Assets/Shaders/PBR_Lit.glsl
            |
            v  (Compilación real en GPU)
  +----------------------------------------------------------------+
  |  OpenGL 4.5 Core Headless Offscreen Context (GLFW_VISIBLE=0)    |
  |  - FBO 1x1 / 16x16 (GL_RGBA32F / GL_RGBA16F)                   |
  |  - Fullscreen / Unit Quad Mesh (VBO/EBO/VAO)                   |
  |  - UBO Binding 0: CameraData (std140, 432 bytes)               |
  |  - UBO Binding 1: LightingData (std140, 1376 bytes)            |
  |  - Sampler Units 0..11 con Texturas Fallback 1x1               |
  +----------------------------------------------------------------+
            |
            v  (glDrawElements + glReadPixels)
      Píxel GPU Real (R, G, B, A)
            |
            v  (Validación Estricta Numérica con Tolerancia Física)
      Analytical Cook-Torrance Ground Truth
```

---

## 2. Componentes de la Infraestructura Headless

### 2.1 `FHeadlessGLContext` ([`Tests/GPU/HeadlessGLContext.hpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Tests/GPU/HeadlessGLContext.hpp))
* **Contexto Offscreen:** Inicializa GLFW en modo oculto (`GLFW_VISIBLE = GLFW_FALSE`) con perfil Core 4.5.
* **Carga de Extensiones:** Carga GLAD dinámicamente y activa `GL_TEXTURE_CUBE_MAP_SEAMLESS`.
* **RenderDriver Activation:** Registra `FOpenGLRenderDriver` en el motor para que `FShader::Create` compile nativamente shaders de producción.
* **Framebuffers:** Configura un FBO con Color Attachment flotante (`GL_RGBA32F`) y Depth/Stencil Attachment (`GL_DEPTH24_STENCIL8`).
* **Quad Unitario:** Geometría de 4 vértices con `Position`, `Normal`, `TexCoord`, `Tangent`, `Bitangent` y `Color`.
* **Gestión de UBOs:** Asigna y actualiza buffers std140 para `CameraData` (binding 0) y `LightingData` (binding 1).
* **Texturas Fallback:** Mantiene 12 unidades de textura activas y enlazadas con texturas $1\times 1$ por defecto (White, Black, Flat Normal, Cubemap, Shadow 2D, Shadow Array).

---

## 3. Matriz de Suites de Tests de Shaders en GPU

| Archivo de Test | Casos de Prueba Cubiertos | Parámetros Evaluados | Referencia Analítica / Invariante |
| :--- | :--- | :--- | :--- |
| [`PBRShaderFresnelTests.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Tests/Shader/PBRShaderFresnelTests.cpp) | Fresnel-Schlick en GPU | $\cos\theta \in \{1.0, 0.5, 0.0\}$ en dieléctricos y oro | $F(1)=F_0$, $F(0.5)=0.07000$, $F(0)=1.0$ |
| [`PBRShaderCookTorranceTests.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Tests/Shader/PBRShaderCookTorranceTests.cpp) | Direct Lighting Cook-Torrance | Dieléctrico ($m=0, r=0.5$) y Metal ($m=1, r=0.5$) | $Lo_{\text{diel}} = 0.356507$, $Lo_{\text{metal}} = (1.273, 0.904, 0.369)$ |
| [`PBRShaderGGXSmithTests.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Tests/Shader/PBRShaderGGXSmithTests.cpp) | GGX NDF & Smith $G_1 \cdot G_2$ | Rugosidad $r \in [0.05, 1.0]$, ángulo rasante $60^\circ$ | Escala monótona $D(r)$, Enmascaramiento $G = 0.609161$ |
| [`PBRShaderDirectLightsTests.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Tests/Shader/PBRShaderDirectLightsTests.cpp) | Directional, Point & Spot | Distancias $d \in \{1, 2, 12\}$, Cono y Penumbra | $N\cdot L$ respuesta, Atenuación UE4, Smoothstep |
| [`PBRShaderIBLTests.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Tests/Shader/PBRShaderIBLTests.cpp) | Image-Based Lighting Pipeline | Cubemap $L=\pi$, Cubemap $L=0$, Metal $m=1$ con LUT | $E_{\text{diff}} = \text{albedo}$, $E_{\text{black}} = 0$, $E_{\text{metal}} = 0.90$ |
| [`PBRShaderShadowTests.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Tests/Shader/PBRShaderShadowTests.cpp) | Sombras Cascaded & PCF | Unshadowed vs Shadowed ($Lo \cdot (1 - \text{shadow})$) | Oclusión completa ante $\text{shadow}=1$ |
| [`PBRShaderPlanarReflectionTests.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Tests/Shader/PBRShaderPlanarReflectionTests.cpp) | Planar Reflections | Toggle $0 \leftrightarrow 1$, textura azul de reflexión | Mezcla especular con textura planar activa |
| [`PBRShaderDeterminismTests.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Tests/Shader/PBRShaderDeterminismTests.cpp) | Repetibilidad GPU | 10 pasadas consecutivas con idénticas entradas | $\Delta = 0.000000$ bit-a-bit en VRAM |

---

## 4. Auditoría de GLSL Mutation Testing (15 Mutaciones)

Se diseñó y ejecutó el script [`Scripts/run_shader_mutations.py`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Scripts/run_shader_mutations.py) que modifica deliberadamente el código fuente GLSL de [`Engine/Assets/Shaders/PBR_Lit.glsl`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Assets/Shaders/PBR_Lit.glsl) y corre la suite de pruebas GPU para comprobar su detección:

```text
=============================================================================================================================
MUTACIÓN INTRODUCIDA EN PBR_LIT.GLSL     | TEST ESPERADO QUE DEBÍA FALLAR         | RESULTADO REAL | TEST QUE FALLÓ
=============================================================================================================================
MUTATION_SHADER_A (Fresnel 5.0 -> 4.0)   | PBRShaderFresnelTests (Oblique 60 deg) | CAUGHT [PASS]  | Hardware GPU Fresnel Schlick
MUTATION_SHADER_B (GGX Denom sin cuad.)  | PBRShaderCookTorranceTests             | CAUGHT [PASS]  | Exact Hardware GPU Numerical Output
MUTATION_SHADER_C (Smith G1 sin G2 mask) | PBRShaderGGXSmithTests (Oblique Angle) | CAUGHT [PASS]  | Hardware GPU Fresnel Schlick / Smith
MUTATION_SHADER_D (Invertir metallic kD) | PBRShaderCookTorranceTests             | CAUGHT [PASS]  | Exact Hardware GPU Numerical Output
MUTATION_SHADER_E (Eliminar diffuse)     | PBRShaderCookTorranceTests             | CAUGHT [PASS]  | Exact Hardware GPU Numerical Output
MUTATION_SHADER_F (Ignorar NdotL)        | PBRShaderDirectLightsTests             | CAUGHT [PASS]  | Hardware GPU Fresnel Schlick / NdotL
MUTATION_SHADER_G (Romper atenuación UE4)| PBRShaderDirectLightsTests             | CAUGHT [PASS]  | Point Light UE4 Radius Attenuation
MUTATION_SHADER_H (Invertir cono spot)   | PBRShaderDirectLightsTests             | CAUGHT [PASS]  | Spot Light Conical Cutoff Penumbra
MUTATION_SHADER_I (Cero IBL Difusa)      | PBRShaderIBLTests                      | CAUGHT [PASS]  | Real Hardware GPU IBL Pipeline
MUTATION_SHADER_J (Ignorar BRDF LUT)     | PBRShaderIBLTests (Metallic Specular)  | CAUGHT [PASS]  | Real Hardware GPU IBL Pipeline
MUTATION_SHADER_K (Invertir toggle planar)| PBRShaderPlanarReflectionTests         | CAUGHT [PASS]  | Real Hardware GPU IBL Pipeline
MUTATION_SHADER_L (Invertir alpha GGX)   | PBRShaderGGXSmithTests                 | CAUGHT [PASS]  | Exact Hardware GPU Numerical Output
MUTATION_SHADER_M (Quitar PI en diffuse) | PBRShaderCookTorranceTests             | CAUGHT [PASS]  | Exact Hardware GPU Numerical Output
MUTATION_SHADER_N (Invertir F(pi/2))     | PBRShaderFresnelTests                  | CAUGHT [PASS]  | Hardware GPU Fresnel Schlick
MUTATION_SHADER_O (Quitar Lo en HDR out) | PBRShaderDeterminismTests              | CAUGHT [PASS]  | Deterministic GPU Shading Run
=============================================================================================================================
RESUMEN DE EFICACIA ANTE MUTACIONES GLSL:
Mutaciones Evaluadas:  15
Mutaciones Detectadas: 15 / 15 (100.0% CAUGHT)
Mutaciones Escapadas:  0 / 15 (0.0%)
=============================================================================================================================
```

---

## 5. Comandos de Ejecución

### Ejecutar Toda la Suite de Tests (CPU Math + IBL + GPU Shaders)
```powershell
python Scripts/run_tests.py
```

### Ejecutar Exclusivamente los Tests de Shaders en GPU
```powershell
python Scripts/run_tests.py -ts="*Shader*"
```

### Ejecutar la Suite de Shader Mutation Testing
```powershell
python Scripts/run_shader_mutations.py
```
