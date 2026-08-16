# ARQUITECTURA DE TESTING DEL RENDERER Y SUITE DE REGRESIÓN MATEMÁTICA
# LeonEngine2 — Framework de Validación Automática y Red de Seguridad Matemática

> **Documento de diseño arquitectónico para la suite de pruebas de regresión matemática, invariantes físicos y estabilidad numérica del renderer de LeonEngine2.**

---

## 1. Visión General y Filosofía de Calidad

LeonEngine2 implementa un pipeline de renderizado físico (PBR Cook-Torrance, Image-Based Lighting Split-Sum, Cascaded Shadow Maps, Planar Reflections). La estabilidad visual del motor depende de la **estricta exactitud de sus propiedades matemáticas** (distribuciones estadísticas, conservación de energía, continuidad espacial y fidelidad de los buffers).

### Principio Fundamental:
> *"Si en el futuro se modifica `IBLGenerator.cpp`, `PBR_Lit.glsl`, la serialización `.libl`, la generación de mips o el muestreo de texturas, la suite de pruebas debe fallar inmediatamente y de forma determinista ante cualquier pérdida de continuidad, divergencia estadística o violación física."*

Las pruebas de regresión se dividen formalmente en:
1. **Tests de Matemática Pura (CPU, Herméticos, Zero-GPU)**: Ejecutables en CI sin drivers gráficos, deterministas y de ejecución en milisegundos.
2. **Tests de Validación de Assets y Caché (CPU/Disk)**: Verificación bit-a-bit de serialización `.libl`, integridad de cabeceras e invalidación por hash FNV-1a.
3. **Tests de Integración y RHI (OpenGL 4.5 / GPU Headless)**: Verificación de subida/descarga de texturas, formatos flotantes (`GL_RGBA16F`, `GL_RG16F`) y `GL_TEXTURE_CUBE_MAP_SEAMLESS`.

---

## 2. Matriz de Clasificación de Pruebas

```text
┌───────────────────────────────────────────────────────────────────────────┐
│                     SUITE DE TESTS DE LEONENGINE2                         │
├─────────────────────────────────────┬─────────────────────────────────────┤
│ 1. CPU PURE MATHEMATICAL SUITE      │ 2. GPU INTEGRATION & ASSET SUITE    │
│    (Sin dependencias de GPU ni GL)  │    (Contexto OpenGL + Formatos)     │
├─────────────────────────────────────┼─────────────────────────────────────┤
│ • Invariantes de Base TBN           │ • Round-Trip Serialización .libl    │
│ • Secuencia Quasi-MC Hammersley     │ • Invalidación de Caché FNV-1a      │
│ • PDF de Hemisferio Ponderado       │ • Subida / Bajada de Texturas GPU   │
│ • Convolución de Irradiancia Difusa │ • Formatos Flotantes (RGBA16F)      │
│ • Integrador Riemann de Referencia  │ • Continuidad en Bordes de Cubemap  │
│ • Filtrado Karis por Ángulo Sólido  │ • Muestreo Seamless de Cubemaps     │
│ • Invariantes GGX, Smith & Schlick  │ • Golden Target Regression Buffers  │
│ • Conservación de Energía PBR       │                                     │
│ • Pirámide de Mipmaps HDR 360°      │                                     │
└─────────────────────────────────────┴─────────────────────────────────────┘
```

---

## 3. Desacoplamiento de Funciones Matemáticas Puras

Para garantizar que los tests prueben **la implementación de producción real** sin duplicar código ni requerir un contexto OpenGL para evaluar fórmulas matemáticas, se extrae la lógica analítica a cabeceras de utilidad matemática pura:

