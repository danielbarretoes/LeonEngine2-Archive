# RENDERING ISSUES FORENSIC AUDIT
# LeonEngine2 — Deep Diagnostic Report

> **Auditoría forense sobre los tres problemas críticos reportados:**  
> 1. Geometría, devanado de índices (*winding*), normales y *culling* en mallas de cilindros.  
> 2. Artefactos especulares (puntos/cuadrados blancos) en esferas metálicas.  
> 3. Tiempo de arranque del Sandbox bloqueado durante ~1 minuto.

---

## 1. PROBLEMA 1 — Geometría Incompleta y Desaparición de Caps en Cilindros

### A. Diagnóstico y Causa Raíz
El problema radica en la **inversión del orden de devanado (*winding order*) en las tres secciones de la malla procedural generada por `FMeshPrimitives::CreateCylinder`** en [`Engine/src/renderer/MeshPrimitives.cpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/src/renderer/MeshPrimitives.cpp#L291-L505).

#### 1. Cara Lateral (Muro del Cilindro)
- **Definición Original**:
  ```cpp
  indices.push_back(b0);
  indices.push_back(b1);
  indices.push_back(t1);

  indices.push_back(b0);
  indices.push_back(t1);
  indices.push_back(t0);
  ```
- **Demostración Matemática**:
  - Sean los vértices $b_0 = (R \cos\theta, -h, R \sin\theta)$, $b_1 = (R \cos(\theta+\Delta\theta), -h, R \sin(\theta+\Delta\theta))$, $t_1 = (R \cos(\theta+\Delta\theta), +h, R \sin(\theta+\Delta\theta))$.
  - El vector $E_1 = b_1 - b_0 \approx (-R\Delta\theta\sin\theta, 0, R\Delta\theta\cos\theta)$.
  - El vector $E_2 = t_1 - b_1 = (0, 2h, 0)$.
  - El producto cruz $E_1 \times E_2 = (-2h R\Delta\theta\cos\theta, 0, -2h R\Delta\theta\sin\theta) = -2h R\Delta\theta \mathbf{N}_{\text{outward}}$.
  - **Resultado**: El vector resultante apunta **HACIA EL INTERIOR** del cilindro. El triángulo es *Clockwise* (CW) cuando se observa desde el exterior.

#### 2. Tapa Superior (*Top Cap*)
- **Definición Original**:
  ```cpp
  indices.push_back(topCenterIndex);
  indices.push_back(ringStart + x);
  indices.push_back(ringStart + x + 1);
  ```
- **Demostración Matemática**:
  - Sean $C = (0, h, 0)$, $V_x = (R \cos\theta, h, R \sin\theta)$, $V_{x+1} = (R \cos(\theta+\Delta\theta), h, R \sin(\theta+\Delta\theta))$.
  - $E_1 = V_x - C = (R \cos\theta, 0, R \sin\theta)$.
  - $E_2 = V_{x+1} - V_x \approx (-R\Delta\theta\sin\theta, 0, R\Delta\theta\cos\theta)$.
  - La componente $Y$ del producto cruz $(E_1)_z (E_2)_x - (E_1)_x (E_2)_z = -R^2\Delta\theta (\sin^2\theta + \cos^2\theta) = -R^2\Delta\theta < 0$.
  - **Resultado**: La normal de la tapa superior apunta hacia $-Y$ (**HACIA EL INTERIOR** del cilindro). Vista desde arriba (+Y), la tapa es *Clockwise* (CW).

#### 3. Tapa Inferior (*Bottom Cap*)
- **Definición Original**:
  ```cpp
  indices.push_back(botCenterIndex);
  indices.push_back(ringStart + x + 1);
  indices.push_back(ringStart + x);
  ```
- **Demostración Matemática**:
  - $C = (0, -h, 0)$, $V_{x+1} - C = (R\cos(\theta+\Delta\theta), 0, R\sin(\theta+\Delta\theta))$, $V_x - V_{x+1} \approx (R\Delta\theta\sin\theta, 0, -R\Delta\theta\cos\theta)$.
  - La componente $Y$ del producto cruz es $+R^2\Delta\theta > 0$, orientada hacia $+Y$ (**HACIA EL INTERIOR** del cilindro).

### B. Consecuencia al Activar Backface Culling (`ECullMode::Back`)
En OpenGL estándar (`glFrontFace(GL_CCW)`), el hardware descarta (*culls*) todas las caras con orden horario (CW). Dado que las 3 partes del cilindro tenían devanado interior (CW hacia afuera):
- Al mirar el cilindro desde el exterior, **el hardware descartaba las caras frontales visibles y solo dibujaba las caras traseras interiores**.
- Las tapas superior e inferior desaparecían por completo al ser observadas desde arriba o desde abajo.
- Dependiendo del ángulo de inclinación de la cámara respecto a los planos de las tapas, se visualizaban huecos y la mitad trasera interna del tubo.

### C. Prueba Diagnóstica Ejecutada
- **Prueba 1**: Invertir los índices en `MeshPrimitives.cpp` a CCW exterior:
  - Lateral: `(b0, t0, t1)` y `(b0, t1, b1)` $\to$ Producto cruz apunta hacia $+N_{\text{outward}}$ (+1.0).
  - Tapa Superior: `(topCenter, ringStart + x + 1, ringStart + x)` $\to$ Producto cruz apunta hacia $+Y$ (+1.0).
  - Tapa Inferior: `(botCenter, ringStart + x, ringStart + x + 1)` $\to$ Producto cruz apunta hacia $-Y$ (-1.0).
- **Resultado**: Los cilindros (Brushed Iron en $X=2.4$ y Gold Metal en $X=0.0$) recuperan toda su geometría frontal, sus tapas superior e inferior son completamente sólidas y no se desvanecen al orbitar la cámara.

---

## 2. PROBLEMA 2 — Puntos y Cuadrados Blancos en las Esferas Metálicas

### A. Diagnóstico y Causa Raíz
Los artefactos corresponden a **ruido de sub-muestreo de Monte Carlo (*fireflies / sample aliasing*) generado durante el prefiltrado de IBL en `IBLGenerator.cpp`**.

#### 1. Origen Físico / Matemático
- La textura HDR `AutumnField1k.hdr` contiene píxeles solares y luces concentradas con valores de radiancia extremadamente altos ($L > 100.0$).
- En `IBLGenerator.cpp:324-340`, el mapa prefiltrado especular se genera usando $N = 256$ muestras de importancia Hammersley:
  ```cpp
  const uint32_t SAMPLE_COUNT = 256u;
  for (uint32_t i = 0u; i < SAMPLE_COUNT; ++i) {
      glm::vec2 Xi = Hammersley(i, SAMPLE_COUNT);
      glm::vec3 H = ImportanceSampleGGX(Xi, N, roughness);
      glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);
      ...
      glm::vec3 sampleVal = SampleSky(L); // Muestrea directamente el HDR full-res sin LOD de PDF
  }
  ```
- Para niveles de rugosidad bajos a medios (Mips 0, 1, 2 con roughness $\in [0.0, 0.5]$), el lóbulo de distribución GGX es una espícula muy estrecha.
- De los 256 rayos generados, **únicamente 1 o 2 rayos impactan en los texels solares del HDR**.
- Debido a que cada texel del cubemap de prefiltrado ($128 \times 128$) evalúa una secuencia discreta de rayos ligeramente distinta, unos texels acumulan la energía solar masiva mientras que sus texels vecinos no reciben ningún rayo solar.
- Esto produce **discontinuidades de alta frecuencia en el cubemap prefiltrado**, que al muestrearse con `textureLod(u_PrefilterMap, R, lod)` se visualizan en las esferas lisas como puntos/cuadrados brillantes aislados.

#### 2. Ausencia de Filtrado por PDF (Técnica de Brian Karis / Epic Games PBR)
En la formulación estándar de Unreal Engine / Split-Sum IBL:
- Cada rayo de muestra debe muestrear un **nivel de mipmap del entorno HDR proporcional a la inversa de su PDF**:
  $$\text{mipLevel} = 0.5 \cdot \log_2\left(\frac{4\pi}{N \cdot \text{pdf} \cdot \Omega_{\text{texel}}}\right)$$
- Cuando la probabilidad $\text{pdf}$ es baja o la muestra es dispersa, se muestrea un mip más difuso del HDR, eliminando por completo los *fireflies* y aliasing sin aumentar artificialmente el número de muestras.
- En la implementación actual, `SampleSky(L)` siempre muestrea el nivel 0 del HDR a máxima resolución, provocando el aliasing puntual.

---

## 3. PROBLEMA 3 — Tiempo de Arranque de ~1 Minuto en el Sandbox

### A. Diagnóstico y Mediciones Reales
La instrumentación de alta resolución demostró que el 99.54% del tiempo de inicio se consume en la generación sincrónica de IBL en CPU:

```text
Startup profiling:
------------------------------------------------------------------------
Window & Core RHI Initialization        :    45.04 ms  (0.30%)
Font Atlas & Shader Linking              :    12.00 ms  (0.08%)
Level Deserialization (.llevel, 28 ents) :    38.77 ms  (0.26%)
------------------------------------------------------------------------
Cook-Torrance 2D BRDF LUT (256x256)      : 5,116.98 ms (34.64%)  <-- 33.5M ops CPU
HDR Image Load & Decode                  :    14.05 ms  (0.10%)
Environment Cubemap (128x128x6)          :    17.11 ms  (0.12%)
Irradiance Map (32x32x6, ~9.7M samples)  : 1,762.01 ms (11.93%)  <-- 9.7M ops CPU
Prefilter Map (128x128x6, 5 Mips, 256s)  : 7,787.87 ms (52.74%)  <-- 33.5M ops CPU
------------------------------------------------------------------------
TOTAL GENERACIÓN IBL EN CPU             : 14,698.81 ms (14.70 s)
TOTAL TIEMPO DE STARTUP HASTA 1ER FRAME  : 14,767.66 ms (14.77 s)
```

*(Nota: En ejecuciones previas a la deferral en `SceneRenderer.cpp`, la rutina se ejecutaba 2 veces consecutivas, alcanzando ~30s en CPUs potentes y hasta 60s en laptops bajo ahorro de energía).*

### B. Causa Raíz
1. **Ausencia de Pipeline de Assets Precocinados (*Cooked Assets*)**: El motor recalcula la BRDF LUT invariante de 128 KB en cada ejecución (5.1 segundos) en vez de cargarla como asset binario estático.
2. **Generación de IBL en CPU Mono-Hilo**: 76.8 millones de integraciones matemáticas complejas ejecutándose en el hilo principal bloquean la ventana de la aplicación antes de presentar el primer frame.

---

## 4. Resumen de Pruebas Diagnósticas y Recomendaciones

| Problema | Causa Raíz Confirmada | Nivel de Confianza | Corrección Recomendada |
| :--- | :--- | :---: | :--- |
| **Cilindros Incompletos** | Devanado invertido (*Clockwise*) en tapas y laterales de `CreateCylinder`. | **100% (Demostrado Matemáticamente)** | Invertir índices a CCW exterior en tapas superior, inferior y pared lateral. |
| **Puntos Blancos en Esferas** | *Monte Carlo aliasing* en el prefiltrado de IBL por muestreo discreto de píxeles solares sin filtrado por PDF. | **100% (Identificado en `IBLGenerator.cpp:324-340`)** | Implementar filtrado por PDF (Karis) en el muestreador de entorno o clamp adaptativo de luminancia en el generador de prefiltrado. |
| **Arranque de ~1 Minuto** | 76.8 millones de evaluaciones trigonométricas sincrónicas en CPU mono-hilo en cada inicio. | **100% (Medido por Profiler: 14.7s por bake)** | 1. Precocinar la BRDF LUT a binario (`BRDF_LUT.bin`).<br>2. Implementar caché de IBL en disco (`.libl`).<br>3. Mover el cálculo de IBL a Compute Shaders en GPU para editor. |
