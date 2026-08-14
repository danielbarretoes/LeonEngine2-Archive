#type vertex
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;
layout(location = 5) in vec3 aColor;

out vec3 v_FragPos;
out vec3 v_Normal;
out vec2 v_TexCoord;
out vec4 v_FragPosLightSpace;
out vec3 v_Color;
out mat3 v_TBN;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform mat4 u_LightSpaceMatrix;

void main() {
    vec4 worldPos = u_Model * vec4(aPos, 1.0);
    v_FragPos = worldPos.xyz;
    
    // Normal matrix for non-uniform scaling
    mat3 normalMatrix = transpose(inverse(mat3(u_Model)));
    vec3 N = normalize(normalMatrix * aNormal);
    vec3 T = normalize(normalMatrix * aTangent);
    T = normalize(T - dot(T, N) * N);
    vec3 B = normalize(normalMatrix * aBitangent);
    B = normalize(B - dot(B, N) * N - dot(B, T) * T);
    v_TBN = mat3(T, B, N);
    v_Normal = N;

    v_TexCoord = aTexCoord;
    v_Color = aColor;
    v_FragPosLightSpace = u_LightSpaceMatrix * worldPos;

    gl_Position = u_ViewProjection * worldPos;
}

#type fragment
#version 330 core

layout(location = 0) out vec4 FragColor;

in vec3 v_FragPos;
in vec3 v_Normal;
in vec2 v_TexCoord;
in vec4 v_FragPosLightSpace;
in vec3 v_Color;
in mat3 v_TBN;

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
uniform sampler2D u_ShadowMap;
uniform sampler2D u_PlanarReflectionMap;

uniform int u_UseAlbedoMap;
uniform int u_UseNormalMap;
uniform int u_UseMetallicMap;
uniform int u_UseAOMap;
uniform int u_UseRoughnessMap;
uniform int u_UseShadows;
uniform int u_UsePlanarReflection;
uniform vec2 u_ScreenSize;

// Environment / IBL Atmosphere Uniforms
uniform vec3 u_EnvSkyColor;
uniform vec3 u_EnvHorizonColor;
uniform vec3 u_EnvGroundColor;
uniform float u_EnvIntensity;

// Direct Lighting Subsystem Uniforms
struct DirectionalLight {
    int enabled;
    vec3 direction;
    vec3 color;
    float ambientIntensity;
    float diffuseIntensity;
    float specularIntensity;
};

struct PointLight {
    int enabled;
    vec3 position;
    vec3 color;
    float constant;
    float linear;
    float quadratic;
    float ambientIntensity;
    float diffuseIntensity;
    float specularIntensity;
};

struct SpotLight {
    int enabled;
    vec3 position;
    vec3 direction;
    vec3 color;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
    float ambientIntensity;
    float diffuseIntensity;
    float specularIntensity;
};

#define MAX_POINT_LIGHTS 16
#define MAX_SPOT_LIGHTS 8

uniform DirectionalLight u_DirLight;
uniform int u_PointLightCount;
uniform PointLight u_PointLights[MAX_POINT_LIGHTS];
uniform int u_SpotLightCount;
uniform SpotLight u_SpotLights[MAX_SPOT_LIGHTS];

uniform vec3 u_ViewPos;

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

// 5. Image-Based Lighting (IBL) Atmospheric Environment Functions
vec3 SampleEnvironmentAtmosphere(vec3 dir, float roughness) {
    float height = dir.y;
    vec3 sky;
    if (height >= 0.0) {
        float horizonFactor = pow(1.0 - height, 4.0);
        sky = mix(u_EnvSkyColor, u_EnvHorizonColor, horizonFactor);
    } else {
        float groundFactor = clamp(-height * 2.5, 0.0, 1.0);
        sky = mix(u_EnvHorizonColor, u_EnvGroundColor, groundFactor);
    }

    // Solar specular reflection in sky
    if (u_DirLight.enabled == 1) {
        vec3 sunDir = normalize(-u_DirLight.direction);
        float cosTheta = max(dot(dir, sunDir), 0.0);
        float sunExponent = mix(256.0, 8.0, roughness);
        float sunHalo = pow(cosTheta, sunExponent) * (1.0 - roughness * 0.5) * 3.5;
        sky += u_DirLight.color * sunHalo;
    }

    // Roughness blur effect on specular environment reflection
    vec3 averageEnv = mix(u_EnvSkyColor, u_EnvHorizonColor, 0.5);
    return mix(sky, averageEnv, clamp(roughness * 0.7, 0.0, 1.0)) * u_EnvIntensity;
}

vec3 GetHemisphereIrradiance(vec3 N) {
    // Ambient diffuse irradiance from atmospheric hemisphere
    float upFactor = N.y * 0.5 + 0.5;
    vec3 irradiance = mix(u_EnvGroundColor, u_EnvSkyColor, upFactor) * u_EnvIntensity;
    return irradiance;
}

