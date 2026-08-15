# GUÍA MAESTRA DE TESTING Y REGRESIÓN MATEMÁTICA DEL RENDERER
# LeonEngine2 — Manual de Uso, Suites, Invariantes y Umbrales

> **Documentación técnica para la ejecución, mantenimiento, interpretación y extensión de la suite de pruebas automatizadas de regresión matemática de LeonEngine2.**

---

## 1. Comandos de Ejecución

La suite se ejecuta desde la raíz del proyecto mediante el script de Python o directamente a través del binario generado por Ninja/CMake:

### Ejecución de Todos los Tests
```powershell
python Scripts/run_tests.py
```
*O alternativamente vía CMake/CTest:*
```powershell
ctest --test-dir build --output-on-failure
```

### Ejecución por Sub-Suites (Filtro por Nombre de Suite `-ts`)
```powershell
# 1. Solo pruebas de PBR Cook-Torrance y BRDF LUT
python Scripts/run_tests.py -ts="*PBR*"

# 2. Solo pruebas de Image-Based Lighting (Irradiancia, Prefilter, Sol)
python Scripts/run_tests.py -ts="*IBL*"

# 3. Solo pruebas de Matemática Pura (Vectores, TBN, Hammersley, PDF)
python Scripts/run_tests.py -ts="*Math*"

# 4. Solo pruebas de Caché e Invalidación (.libl, FNV-1a)
python Scripts/run_tests.py -ts="*Cache*"

# 5. Solo pruebas de Decodificación HDR y Mipmaps
python Scripts/run_tests.py -ts="*HDR*"

# 6. Solo pruebas de Integración GPU (Upload RGBA16F, Seamless Cubemap)
python Scripts/run_tests.py -ts="*GPU*"
```

### Ejecución de un Test Individual (Filtro por Nombre de Test `-tc`)
```powershell
# Ejecutar únicamente el test de convergencia solar
python Scripts/run_tests.py -tc="*Solar*"
```

---

## 2. Inventario de Suites y Pruebas Implementadas

```text
========================================================================================
SUITE DE TESTS              | ARCHIVO FUENTE                 | TIPO    | ASERCIONES
========================================================================================
Math - Vector & TBN         | Tests/Math/VectorTBNTests.cpp  | CPU/Math|         45
Math - Hammersley & QMC     | Tests/Math/HammersleyTests.cpp | CPU/Math|      7,174
Math - Hemisphere PDF       | Tests/Math/HemispherePDFTests  | CPU/Math|         32
IBL - Irradiance Invariants | Tests/IBL/IrradianceConvTests  | CPU/Math|         24
IBL - Reference Integrator  | Tests/IBL/ReferenceIntegrator  | CPU/Math|         12
IBL - Solar HDR Regression  | Tests/IBL/SolarHDRRegression   | CPU/Math|         49
IBL - Spatial Continuity    | Tests/IBL/IrradianceContinuity | CPU/Math|     24,582
IBL - Specular Prefilter    | Tests/IBL/SpecularPrefilter    | CPU/Math|        180
IBL - Determinism           | Tests/IBL/DeterminismTests     | CPU/Math|         20
PBR - BRDF LUT Invariants   | Tests/PBR/BRDFLUTTests.cpp     | CPU/Math|    524,310
PBR - Cook-Torrance BRDF    | Tests/PBR/PBRBrdfTests.cpp     | CPU/Math|         72
PBR - Energy Conservation   | Tests/PBR/EnergyConservation   | CPU/Math|        686
HDR - Decoding & Radiance   | Tests/HDR/HDRDecodingTests.cpp | CPU/Math|  7,526,403
HDR - Mipmap Pyramid & Wrap | Tests/HDR/MipChainTests.cpp    | CPU/Math|      1,720
Cache - Serialization       | Tests/Cache/SerializationTests | CPU/Disk|         10
Cache - Invalidation        | Tests/Cache/CacheInvalidation  | CPU/Disk|          9
GPU - Texture Upload & RHI  | Tests/GPU/TextureUploadTests   | GPU/GL  |     16,386
GPU - Cubemap Seams         | Tests/GPU/CubemapSeamTests.cpp | CPU/Math|      2,176
----------------------------------------------------------------------------------------
TOTALES                     | 18 Archivos de Prueba          | 27 Tests|  8,098,870 PASS
========================================================================================
```

---

## 3. Umbrales y Criterios Matemáticos de Aceptación

