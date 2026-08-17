#type vertex
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec4 aTangent; // xyz + handedness (B = cross(N,T)*w)
layout(location = 4) in vec3 aColor;
layout(location = 5) in vec2 aLightmapUV;
layout(location = 6) in ivec4 aBoneIndices;
layout(location = 7) in vec4 aBoneWeights;

out vec3 v_FragPos;
out vec3 v_Normal;
out vec2 v_TexCoord;
out vec3 v_Color;
out mat3 v_TBN;
out vec2 v_LightmapUV;

layout(std140) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_LightSpaceMatrices[4];
    mat4 u_SpotLightSpaceMatrix;
    vec4 u_ViewPos;
    vec4 u_CameraForward;
    vec4 u_CascadeSplits;
    vec4 u_ShadowParams;
    ivec4 u_ShadowSettings; // x = filterMode, y = shadowedSpotIndex, z = 0, w = debug
};

layout(std140, binding = 2) uniform BonePalette {
    mat4 u_Bones[128];
};

uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;
uniform int u_EnableClipPlane = 0;
uniform vec4 u_ClipPlane = vec4(0.0, 1.0, 0.0, 0.0);

mat4 SkinMatrix() {
    return u_Bones[aBoneIndices.x] * aBoneWeights.x
         + u_Bones[aBoneIndices.y] * aBoneWeights.y
         + u_Bones[aBoneIndices.z] * aBoneWeights.z
         + u_Bones[aBoneIndices.w] * aBoneWeights.w;
}

void main() {
    mat4 skin = SkinMatrix();
    vec4 skinnedPos = skin * vec4(aPos, 1.0);
    mat3 skinN = mat3(skin);
    vec3 skinnedN = skinN * aNormal;
    vec3 skinnedT = skinN * aTangent.xyz;

    vec4 worldPos = u_Model * skinnedPos;
    v_FragPos = worldPos.xyz;

    vec3 N = normalize(u_NormalMatrix * skinnedN);
    vec3 T = u_NormalMatrix * skinnedT;
    float tLen = length(T);
    T = tLen > 1e-8 ? T / tLen : vec3(1.0, 0.0, 0.0);
    T = normalize(T - N * dot(N, T));
    float detSign = determinant(u_NormalMatrix) < 0.0 ? -1.0 : 1.0;
    vec3 B = cross(N, T) * aTangent.w * detSign;
    v_TBN = mat3(T, B, N);
    v_Normal = N;

    v_TexCoord = aTexCoord;
    v_Color = aColor;
    v_LightmapUV = aLightmapUV;

    gl_ClipDistance[0] = (u_EnableClipPlane == 1) ? dot(vec4(worldPos.xyz, 1.0), u_ClipPlane) : 1.0;
    gl_Position = u_ViewProjection * worldPos;
}

#type fragment
#version 450 core

layout(location = 0) out vec4 FragColor;

in vec3 v_FragPos;
in vec3 v_Normal;
in vec2 v_TexCoord;
in vec3 v_Color;
in mat3 v_TBN;
in vec2 v_LightmapUV;

layout(std140) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_LightSpaceMatrices[4];
    mat4 u_SpotLightSpaceMatrix;
    vec4 u_ViewPos;
    vec4 u_CameraForward;
    vec4 u_CascadeSplits;
    vec4 u_ShadowParams;
    ivec4 u_ShadowSettings; // x = filterMode, y = shadowedSpotIndex, z = 0, w = debug
};

// Direct Lighting & Environment Subsystem (std140)
// PBR-correct: single Intensity per light — no Phong Ambient/Diffuse/Specular split (audit fix ALTO-05)
struct DirectionalLight {
    vec4 direction;   // xyz = dir (normalized), w = enabled (1.0 / 0.0)
    vec4 color;       // xyz = color, w = intensity (radiance multiplier)
};

struct PointLight {
    vec4 position;    // xyz = pos, w = enabled (1.0 / 0.0)
    vec4 color;       // xyz = color, w = intensity
    vec4 params;      // x = radius (UE4 inverse-square falloff), yzw = 0
};

struct SpotLight {
    vec4 position;    // xyz = pos, w = enabled (1.0 / 0.0)
    vec4 direction;   // xyz = dir, w = cutOff (cos)
    vec4 color;       // xyz = color, w = outerCutOff (cos)
    vec4 params;      // x = radius, y = intensity, zw = 0
};

#define MAX_POINT_LIGHTS 16
#define MAX_SPOT_LIGHTS 8

// UBO Binding 1: Lighting Data (std140)
layout(std140) uniform LightingData {
    DirectionalLight u_DirLight;
    PointLight u_PointLights[MAX_POINT_LIGHTS];
    SpotLight u_SpotLights[MAX_SPOT_LIGHTS];
    ivec4 u_LightCounts;        // x = pointCount, y = spotCount
    vec4 u_EnvSkyColor;         // xyz = skyZenithColor, w = envIntensity
    vec4 u_EnvHorizonColor;     // xyz = horizonColor
    vec4 u_EnvGroundColor;      // xyz = groundColor
};

