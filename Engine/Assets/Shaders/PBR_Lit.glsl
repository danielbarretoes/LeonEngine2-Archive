#type vertex
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;
layout(location = 5) in vec3 aColor;

out vec3 v_FragPos;
out vec3 v_Normal;
out vec2 v_TexCoord;
out vec3 v_Color;
out mat3 v_TBN;

// UBO Binding 0: Camera Data (std140)
layout(std140) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_LightSpaceMatrices[4];
    mat4 u_SpotLightSpaceMatrix;
    vec4 u_ViewPos;
    vec4 u_CascadeSplits;
};

uniform mat4 u_Model;
// Normal matrix pre-computed CPU-side to avoid per-vertex GPU inverse (audit fix MEDIO-05)
uniform mat3 u_NormalMatrix;

void main() {
    vec4 worldPos = u_Model * vec4(aPos, 1.0);
    v_FragPos = worldPos.xyz;

    // Use CPU-pre-computed normal matrix (no inverse on GPU)
    vec3 N = normalize(u_NormalMatrix * aNormal);
    vec3 T = normalize(u_NormalMatrix * aTangent);
    T = normalize(T - dot(T, N) * N);
    vec3 B = normalize(u_NormalMatrix * aBitangent);
    B = normalize(B - dot(B, N) * N - dot(B, T) * T);
    v_TBN = mat3(T, B, N);
    v_Normal = N;

    v_TexCoord = aTexCoord;
    v_Color    = aColor;

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

// UBO Binding 0: Camera Data (std140)
layout(std140) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_LightSpaceMatrices[4];
    mat4 u_SpotLightSpaceMatrix;
    vec4 u_ViewPos;
    vec4 u_CascadeSplits;
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

uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicMap;
uniform sampler2D u_AOMap;
uniform sampler2D u_RoughnessMap;

// Cascaded Shadow Maps (CSM) Samplers
uniform sampler2DShadow u_ShadowMap0;
uniform sampler2DShadow u_ShadowMap1;
uniform sampler2DShadow u_ShadowMap2;
uniform sampler2DShadow u_SpotShadowMap;
uniform sampler2D u_PlanarReflectionMap;

// Real IBL Maps
uniform samplerCube u_IrradianceMap;
uniform samplerCube u_PrefilterMap;
uniform sampler2D u_BRDFLUT;
uniform int u_UseIBL;

uniform int u_UseAlbedoMap;
uniform int u_UseNormalMap;
uniform int u_UseMetallicMap;
uniform int u_UseAOMap;
uniform int u_UseRoughnessMap;
uniform int u_UseShadows;
uniform int u_UseSpotShadows;
uniform int u_UsePlanarReflection;
uniform vec2 u_ScreenSize;

const float PI = 3.14159265358979323846;

// 2. Normal Distribution Function (Trowbridge-Reitz GGX)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001);
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

// 5. Image-Based Lighting (IBL) Atmospheric Environment Fallback
vec3 SampleEnvironmentAtmosphere(vec3 dir, float roughness) {
    float height = dir.y;
    vec3 sky;
    if (height >= 0.0) {
        float horizonFactor = pow(1.0 - height, 4.0);
        sky = mix(u_EnvSkyColor.rgb, u_EnvHorizonColor.rgb, horizonFactor);
    } else {
        float groundFactor = clamp(-height * 2.5, 0.0, 1.0);
        sky = mix(u_EnvHorizonColor.rgb, u_EnvGroundColor.rgb, groundFactor);
    }

    if (u_DirLight.direction.w > 0.5) {
        vec3 sunDir = normalize(-u_DirLight.direction.xyz);
        float cosTheta = max(dot(dir, sunDir), 0.0);
        float sunExponent = mix(256.0, 8.0, roughness);
        float sunHalo = pow(cosTheta, sunExponent) * (1.0 - roughness * 0.5) * 3.5;
        sky += u_DirLight.color.rgb * sunHalo;
    }

    vec3 averageEnv = mix(u_EnvSkyColor.rgb, u_EnvHorizonColor.rgb, 0.5);
    return mix(sky, averageEnv, clamp(roughness * 0.7, 0.0, 1.0)) * u_EnvSkyColor.w;
}

vec3 GetHemisphereIrradiance(vec3 N) {
    float upFactor = N.y * 0.5 + 0.5;
    return mix(u_EnvGroundColor.rgb, u_EnvSkyColor.rgb, upFactor) * u_EnvSkyColor.w;
}

// 6. Hardware PCF Shadow Calculation per Shadow Map
float SampleShadowMap(sampler2DShadow shadowMap, vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float bias = max(0.0012 * (1.0 - dot(normal, lightDir)), 0.0002);
    float currentDepth = projCoords.z - bias;

    float shadow = 0.0;
    vec2 texelSize = vec2(1.0) / vec2(textureSize(shadowMap, 0));
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            shadow += texture(shadowMap, vec3(projCoords.xy + vec2(x, y) * texelSize, currentDepth));
        }
    }
    return 1.0 - (shadow / 9.0);
}