| Métrica / Invariante | Umbral de Test | Justificación Física / Matemática |
| :--- | :---: | :--- |
| **Ortonormalidad TBN** | $\|T \cdot N\| < 10^{-5}$ | Gram-Schmidt ortogonal estricto en marco tangente. |
| **Media Coseno Hemisferio** | $\mathbb{E}[\cos\theta] \in [0.65, 0.68]$ | Distribución ponderada por coseno analítica ($\mathbb{E} = 2/3 \approx 0.6667$). |
| **Normalización de PDF** | $\int_\Omega p(\omega) d\omega = 1.0 \pm 0.001$ | Condición estricta de función de densidad de probabilidad. |
| **Entorno Constante $L=1$** | $E = \pi \pm 0.01$ | $\int_{\text{hemisphere}} \cos\theta \, d\omega = \pi$. Detecta errores en PDF o pesos. |
| **Entorno Negro $L=0$** | $E = 0.0 \pm 10^{-5}$ | Sin radiación incidente no puede existir irradiancia. |
| **Step Function Hemisferio** | $+Y \to \pi, -Y \to 0, +X \to \pi/2$ | Simetría analítica en domos de luz divididos. |
| **Error vs Integrador Riemann** | $\text{RelErr} < 8\%$ | Coherencia entre Monte Carlo y suma de Riemann de alta resolución. |
| **Discontinuidad Solar Vecindad** | $\text{Ratio} \le 1.25\text{x}$ | Eliminación de picos aislados provocados por el submuestreo del sol. |
| **Luminancia Solar Máxima IBL** | $L_{\text{irradiance}} < 25.0$ | Impide que el sol puntual ($114,033$) se filtre como valor discreto en el cubemap. |
| **Conservación de Energía** | $k_D + k_S \le 1.0001$ | Segunda ley de la termodinámica: una superficie no emite más luz de la que recibe. |
| **Dieléctricos ($m=0$)** | $k_D + k_S \equiv 1.0$ | División complementaria estricta entre reflexión especular y refracción difusa. |
| **Metales Puros ($m=1$)** | $k_D \equiv 0.0$ | Los conductores absorben toda la luz refractada en la banda de conducción. |
| **BRDF LUT Bounds** | $A \in [0, 1], B \in [0, 1], A+B \le 1.05$ | Factores de escala y sesgo del Split-Sum de Cook-Torrance. |
| **Serialización `.libl`** | Diferencia bit-a-bit $= 0$ | Fidelidad absoluta sin pérdida de precisión en buffers flotantes en disco. |
| **Invalidación FNV-1a** | $\text{Hash}(A) \neq \text{Hash}(A')$ | El cambio de un único byte en el HDR fuente invalida la caché automáticamente. |
| **Upload GPU Half-Float** | $\text{Error} \le 0.005$ | Precisión nativa de la mantisa de 11 bits en `GL_RGBA16F`. |

---

## 4. Cómo Interpretar y Diagnosticar un Fallo

### A. Fallo en `Constant Environment L = 1.0 Produces Exactly E = PI`
* **Causa**: Alguien modificó la ponderación por coseno, el factor de normalización $\frac{\pi}{N}$, la base ortonormal TBN o la PDF en `IBLMath.hpp` / `IBLGenerator.cpp`.
* **Solución**: Verificar que la acumulación de muestras use $\frac{\pi}{N} \sum L_i$ y que el marco tangente no altere la longitud del vector.

### B. Fallo en `All 6 Faces x 32x32 Spatial Neighbor Outlier Ratio <= 1.25x`
* **Causa**: Se desactivó el filtrado de Mipmaps por ángulo sólido en la irradiancia (`lod_irradiance`), provocando que rayos aislados golpeen el sol de $114,033$.
* **Solución**: Restaurar el cálculo de LOD $\text{lod} = \max(0.5 \log_2(\Omega_s / \Omega_p) + 1.0, 0.0)$.

### C. Fallo en `Diffuse and Specular Fractions Satisfy kD + kS <= 1.0`
* **Causa**: Se alteró la conservación de energía en `PBRMath.hpp` o `PBR_Lit.glsl` permitiendo que $k_D = 1 - k_S$ sin multiplicar por $(1 - \text{metallic})$.
* **Solución**: Asegurar $k_D = (1 - k_S)(1 - \text{metallic})$ y $k_S = F$.

### D. Fallo en `Round-Trip Bitwise Identical Serialization`
* **Causa**: Se modificó la cabecera `FIBLCacheHeader` o el orden de escritura de caras/mips en `.libl`.
* **Solución**: Sincronizar `FIBLCacheHeader::Version` y verificar los bucles de serialización.

---

## 5. Arquitectura para Integración Continua (CI)

1. **Compilación de Tests**: `RendererTests` compila como un ejecutable autónomo.
2. **Ejecución Headless**: La suite de pruebas de CPU corre sin requerir ventana gráfica ni display en servidores Linux/Windows.
3. **GPU Integration**: El test de integración `GPU - Texture Upload` inicializa un contexto offscreen GLFW/GLAD invisible de $64 \times 64$; si el host no tiene GPU, el test se omite limpiamente sin romper el pipeline de CI.