// PBR Material Properties
uniform vec3 u_AlbedoColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;
uniform float u_NormalScale = 1.0;
uniform float u_OcclusionStrength = 1.0;
uniform vec3 u_EmissiveColor;
uniform float u_EmissiveIntensity;
uniform int u_AlphaMode = 0;       // 0 = Opaque, 1 = Mask, 2 = Blend
uniform float u_AlphaCutoff = 0.5;
uniform vec2 u_UVTiling = vec2(1.0, 1.0);
uniform vec2 u_UVOffset = vec2(0.0, 0.0);

// Material Textures
layout(binding = 0) uniform sampler2D u_AlbedoMap;
layout(binding = 1) uniform sampler2D u_NormalMap;
layout(binding = 2) uniform sampler2D u_MetallicMap;
layout(binding = 3) uniform sampler2D u_AOMap;
layout(binding = 4) uniform sampler2D u_RoughnessMap;
layout(binding = 5) uniform sampler2D u_PlanarReflectionMap;

// Real IBL Maps
layout(binding = 6) uniform sampler2D u_BRDFLUT;
layout(binding = 7) uniform samplerCube u_IrradianceMap;
layout(binding = 8) uniform samplerCube u_PrefilterMap;

// Emissive Texture Map
layout(binding = 9) uniform sampler2D u_EmissiveMap;

// Cascaded Shadow Map (Texture2DArray Hardware PCF) & Spot Shadow Map
layout(binding = 10) uniform sampler2DArrayShadow u_CascadeShadowMap;
layout(binding = 11) uniform sampler2DShadow u_SpotShadowMap;
layout(binding = 12) uniform sampler2D u_Lightmap;

uniform int u_UseIBL;
uniform int u_UseAlbedoMap;
uniform int u_UseNormalMap;
uniform int u_UseMetallicMap;
uniform int u_UseAOMap;
uniform int u_UseRoughnessMap;
uniform int u_UseEmissiveMap;
uniform int u_UsePlanarReflection;
uniform int u_UseShadows;
uniform int u_UseSpotShadows;
uniform int u_DebugMode;
uniform vec2 u_ScreenSize;
uniform int u_UseLightmap;
uniform int u_LightmapUseTexCoord;
uniform vec2 u_LightmapScale;
uniform vec2 u_LightmapBias;

// -----------------------------------------------------------------------------
// Poisson Disk Offsets (16 Samples - Vogel spiral distribution)
// -----------------------------------------------------------------------------
const vec2 POISSON_DISK[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870),
    vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845),
    vec2( 0.97484398,  0.75648377),
    vec2( 0.44323325, -0.97511554),
    vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507),
    vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367),
    vec2( 0.14383161, -0.14100790)
);

float InterleavedGradientNoise(vec2 screenPos) {
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(screenPos, magic.xy)));
}

// -----------------------------------------------------------------------------
// 1. Physically-Based Shading Equations (PBR Cook-Torrance)
// -----------------------------------------------------------------------------
const float PI = 3.14159265358979323846;

// 2. Normal Distribution Function (Trowbridge-Reitz GGX)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float f = (NdotH2 * (a2 - 1.0) + 1.0);
    float denom = PI * f * f;
    return denom > 1e-20 ? nom / denom : 0.0;
}

// 3. Geometry Function (Smith Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, 0.0000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// 4. Fresnel Function with Roughness
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 SafeHalfVector(vec3 V, vec3 L) {
    vec3 h = V + L;
    float h2 = dot(h, h);
    return h2 > 1e-8 ? h * inversesqrt(h2) : vec3(0.0);
}

// 5. Image-Based Lighting (IBL) Atmospheric Environment Fallback
vec3 SampleEnvironmentAtmosphere(vec3 dir, float roughness) {
    float height = dir.y;
    vec3 sky;
    if (height >= 0.0) {
        float horizonFactor = pow(1.0 - height, 4.0);
        sky = mix(u_EnvSkyColor.rgb, u_EnvHorizonColor.rgb, horizonFactor);
    } else {
        float groundFactor = clamp(-height * 3.0, 0.0, 1.0);
        sky = mix(u_EnvHorizonColor.rgb, u_EnvGroundColor.rgb, groundFactor);
    }
    return sky * u_EnvSkyColor.w;
}

vec3 GetHemisphereIrradiance(vec3 normal) {
    float upFraction = normal.y * 0.5 + 0.5;
    vec3 groundToSky = mix(u_EnvGroundColor.rgb, u_EnvSkyColor.rgb, upFraction);
    return groundToSky * u_EnvSkyColor.w * PI;
}