float CalculateCascadedDirectionalShadow(vec3 fragPos, vec3 normal, vec3 lightDir) {
    if (u_UseShadows == 0) return 0.0;

    float dist = length(u_ViewPos.xyz - fragPos);
    int cascadeIndex = 2;
    if (dist < u_CascadeSplits.x) {
        cascadeIndex = 0;
    } else if (dist < u_CascadeSplits.y) {
        cascadeIndex = 1;
    }

    vec4 fragPosLightSpace = u_LightSpaceMatrices[cascadeIndex] * vec4(fragPos, 1.0);
    float shadow = 0.0;
    if (cascadeIndex == 0) {
        shadow = SampleShadowMap(u_ShadowMap0, fragPosLightSpace, normal, lightDir);
    } else if (cascadeIndex == 1) {
        shadow = SampleShadowMap(u_ShadowMap1, fragPosLightSpace, normal, lightDir);
    } else {
        shadow = SampleShadowMap(u_ShadowMap2, fragPosLightSpace, normal, lightDir);
    }

    // Soft fadeout at far shadow distance
    if (dist > u_CascadeSplits.z) {
        float fade = clamp((dist - u_CascadeSplits.z) / 15.0, 0.0, 1.0);
        shadow = mix(shadow, 0.0, fade);
    }

    return shadow;
}

float CalculateSpotShadow(vec3 fragPos, vec3 normal, vec3 lightDir) {
    if (u_UseSpotShadows == 0) return 0.0;
    vec4 fragPosSpotLightSpace = u_SpotLightSpaceMatrix * vec4(fragPos, 1.0);
    return SampleShadowMap(u_SpotShadowMap, fragPosSpotLightSpace, normal, lightDir);
}