1. **[`Engine/Source/Runtime/Renderer/Public/Renderer/FFIBLMath.hpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Source/Runtime/Renderer/Public/Renderer/FFIBLMath.hpp)**:
   - `RadicalInverse_VdC(uint32_t bits)`
   - `Hammersley(uint32_t i, uint32_t N)`
   - `ImportanceSampleGGX(glm::vec2 Xi, glm::vec3 N, float roughness)`
   - `CosineSampleHemisphere(glm::vec2 Xi, glm::vec3 N)`
   - `CosineHemispherePDF(float cosTheta)`
   - `GeometrySchlickGGX_IBL(float NdotV, float roughness)`
   - `GeometrySmith_IBL(glm::vec3 N, glm::vec3 V, glm::vec3 L, float roughness)`
   - `IntegrateBRDF(float NdotV, float roughness, uint32_t sampleCount)`
   - `GetCubeDirection(int face, float u, float v)`
   - `FHDREquirectangularMipChain` (Construcción, interpolación bilineal y muestreo trilineal con wrap horizontal)
   - `FIBLCacheHeader` y `ComputeFileHash64(const std::string& path)`
2. **[`Engine/Source/Runtime/Renderer/Public/Renderer/FFPBRMath.hpp`](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Engine/Source/Runtime/Renderer/Public/Renderer/FFPBRMath.hpp)**:
   - `DistributionGGX(float NdotH, float roughness)`
   - `GeometrySchlickGGX_Direct(float NdotV, float roughness)`
   - `GeometrySmith_Direct(float NdotV, float NdotL, float roughness)`
   - `FresnelSchlick(float cosTheta, glm::vec3 F0)`
   - `FresnelSchlickRoughness(float cosTheta, glm::vec3 F0, float roughness)`
   - `EvaluateCookTorrance(glm::vec3 N, glm::vec3 V, glm::vec3 L, glm::vec3 albedo, float metallic, float roughness)`
   - `ValidateEnergyConservation(glm::vec3 kD, glm::vec3 F, float metallic)`

---

## 4. Estructura de Directorios de Pruebas (`Tests/`)

```text
Tests/
├── CMakeLists.txt                         # Target CTest/Runner: RendererTests
├── TestRunner.cpp                         # Punto de entrada doctest / reporter
│
├── Math/
│   ├── VectorTBNTests.cpp                 # Orthonormalidad TBN, Gram-Schmidt
│   ├── HammersleyTests.cpp                # Dispersión uniforme Quasi-MC, bounds [0, 1)
│   └── HemispherePDFTests.cpp             # Integración de PDF = 1, cos(theta)/PI
│
├── IBL/
│   ├── IrradianceConvolutionTests.cpp     # Entorno constante (E=PI), negro (E=0), canales
│   ├── ReferenceIntegratorTests.cpp       # Comparación contra Riemann analítico (100k samples)
│   ├── SolarHDRRegressionTests.cpp        # AutumnField1k.hdr, disco solar, saltos vecindad
│   ├── IrradianceContinuityTests.cpp      # Cubemap 6x32x32, ratios vecindad <= 1.15x
│   └── SpecularPrefilterTests.cpp         # 5 Mips, respuesta continua a la rugosidad
│
├── PBR/
│   ├── BRDFLUTTests.cpp                   # Invariantes LUT 2D, monotonía, límites
│   ├── PBRBrdfTests.cpp                   # GGX >= 0, Smith in [0,1], Fresnel in [0,1]
│   └── EnergyConservationTests.cpp        # kD + kS <= 1, metales vs dieléctricos
│
├── HDR/
│   ├── HDRDecodingTests.cpp               # Carga RGBE, valores finitos, no NaN/Inf
│   └── MipChainTests.cpp                  # Pirámide 1024..1, wrap horizontal 360°
│
├── Cache/
│   ├── SerializationTests.cpp             # Round-trip .libl bitwise identical
│   └── CacheInvalidationTests.cpp         # Hash mismatch -> miss, Version -> miss
│
├── GPU/
│   └── TextureUploadTests.cpp             # Headless GL upload/readback, RGBA16F, Seamless
│
└── Fixtures/                              # Datos sintéticos pequeños para pruebas instantáneas
    ├── ConstantWhiteHDR.hdr               # Entorno constante L = 1.0 (16x8)
    ├── PureRedHDR.hdr                     # Entorno monocromático R=1, G=0, B=0 (16x8)
    ├── HemisphereStepHDR.hdr              # Hemisferio iluminado superior / inferior negro
    └── SyntheticSolarHDR.hdr              # Cielo suave + punto solar calibrado
```

---

## 5. Framework de Pruebas Seleccionado: `doctest`

Se utiliza **`doctest`** (v2.4.11) por las siguientes razones de ingeniería:
1. **Velocidad de Compilación**: Es el framework C++20 más rápido del ecosistema (compila en $< 200\text{ ms}$).
2. **Cero Dependencias**: Header único autocontenido (`ThirdParty/doctest/doctest.h`).
3. **Filtros por Línea de Comandos**: Permite ejecutar sub-suites inmediatamente:
   - `./RendererTests -tc="*Math*"`
   - `./RendererTests -tc="*IBL*"`
   - `./RendererTests -tc="*PBR*"`
   - `./RendererTests -tc="*Cache*"`
4. **Aproximación Numérica (`doctest::Approx`)**: Tolerancias explícitas `epsilon` para floats y vectores GLM.

---

## 6. Pipeline de Ejecución Unificado (`Scripts/run_tests.py`)

Se provee un script maestro en Python:
```powershell
python Scripts/run_tests.py
```
Que compila incrementalmente el ejecutable de pruebas y reporta:
- Total de pruebas ejecutadas.
- Número de aserciones evaluadas.
- Tiempo total en milisegundos.
- Desglose por suite (Math, IBL, PBR, HDR, Cache, GPU).