// -----------------------------------------------------------------------------
// Advanced Cascaded Shadow Mapping (Multi-Filter, Normal Offset Bias & Blending)
// -----------------------------------------------------------------------------
float ShadowSlopeTan(vec3 normal, vec3 lightDir) {
    float NdotL = clamp(dot(normal, lightDir), 0.0, 1.0);
    float sinTheta = sqrt(max(0.0, 1.0 - NdotL * NdotL));
    // Clamp extreme grazing: tan(θ) → ∞ would peter-pan the contact shadow.
    return min(sinTheta / max(NdotL, 0.08), 8.0);
}

// Rasterized triangle plane (matches the shadow-map depth). Phong-interpolated vertex
// normals under-bias smooth/concave panels. Degenerate dFdx (1px tests) falls back.
vec3 ShadowFaceNormal(vec3 fragPos, vec3 vertexN) {
    vec3 fn = cross(dFdx(fragPos), dFdy(fragPos));
    float len2 = dot(fn, fn);
    if (len2 < 1e-12)
        return vertexN;
    fn *= inversesqrt(len2);
    return fn * sign(dot(fn, vertexN) + 1e-8);
}

// GPU Gems 3: ∂z/∂u, ∂z/∂v in shadow UV so PCF taps follow the receiver plane.
vec2 ReceiverPlaneDzDuv(vec3 dpdx, vec3 dpdy) {
    vec2 dzDuv;
    dzDuv.x = dpdy.y * dpdx.z - dpdx.y * dpdy.z;
    dzDuv.y = dpdx.x * dpdy.z - dpdy.x * dpdx.z;
    float det = dpdx.x * dpdy.y - dpdx.y * dpdy.x;
    float invDet = 1.0 / (sign(det) * max(abs(det), 1e-6));
    return clamp(dzDuv * invDet, vec2(-16.0), vec2(16.0));
}

float ShadowTapArray(sampler2DArrayShadow shadowMap, int cascadeIndex, vec2 uv, float refZ) {
    return texture(shadowMap, vec4(uv, float(cascadeIndex), refZ));
}

float ShadowTap2D(sampler2DShadow shadowMap, vec2 uv, float refZ) {
    return texture(shadowMap, vec3(uv, refZ));
}

float FilterShadowArray(sampler2DArrayShadow shadowMap, int cascadeIndex, vec3 projCoords, float refZ) {
    vec2 texelSize = vec2(1.0) / vec2(textureSize(shadowMap, 0).xy);
    vec2 dzDuv = ReceiverPlaneDzDuv(dFdx(projCoords), dFdy(projCoords));
    int filterMode = u_ShadowSettings.x;
    float shadow = 0.0;

    if (filterMode == 0) {
        return 1.0 - ShadowTapArray(shadowMap, cascadeIndex, projCoords.xy, refZ);
    } else if (filterMode == 1) {
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                vec2 uvOff = vec2(x, y) * texelSize;
                shadow += ShadowTapArray(shadowMap, cascadeIndex, projCoords.xy + uvOff, refZ + dot(dzDuv, uvOff));
            }
        }
        return 1.0 - (shadow / 9.0);
    } else if (filterMode == 2) {
        for (int x = -2; x <= 2; ++x) {
            for (int y = -2; y <= 2; ++y) {
                vec2 uvOff = vec2(x, y) * texelSize;
                shadow += ShadowTapArray(shadowMap, cascadeIndex, projCoords.xy + uvOff, refZ + dot(dzDuv, uvOff));
            }
        }
        return 1.0 - (shadow / 25.0);
    }

    float noise = InterleavedGradientNoise(gl_FragCoord.xy);
    float angle = noise * 2.0 * PI;
    mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
    float diskRadius = 1.75;
    for (int i = 0; i < 16; ++i) {
        vec2 uvOff = rot * POISSON_DISK[i] * diskRadius * texelSize;
        shadow += ShadowTapArray(shadowMap, cascadeIndex, projCoords.xy + uvOff, refZ + dot(dzDuv, uvOff));
    }
    return 1.0 - (shadow / 16.0);
}

float FilterShadow2D(sampler2DShadow shadowMap, vec3 projCoords, float refZ) {
    vec2 texelSize = vec2(1.0) / vec2(textureSize(shadowMap, 0));
    vec2 dzDuv = ReceiverPlaneDzDuv(dFdx(projCoords), dFdy(projCoords));
    int filterMode = u_ShadowSettings.x;
    float shadow = 0.0;

    if (filterMode == 0) {
        return 1.0 - ShadowTap2D(shadowMap, projCoords.xy, refZ);
    } else if (filterMode == 1) {
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                vec2 uvOff = vec2(x, y) * texelSize;
                shadow += ShadowTap2D(shadowMap, projCoords.xy + uvOff, refZ + dot(dzDuv, uvOff));
            }
        }
        return 1.0 - (shadow / 9.0);
    } else if (filterMode == 2) {
        for (int x = -2; x <= 2; ++x) {
            for (int y = -2; y <= 2; ++y) {
                vec2 uvOff = vec2(x, y) * texelSize;
                shadow += ShadowTap2D(shadowMap, projCoords.xy + uvOff, refZ + dot(dzDuv, uvOff));
            }
        }
        return 1.0 - (shadow / 25.0);
    }

    float noise = InterleavedGradientNoise(gl_FragCoord.xy);
    float angle = noise * 2.0 * PI;
    mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
    float diskRadius = 1.75;
    for (int i = 0; i < 16; ++i) {
        vec2 uvOff = rot * POISSON_DISK[i] * diskRadius * texelSize;
        shadow += ShadowTap2D(shadowMap, projCoords.xy + uvOff, refZ + dot(dzDuv, uvOff));
    }
    return 1.0 - (shadow / 16.0);
}