void main() {
    // Albedo
    vec3 albedo = u_AlbedoColor;
    if (u_UseAlbedoMap == 1) {
        albedo *= pow(texture(u_AlbedoMap, v_TexCoord).rgb, vec3(2.2));
    }

    // Normal Mapping with orthogonal TBN
    vec3 N = normalize(v_Normal);
    if (u_UseNormalMap == 1) {
        vec3 normalMap = texture(u_NormalMap, v_TexCoord).rgb;
        normalMap = normalize(normalMap * 2.0 - 1.0);
        N = normalize(v_TBN * normalMap);
    }

    // Metallic & Roughness
    float metallic = u_Metallic;
    if (u_UseMetallicMap == 1) {
        metallic = texture(u_MetallicMap, v_TexCoord).r;
    }

    float roughness = u_Roughness;
    if (u_UseRoughnessMap == 1) {
        roughness = texture(u_RoughnessMap, v_TexCoord).r;
    }
    roughness = clamp(roughness, 0.04, 1.0);

    // Ambient Occlusion (AO Map & Scalar)
    float ao = u_AO;
    if (u_UseAOMap == 1) {
        ao *= texture(u_AOMap, v_TexCoord).r;
    }
    ao = clamp(ao, 0.0, 1.0);

    vec3 V = normalize(u_ViewPos.xyz - v_FragPos);
    vec3 R = reflect(-V, N);

    // Base Reflectance (Dielectric 0.04 vs Metallic)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Direct lighting radiance accumulation (Lo)
    vec3 Lo = vec3(0.0);

    // 1. Directional Sunlight — single radiance = color * intensity (PBR-correct)
    if (u_DirLight.direction.w > 0.5) {
        vec3 L = normalize(-u_DirLight.direction.xyz);
        vec3 H = normalize(V + L);
        // Radiance: color * intensity (one physical quantity, no Phong split)
        vec3 radiance = u_DirLight.color.rgb * u_DirLight.color.w;

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3  numerator   = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3  specular    = numerator / denominator;

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);
        float shadow = CalculateCascadedDirectionalShadow(v_FragPos, N, L);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);
    }

    // 2. Point Lights — UE4 inverse-square radius falloff (audit fix ALTO-06)
    int pointCount = min(u_LightCounts.x, MAX_POINT_LIGHTS);
    for (int i = 0; i < pointCount; ++i) {
        if (u_PointLights[i].position.w < 0.5) continue;

        vec3  L        = normalize(u_PointLights[i].position.xyz - v_FragPos);
        vec3  H        = normalize(V + L);
        float distance = length(u_PointLights[i].position.xyz - v_FragPos);

        // UE4/Filament inverse-square falloff with radius — params.x IS the radius (correct field)
        float radius  = max(u_PointLights[i].params.x, 0.001);
        float distSq  = distance * distance;
        float factor  = clamp(1.0 - (distSq * distSq) / (radius * radius * radius * radius), 0.0, 1.0);
        float attenuation = (factor * factor) / (distSq + 1.0);

        // Radiance = color * intensity * attenuation
        vec3 radiance = u_PointLights[i].color.rgb * u_PointLights[i].color.w * attenuation;

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3  numerator   = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3  specular    = numerator / denominator;

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // 3. Spot Lights — same radius falloff model as point lights
    int spotCount = min(u_LightCounts.y, MAX_SPOT_LIGHTS);
    for (int i = 0; i < spotCount; ++i) {
        if (u_SpotLights[i].position.w < 0.5) continue;

        vec3  L        = normalize(u_SpotLights[i].position.xyz - v_FragPos);
        vec3  H        = normalize(V + L);
        float distance = length(u_SpotLights[i].position.xyz - v_FragPos);

        // params.x = radius, params.y = intensity
        float radius  = max(u_SpotLights[i].params.x, 0.001);
        float distSq  = distance * distance;
        float factor  = clamp(1.0 - (distSq * distSq) / (radius * radius * radius * radius), 0.0, 1.0);
        float attenuation = (factor * factor) / (distSq + 1.0);

        float theta       = dot(L, normalize(-u_SpotLights[i].direction.xyz));
        float cutOff      = u_SpotLights[i].direction.w;
        float outerCutOff = u_SpotLights[i].color.w;
        float epsilon     = cutOff - outerCutOff;
        float spotFactor  = clamp((theta - outerCutOff) / max(epsilon, 0.0001), 0.0, 1.0);

        float spotShadow = (i == 0) ? CalculateSpotShadow(v_FragPos, N, L) : 0.0;
        vec3 radiance = u_SpotLights[i].color.rgb * u_SpotLights[i].params.y
                        * attenuation * spotFactor * (1.0 - spotShadow);

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3  numerator   = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3  specular    = numerator / denominator;

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // ========================================================
    // 4. Image-Based Lighting (IBL) Indirect Lighting Pass with AO
    // ========================================================
    float NdotV = max(dot(N, V), 0.0);
    vec3 F_IBL = FresnelSchlickRoughness(NdotV, F0, roughness);
    
    vec3 kS_IBL = F_IBL;
    vec3 kD_IBL = (1.0 - kS_IBL) * (1.0 - metallic);
    
    vec3 diffuseIBL;
    vec3 specularIBL;
    vec2 envBRDF;

    if (u_UseIBL == 1) {
        // Real Image-Based Lighting via pre-convolved irradiance, pre-filtered environment maps and 2D BRDF LUT
        vec3 irradiance = texture(u_IrradianceMap, N).rgb * u_EnvSkyColor.w;
        diffuseIBL = irradiance * albedo;

        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefilteredColor = textureLod(u_PrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb * u_EnvSkyColor.w;
        envBRDF = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
        specularIBL = prefilteredColor * (F_IBL * envBRDF.x + envBRDF.y);
    } else {
        // Fallback: Analytical Procedural Atmosphere IBL
        vec3 irradiance = GetHemisphereIrradiance(N);
        diffuseIBL = irradiance * albedo;

        vec3 prefilteredColor = SampleEnvironmentAtmosphere(R, roughness);
        envBRDF = vec2(1.0 - roughness, roughness * 0.5);
        specularIBL = prefilteredColor * (F_IBL * envBRDF.x + envBRDF.y);
    }

    // 4.3 Real-Time Planar Reflections (Reflecting Scene Objects in Floor)
    if (u_UsePlanarReflection == 1 && N.y > 0.5) {
        vec2 screenUV = gl_FragCoord.xy / u_ScreenSize;
        vec2 perturbedUV = screenUV + vec2(N.x, N.z) * 0.03 * (1.0 - roughness);
        perturbedUV = clamp(perturbedUV, 0.001, 0.999);
        vec3 planarColor = texture(u_PlanarReflectionMap, perturbedUV).rgb;
        
        float reflectStrength = clamp((1.0 - roughness * 1.1), 0.0, 1.0) * (0.7 + 0.3 * F_IBL.r);
        specularIBL = mix(specularIBL, planarColor * (F_IBL * envBRDF.x + envBRDF.y), reflectStrength);
    }

    // Occlude indirect ambient radiance by AO
    vec3 ambient = (kD_IBL * diffuseIBL + specularIBL) * ao;

    // Output pure linear HDR color (Post-Processing Pass handles Tonemapping & Gamma)
    vec3 hdrColor = ambient + Lo;
    FragColor = vec4(hdrColor, 1.0);
}