// 6. Shadow Calculation with 3x3 PCF Kernel
float CalculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    if (u_UseShadows == 0) return 0.0;

    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float bias = max(0.003 * (1.0 - dot(normal, lightDir)), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(u_ShadowMap, 0);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(u_ShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }

    return shadow / 9.0;
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

    vec3 V = normalize(u_ViewPos - v_FragPos);
    vec3 R = reflect(-V, N);

    // Base Reflectance (Dielectric 0.04 vs Metallic)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Direct lighting radiance accumulation (Lo)
    vec3 Lo = vec3(0.0);

    // 1. Directional Sunlight with Shadows
    if (u_DirLight.enabled == 1) {
        vec3 L = normalize(-u_DirLight.direction);
        vec3 H = normalize(V + L);
        vec3 radiance = u_DirLight.color * u_DirLight.diffuseIntensity;

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = (numerator / denominator) * u_DirLight.specularIntensity;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);
        float shadow = CalculateShadow(v_FragPosLightSpace, N, L);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);
        Lo += u_DirLight.color * u_DirLight.ambientIntensity * albedo * ao;
    }

    // 2. Point Lights (Multi-Light Loop)
    int pointCount = min(u_PointLightCount, MAX_POINT_LIGHTS);
    for (int i = 0; i < pointCount; ++i) {
        if (u_PointLights[i].enabled == 0) continue;

        vec3 L = normalize(u_PointLights[i].position - v_FragPos);
        vec3 H = normalize(V + L);
        float distance = length(u_PointLights[i].position - v_FragPos);
        float attenuation = 1.0 / (u_PointLights[i].constant + u_PointLights[i].linear * distance + u_PointLights[i].quadratic * (distance * distance));
        vec3 radiance = u_PointLights[i].color * u_PointLights[i].diffuseIntensity * attenuation;

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = (numerator / denominator) * u_PointLights[i].specularIntensity;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
        Lo += u_PointLights[i].color * u_PointLights[i].ambientIntensity * attenuation * albedo * ao;
    }

    // 3. Spotlights (Multi-Light Loop)
    int spotCount = min(u_SpotLightCount, MAX_SPOT_LIGHTS);
    for (int i = 0; i < spotCount; ++i) {
        if (u_SpotLights[i].enabled == 0) continue;

        vec3 L = normalize(u_SpotLights[i].position - v_FragPos);
        vec3 H = normalize(V + L);
        float distance = length(u_SpotLights[i].position - v_FragPos);
        float attenuation = 1.0 / (u_SpotLights[i].constant + u_SpotLights[i].linear * distance + u_SpotLights[i].quadratic * (distance * distance));

        float theta = dot(L, normalize(-u_SpotLights[i].direction));
        float epsilon = u_SpotLights[i].cutOff - u_SpotLights[i].outerCutOff;
        float spotIntensity = clamp((theta - u_SpotLights[i].outerCutOff) / max(epsilon, 0.0001), 0.0, 1.0);

        vec3 radiance = u_SpotLights[i].color * u_SpotLights[i].diffuseIntensity * attenuation * spotIntensity;

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = (numerator / denominator) * u_SpotLights[i].specularIntensity;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
        Lo += u_SpotLights[i].color * u_SpotLights[i].ambientIntensity * attenuation * spotIntensity * albedo * ao;
    }

    // ========================================================
    // 4. Image-Based Lighting (IBL) Indirect Lighting Pass with AO
    // ========================================================
    float NdotV = max(dot(N, V), 0.0);
    vec3 F_IBL = FresnelSchlickRoughness(NdotV, F0, roughness);
    
    vec3 kS_IBL = F_IBL;
    vec3 kD_IBL = (1.0 - kS_IBL) * (1.0 - metallic);
    
    // 4.1 Indirect Diffuse (Hemisphere Irradiance modulated by AO)
    vec3 irradiance = GetHemisphereIrradiance(N);
    vec3 diffuseIBL = irradiance * albedo;

    // 4.2 Indirect Specular (Environment Reflection based on Roughness)
    vec3 prefilteredColor = SampleEnvironmentAtmosphere(R, roughness);
    vec2 envBRDF = vec2(1.0 - roughness, roughness * 0.5);
    vec3 specularIBL = prefilteredColor * (F_IBL * envBRDF.x + envBRDF.y);

    // 4.3 Real-Time Planar Reflections (Reflecting Scene Objects in Floor)
    if (u_UsePlanarReflection == 1 && N.y > 0.5) {
        vec2 screenUV = gl_FragCoord.xy / u_ScreenSize;
        // Perturb reflection screen UV with normal map perturbation in XZ
        vec2 perturbedUV = screenUV + vec2(N.x, N.z) * 0.03 * (1.0 - roughness);
        perturbedUV = clamp(perturbedUV, 0.001, 0.999);
        vec3 planarColor = texture(u_PlanarReflectionMap, perturbedUV).rgb;
        
        // Blend planar reflection over IBL environment specular based on roughness & Fresnel
        float reflectStrength = clamp((1.0 - roughness * 1.1), 0.0, 1.0) * (0.7 + 0.3 * F_IBL.r);
        specularIBL = mix(specularIBL, planarColor * (F_IBL * envBRDF.x + envBRDF.y), reflectStrength);
    }

    // Occlude indirect ambient radiance by AO
    vec3 ambient = (kD_IBL * diffuseIBL + specularIBL) * ao;

    // Output pure linear HDR color (Post-Processing Pass handles Tonemapping & Gamma)
    vec3 hdrColor = ambient + Lo;
    FragColor = vec4(hdrColor, 1.0);
}