vec3 ApplyShadowUvNormalOffset(vec3 projCoords, mat4 lightMatrix, vec3 normal, float slopeFactor, vec2 texelSize) {
    // Vertical receivers under a near-vertical light occupy ~1 shadow texel (the caster silhouette).
    // Depth bias cannot move the sample off that sliver; a UV push along N.xy can.
    vec2 nUV = (lightMatrix * vec4(normal, 0.0)).xy;
    float nLen = length(nUV);
    if (nLen < 1e-6)
        return projCoords;
    float taps = 4.0 + 4.0 * slopeFactor;
    vec2 uvShift = (nUV / nLen) * min(texelSize * taps, vec2(0.02));
    projCoords.xy += uvShift;
    return projCoords;
}

float SampleCascadeShadowSlice(sampler2DArrayShadow shadowMap, int cascadeIndex, vec3 fragPos, vec3 vertexN, vec3 lightDir) {
    vec3 normal = ShadowFaceNormal(fragPos, vertexN);
    float NdotL = max(dot(normal, lightDir), 0.0);
    float slopeFactor = max(1.0 - NdotL, 0.0);
    float tanTheta = ShadowSlopeTan(normal, lightDir);

    float constBias  = u_ShadowParams.x;
    float slopeBias  = u_ShadowParams.y;
    float normalBias = u_ShadowParams.z;

    vec3 normalOffset = normal * (normalBias * slopeFactor);
    vec4 biasedFragPos = vec4(fragPos + normalOffset, 1.0);
    vec4 fragPosLightSpace = u_LightSpaceMatrices[cascadeIndex] * biasedFragPos;
    if (fragPosLightSpace.w <= 0.0)
        return 0.0;

    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    vec2 texelSize = vec2(1.0) / vec2(textureSize(shadowMap, 0).xy);
    projCoords = ApplyShadowUvNormalOffset(projCoords, u_LightSpaceMatrices[cascadeIndex], normal, slopeFactor, texelSize);

    if (projCoords.z > 1.0 || projCoords.z < 0.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float bias = constBias + slopeBias * tanTheta;
    return FilterShadowArray(shadowMap, cascadeIndex, projCoords, projCoords.z - bias);
}

float SampleSpotShadowMap(sampler2DShadow shadowMap, vec3 fragPos, vec3 vertexN, vec3 lightDir) {
    vec3 normal = ShadowFaceNormal(fragPos, vertexN);
    float NdotL = max(dot(normal, lightDir), 0.0);
    float slopeFactor = max(1.0 - NdotL, 0.0);
    float tanTheta = ShadowSlopeTan(normal, lightDir);

    float constBias  = u_ShadowParams.x * 1.5;
    float slopeBias  = u_ShadowParams.y * 1.5;
    float normalBias = u_ShadowParams.z;

    vec3 normalOffset = normal * (normalBias * slopeFactor);
    vec4 fragPosSpotLightSpace = u_SpotLightSpaceMatrix * vec4(fragPos + normalOffset, 1.0);
    if (fragPosSpotLightSpace.w <= 0.0)
        return 0.0;

    vec3 projCoords = fragPosSpotLightSpace.xyz / fragPosSpotLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    vec2 texelSize = vec2(1.0) / vec2(textureSize(shadowMap, 0));
    projCoords = ApplyShadowUvNormalOffset(projCoords, u_SpotLightSpaceMatrix, normal, slopeFactor, texelSize);

    if (projCoords.z > 1.0 || projCoords.z < 0.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float bias = constBias + slopeBias * tanTheta;
    return FilterShadow2D(shadowMap, projCoords, projCoords.z - bias);
}

float CalculateCascadedDirectionalShadow(vec3 fragPos, vec3 normal, vec3 lightDir, out int outCascadeIndex) {
    if (u_UseShadows == 0) {
        outCascadeIndex = 0;
        return 0.0;
    }

    // Planar view depth along camera forward axis (matches perspective frustum slices exactly)
    float depth = dot(fragPos - u_ViewPos.xyz, u_CameraForward.xyz);
    int cascadeIndex = 3;
    if (depth < u_CascadeSplits.x) {
        cascadeIndex = 0;
    } else if (depth < u_CascadeSplits.y) {
        cascadeIndex = 1;
    } else if (depth < u_CascadeSplits.z) {
        cascadeIndex = 2;
    }
    outCascadeIndex = cascadeIndex;

    float shadow = SampleCascadeShadowSlice(u_CascadeShadowMap, cascadeIndex, fragPos, normal, lightDir);

    // Cascade smooth blend transition
    float blendWidth = u_ShadowParams.w;
    if (blendWidth > 0.001 && cascadeIndex < 3) {
        float splitDist = (cascadeIndex == 0) ? u_CascadeSplits.x :
                          (cascadeIndex == 1) ? u_CascadeSplits.y : u_CascadeSplits.z;
        float blendThreshold = splitDist * (1.0 - blendWidth);
        if (depth > blendThreshold) {
            float nextShadow = SampleCascadeShadowSlice(u_CascadeShadowMap, cascadeIndex + 1, fragPos, normal, lightDir);
            float alpha = clamp((depth - blendThreshold) / max(splitDist - blendThreshold, 0.001), 0.0, 1.0);
            shadow = mix(shadow, nextShadow, alpha);
        }
    }

    // Soft fadeout at far shadow distance (split3 / cascadeSplits.w)
    if (depth > u_CascadeSplits.w) {
        float fade = clamp((depth - u_CascadeSplits.w) / 15.0, 0.0, 1.0);
        shadow = mix(shadow, 0.0, fade);
    }

    return shadow;
}

float CalculateSpotShadow(vec3 fragPos, vec3 normal, vec3 lightDir) {
    if (u_UseSpotShadows == 0) return 0.0;
    return SampleSpotShadowMap(u_SpotShadowMap, fragPos, normal, lightDir);
}

void main() {
    // 1. Texture Coordinate Transformation (Tiling & Offset)
    vec2 uv = v_TexCoord * u_UVTiling + u_UVOffset;

    // 2. Albedo & Alpha Evaluation
    vec4 albedoSample = (u_UseAlbedoMap == 1) ? texture(u_AlbedoMap, uv) : vec4(1.0);
    float alpha = albedoSample.a;

    // Alpha Masking Discard
    if (u_AlphaMode == 1 && alpha < u_AlphaCutoff) {
        discard;
    }

    // Albedo is already linear: hardware sRGB decode for Color textures, CPU material colors are linear.
    vec3 albedo = u_AlbedoColor * ((u_UseAlbedoMap == 1) ? albedoSample.rgb : vec3(1.0));

    // 3. Tangent Space Gram-Schmidt Orthogonalization & Normal Mapping
    vec3 geoN = normalize(v_Normal);
    vec3 N = geoN;
    vec3 T = normalize(v_TBN[0]);
    T = normalize(T - N * dot(N, T));
    vec3 B = normalize(v_TBN[1]);
    B = normalize(B - N * dot(N, B) - T * dot(T, B));
    mat3 TBN = mat3(T, B, N);

    if (u_UseNormalMap == 1) {
        vec3 normalSample = texture(u_NormalMap, uv).rgb;
        vec3 normalTS = normalSample * 2.0 - 1.0;
        normalTS.xy *= u_NormalScale;
        normalTS = normalize(normalTS);
        N = normalize(TBN * normalTS);
    }

    // 4. Metallic, Roughness & AO
    float metallic = u_Metallic * ((u_UseMetallicMap == 1) ? texture(u_MetallicMap, uv).r : 1.0);
    metallic = clamp(metallic, 0.0, 1.0);

    float roughness = u_Roughness * ((u_UseRoughnessMap == 1) ? texture(u_RoughnessMap, uv).r : 1.0);
    roughness = clamp(roughness, 0.04, 1.0);

    float ao = u_AO;
    if (u_UseAOMap == 1) {
        float aoSample = texture(u_AOMap, uv).r;
        ao *= mix(1.0, aoSample, u_OcclusionStrength);
    }
    ao = clamp(ao, 0.0, 1.0);

    vec3 V = normalize(u_ViewPos.xyz - v_FragPos);
    vec3 R = reflect(-V, N);

    // Base Reflectance (Dielectric 0.04 vs Metallic)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Direct lighting radiance accumulation (Lo)
    vec3 Lo = vec3(0.0);
    vec3 LoDiffuse = vec3(0.0);
    vec3 LoSpecular = vec3(0.0);
    int activeCascadeIndex = 0;
    float dirShadow = 0.0;
    float spotShadowFactor = 0.0;

    // 5.1 Directional Sunlight — single radiance = color * intensity (PBR-correct)
    if (u_DirLight.direction.w > 0.5) {
        vec3 L = normalize(-u_DirLight.direction.xyz);
        vec3 H = SafeHalfVector(V, L);
        vec3 radiance = u_DirLight.color.rgb * u_DirLight.color.w;

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3  numerator   = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3  specular    = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);
        // Shadow tests use the rasterized face normal. Interpolated/normal-mapped N jitters bias (acne).
        vec3 shadowN = ShadowFaceNormal(v_FragPos, geoN);
        dirShadow = CalculateCascadedDirectionalShadow(v_FragPos, shadowN, L, activeCascadeIndex);
        dirShadow *= smoothstep(0.0, 0.22, max(dot(shadowN, L), 0.0));

        vec3 vis = radiance * NdotL * (1.0 - dirShadow);
        LoDiffuse += (kD * albedo / PI) * vis;
        LoSpecular += specular * vis;
    }

    // 5.2 Point Lights Loop
    for (int i = 0; i < u_LightCounts.x && i < MAX_POINT_LIGHTS; ++i) {
        if (u_PointLights[i].position.w < 0.5) continue;

        vec3 lightPos = u_PointLights[i].position.xyz;
        vec3 L = normalize(lightPos - v_FragPos);
        vec3 H = SafeHalfVector(V, L);

        float distance = length(lightPos - v_FragPos);
        float radius   = u_PointLights[i].params.x;

        // Unreal Engine 4 Smooth Windowed Inverse-Square Falloff
        float distanceRatio = distance / max(radius, 0.0001);
        float d4 = distanceRatio * distanceRatio * distanceRatio * distanceRatio;
        float windowFactor = clamp(1.0 - d4, 0.0, 1.0);
        float attenuation = (windowFactor * windowFactor) / (distance * distance + 1.0);

        vec3 radiance = u_PointLights[i].color.rgb * u_PointLights[i].color.w * attenuation;

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3  numerator   = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3  specular    = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);
        vec3 vis = radiance * NdotL;
        LoDiffuse += (kD * albedo / PI) * vis;
        LoSpecular += specular * vis;
    }

    // 5.3 Spot Lights Loop
    for (int i = 0; i < u_LightCounts.y && i < MAX_SPOT_LIGHTS; ++i) {
        if (u_SpotLights[i].position.w < 0.5) continue;

        vec3 lightPos = u_SpotLights[i].position.xyz;
        vec3 L = normalize(lightPos - v_FragPos);
        vec3 H = SafeHalfVector(V, L);

        float theta = dot(L, normalize(-u_SpotLights[i].direction.xyz));
        float innerCos = u_SpotLights[i].direction.w;
        float outerCos = u_SpotLights[i].color.w;

        if (theta > outerCos) {
            float epsilon = innerCos - outerCos;
            float spotIntensity = clamp((theta - outerCos) / max(epsilon, 0.0001), 0.0, 1.0);
            spotIntensity = spotIntensity * spotIntensity * (3.0 - 2.0 * spotIntensity); // Smoothstep

            float distance = length(lightPos - v_FragPos);
            float radius   = u_SpotLights[i].params.x;
            float distRatio = distance / max(radius, 0.0001);
            float d4 = distRatio * distRatio * distRatio * distRatio;
            float windowFactor = clamp(1.0 - d4, 0.0, 1.0);
            float attenuation = (windowFactor * windowFactor) / (distance * distance + 1.0);

            vec3 radiance = u_SpotLights[i].color.rgb * u_SpotLights[i].params.y * attenuation * spotIntensity;

            float NDF = DistributionGGX(N, H, roughness);
            float G   = GeometrySmith(N, V, L, roughness);
            vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

            vec3  numerator   = NDF * G * F;
            float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
            vec3  specular    = numerator / denominator;

            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;

            float NdotL = max(dot(N, L), 0.0);
            vec3 spotShadowN = ShadowFaceNormal(v_FragPos, geoN);
            float spotShadow = (i == u_ShadowSettings.y) ? CalculateSpotShadow(v_FragPos, spotShadowN, L) : 0.0;
            spotShadow *= smoothstep(0.0, 0.22, max(dot(spotShadowN, L), 0.0));
            if (i == u_ShadowSettings.y)
                spotShadowFactor = 1.0 - spotShadow;

            vec3 vis = radiance * NdotL * (1.0 - spotShadow);
            LoDiffuse += (kD * albedo / PI) * vis;
            LoSpecular += specular * vis;
        }
    }

    Lo = LoDiffuse + LoSpecular;

    // 6. Image-Based Lighting (IBL)
    float NdotV = max(dot(N, V), 0.0);
    vec3 F_IBL = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kS_IBL = F_IBL;
    vec3 kD_IBL = 1.0 - kS_IBL;
    kD_IBL *= 1.0 - metallic;

    vec3 diffuseIBL;
    vec3 reflectionLi;
    vec2 envBRDF;

    if (u_UseIBL == 1) {
        vec3 irradiance = texture(u_IrradianceMap, N).rgb * u_EnvSkyColor.w;
        diffuseIBL = irradiance * albedo / PI;

        const float MAX_REFLECTION_LOD = 4.0;
        reflectionLi = textureLod(u_PrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb * u_EnvSkyColor.w;
        envBRDF = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
    } else {
        vec3 irradiance = GetHemisphereIrradiance(N);
        diffuseIBL = irradiance * albedo / PI;

        reflectionLi = SampleEnvironmentAtmosphere(R, roughness);
        envBRDF = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
    }

    // 7. Real-Time Planar Reflections (Karis split-sum)
    // Planar FBO is linear HDR scene radiance Li along an approximate mirror ray.
    // Blur Li with roughness via the planar mip chain (same LOD convention as prefilter: roughness * 4).
    // Then apply the split-sum BRDF factor once — do NOT replace specularIBL with planar*F alone
    // (that skipped the LUT scale/bias and kept sharp emissive peaks at full intensity).
    if (u_UsePlanarReflection == 1) {
        vec2 screenUV = gl_FragCoord.xy / u_ScreenSize;
        vec2 perturbedUV = clamp(screenUV + vec2(N.x, N.z) * 0.03 * (1.0 - roughness), 0.001, 0.999);

        const float MAX_PLANAR_LOD = 4.0;
        float planarLod = roughness * MAX_PLANAR_LOD;
        vec3 planarLi = textureLod(u_PlanarReflectionMap, perturbedUV, planarLod).rgb;

        // Wide GGX lobes cannot be represented by a single planar ray — fade toward IBL with roughness.
        float reflectStrength = clamp(1.0 - roughness * 1.1, 0.0, 1.0);
        reflectionLi = mix(reflectionLi, planarLi, reflectStrength);
    }

    vec3 specularIBL = reflectionLi * (F_IBL * envBRDF.x + envBRDF.y);

    // Indirect ambient radiance occluded by AO
    vec3 ambient = (kD_IBL * diffuseIBL + specularIBL) * ao;

    // Lightmap stores baked diffuse irradiance E = ∫ Li max(N·wi,0) dω
    // Lo_diffuse = albedo / π * E
    vec3 lightmapIrradiance = vec3(0.0);
    vec2 lightmapUVSample = vec2(0.0);
    vec3 bakedLighting = vec3(0.0);
    if (u_UseLightmap == 1) {
        lightmapUVSample = (u_LightmapUseTexCoord == 1) ? v_TexCoord : v_LightmapUV;
        lightmapUVSample = lightmapUVSample * u_LightmapScale + u_LightmapBias;
        lightmapIrradiance = texture(u_Lightmap, lightmapUVSample).rgb;
        bakedLighting = kD_IBL * (albedo / PI) * lightmapIrradiance;
        ambient = specularIBL * ao;
    }

    vec3 emissiveMapSample = (u_UseEmissiveMap == 1) ? texture(u_EmissiveMap, uv).rgb : vec3(1.0);
    vec3 emissive = u_EmissiveColor * u_EmissiveIntensity * emissiveMapSample;

    // Output pure linear HDR color (Post-Processing Pass handles Tonemapping & Gamma)
    vec3 hdrColor = ambient + Lo + bakedLighting + emissive;

    // Forensic Debug Views
    if (u_DebugMode == 1 || u_DebugMode == 2) {
        FragColor = vec4(textureLod(u_PrefilterMap, R, 0.0).rgb, 1.0);
        return;
    } else if (u_DebugMode == 3) {
        FragColor = vec4(textureLod(u_PrefilterMap, R, 1.0).rgb, 1.0);
        return;
    } else if (u_DebugMode == 4) {
        FragColor = vec4(textureLod(u_PrefilterMap, R, 2.0).rgb, 1.0);
        return;
    } else if (u_DebugMode == 5) {
        FragColor = vec4(textureLod(u_PrefilterMap, R, 3.0).rgb, 1.0);
        return;
    } else if (u_DebugMode == 6) {
        FragColor = vec4(textureLod(u_PrefilterMap, R, 4.0).rgb, 1.0);
        return;
    } else if (u_DebugMode == 7) {
        FragColor = vec4(texture(u_IrradianceMap, N).rgb, 1.0);
        return;
    } else if (u_DebugMode == 8) {
        FragColor = vec4(texture(u_BRDFLUT, vec2(NdotV, roughness)).rg, 0.0, 1.0);
        return;
    } else if (u_DebugMode == 9) {
        FragColor = vec4(specularIBL, 1.0);
        return;
    } else if (u_DebugMode == 10) {
        FragColor = vec4(Lo, 1.0);
        return;
    } else if (u_DebugMode == 11) {
        FragColor = vec4(N * 0.5 + 0.5, 1.0);
        return;
    } else if (u_DebugMode == 12) {
        FragColor = vec4(R * 0.5 + 0.5, 1.0);
        return;
    } else if (u_DebugMode == 13) {
        FragColor = vec4(texture(u_PlanarReflectionMap, gl_FragCoord.xy / u_ScreenSize).rgb, 1.0);
        return;
    } else if (u_DebugMode == 14) {
        FragColor = vec4(albedo, 1.0);
        return;
    } else if (u_DebugMode == 15) {
        FragColor = vec4(vec3(metallic), 1.0);
        return;
    } else if (u_DebugMode == 16) {
        FragColor = vec4(vec3(roughness), 1.0);
        return;
    } else if (u_DebugMode == 17) {
        FragColor = vec4(N * 0.5 + 0.5, 1.0);
        return;
    } else if (u_DebugMode == 18) {
        FragColor = vec4(vec3(ao), 1.0);
        return;
    } else if (u_DebugMode == 19) {
        FragColor = vec4(emissive, 1.0);
        return;
    } else if (u_DebugMode == 20) {
        FragColor = vec4(normalize(T) * 0.5 + 0.5, 1.0);
        return;
    } else if (u_DebugMode == 21) {
        FragColor = vec4(normalize(B) * 0.5 + 0.5, 1.0);
        return;
    } else if (u_DebugMode == 22) {
        FragColor = vec4(fract(uv), 0.0, 1.0);
        return;
    } else if (u_DebugMode == 23) {
        vec3 dirSunL = (u_DirLight.direction.w > 0.5) ? normalize(-u_DirLight.direction.xyz) : vec3(0.0, 1.0, 0.0);
        FragColor = vec4(vec3(max(dot(N, dirSunL), 0.0)), 1.0);
        return;
    } else if (u_DebugMode == 24) {
        // Direct Shadow Factor (1.0 = lit, 0.0 = occluded)
        FragColor = vec4(vec3(1.0 - dirShadow), 1.0);
        return;
    } else if (u_DebugMode == 25) {
        // Cascade Index False-Color (0: Red, 1: Green, 2: Blue, 3: Yellow)
        vec3 cascadeColors[4] = vec3[](
            vec3(1.0, 0.15, 0.15),
            vec3(0.15, 0.90, 0.20),
            vec3(0.20, 0.40, 1.00),
            vec3(1.00, 0.90, 0.10)
        );
        FragColor = vec4(cascadeColors[clamp(activeCascadeIndex, 0, 3)], 1.0);
        return;
    } else if (u_DebugMode == 26) {
        FragColor = vec4(vec3(spotShadowFactor), 1.0);
        return;
    } else if (u_DebugMode == 27) {
        // Cascade 0 Depth Map
        vec4 p0 = u_LightSpaceMatrices[0] * vec4(v_FragPos, 1.0);
        vec3 c0 = p0.xyz / p0.w * 0.5 + 0.5;
        FragColor = vec4(vec3(c0.z), 1.0);
        return;
    } else if (u_DebugMode == 28) {
        // Cascade 1 Depth Map
        vec4 p1 = u_LightSpaceMatrices[1] * vec4(v_FragPos, 1.0);
        vec3 c1 = p1.xyz / p1.w * 0.5 + 0.5;
        FragColor = vec4(vec3(c1.z), 1.0);
        return;
    } else if (u_DebugMode == 29) {
        // Cascade 2 Depth Map
        vec4 p2 = u_LightSpaceMatrices[2] * vec4(v_FragPos, 1.0);
        vec3 c2 = p2.xyz / p2.w * 0.5 + 0.5;
        FragColor = vec4(vec3(c2.z), 1.0);
        return;
    } else if (u_DebugMode == 30) {
        // Cascade 3 Depth Map
        vec4 p3 = u_LightSpaceMatrices[3] * vec4(v_FragPos, 1.0);
        vec3 c3 = p3.xyz / p3.w * 0.5 + 0.5;
        FragColor = vec4(vec3(c3.z), 1.0);
        return;
    } else if (u_DebugMode == 31) {
        // Baked lighting only (albedo * lightmap irradiance)
        FragColor = vec4(bakedLighting, 1.0);
        return;
    } else if (u_DebugMode == 32) {
        // Raw lightmap irradiance (no material)
        FragColor = vec4(lightmapIrradiance, 1.0);
        return;
    } else if (u_DebugMode == 33) {
        // Lightmap atlas UV (chart space after scale/bias)
        FragColor = vec4(fract(lightmapUVSample), u_UseLightmap == 1 ? 0.0 : 1.0, 1.0);
        return;
    } else if (u_DebugMode == 34) {
        FragColor = vec4(Lo + bakedLighting, 1.0);
        return;
    } else if (u_DebugMode == 35) {
        FragColor = vec4(diffuseIBL, 1.0);
        return;
    } else if (u_DebugMode == 36) {
        FragColor = vec4(Lo, 1.0);
        return;
    } else if (u_DebugMode == 37) {
        FragColor = vec4(hdrColor, 1.0);
        return;
    } else if (u_DebugMode == 38) {
        FragColor = vec4(LoDiffuse, 1.0);
        return;
    } else if (u_DebugMode == 39) {
        FragColor = vec4(LoSpecular, 1.0);
        return;
    }

    FragColor = vec4(hdrColor, alpha);
}
