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
    
    // Gram-Schmidt process for orthogonal TBN basis with fallback
    vec3 rawT = normalMatrix * aTangent;
    vec3 T;
    if (length(rawT) > 0.0001) {
        T = normalize(rawT);
        vec3 orthoT = T - dot(T, N) * N;
        if (length(orthoT) > 0.0001) {
            T = normalize(orthoT);
        } else {
            T = normalize(cross(N, vec3(0.0, 1.0, 0.0)));
            if (length(T) < 0.0001) T = normalize(cross(N, vec3(1.0, 0.0, 0.0)));
        }
    } else {
        T = normalize(cross(N, vec3(0.0, 1.0, 0.0)));
        if (length(T) < 0.0001) T = normalize(cross(N, vec3(1.0, 0.0, 0.0)));
    }
    vec3 B = cross(N, T);
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

uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicMap;
uniform sampler2D u_RoughnessMap;
uniform sampler2D u_ShadowMap;

uniform int u_UseAlbedoMap;
uniform int u_UseNormalMap;
uniform int u_UseMetallicMap;
uniform int u_UseRoughnessMap;
uniform int u_UseShadows;

// Environment / IBL Atmosphere Uniforms
uniform vec3 u_EnvSkyColor;
uniform vec3 u_EnvHorizonColor;
uniform vec3 u_EnvGroundColor;
uniform float u_EnvIntensity;
uniform float u_Exposure;

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

uniform DirectionalLight u_DirLight;
uniform PointLight u_PointLight;
uniform SpotLight u_SpotLight;

uniform vec3 u_ViewPos;

const float PI = 3.14159265358979323846;

// 1. ACES Film Tonemapping Curve (Unreal Engine Standard)
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

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

    if (projCoords.z > 1.0)
        return 0.0;

    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(u_ShadowMap, 0);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(u_ShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += projCoords.z - bias > pcfDepth ? 1.0 : 0.0;
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

    // Normal Mapping
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

    vec3 V = normalize(u_ViewPos - v_FragPos);
    vec3 R = reflect(-V, N);

    // Base Reflectance (Dielectric 0.04 vs Metallic)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Direct lighting radiance accumulation (Lo)
    vec3 Lo = vec3(0.0);

    // 1. Directional Sunlight
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
        Lo += u_DirLight.color * u_DirLight.ambientIntensity * albedo;
    }

    // 2. Point Light
    if (u_PointLight.enabled == 1) {
        vec3 L = normalize(u_PointLight.position - v_FragPos);
        vec3 H = normalize(V + L);
        float distance = length(u_PointLight.position - v_FragPos);
        float attenuation = 1.0 / (u_PointLight.constant + u_PointLight.linear * distance + u_PointLight.quadratic * (distance * distance));
        vec3 radiance = u_PointLight.color * u_PointLight.diffuseIntensity * attenuation;

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = (numerator / denominator) * u_PointLight.specularIntensity;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
        Lo += u_PointLight.color * u_PointLight.ambientIntensity * attenuation * albedo;
    }

    // 3. Spotlight
    if (u_SpotLight.enabled == 1) {
        vec3 L = normalize(u_SpotLight.position - v_FragPos);
        vec3 H = normalize(V + L);
        float distance = length(u_SpotLight.position - v_FragPos);
        float attenuation = 1.0 / (u_SpotLight.constant + u_SpotLight.linear * distance + u_SpotLight.quadratic * (distance * distance));

        float theta = dot(L, normalize(-u_SpotLight.direction));
        float epsilon = u_SpotLight.cutOff - u_SpotLight.outerCutOff;
        float spotIntensity = clamp((theta - u_SpotLight.outerCutOff) / epsilon, 0.0, 1.0);

        vec3 radiance = u_SpotLight.color * u_SpotLight.diffuseIntensity * attenuation * spotIntensity;

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = (numerator / denominator) * u_SpotLight.specularIntensity;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
        Lo += u_SpotLight.color * u_SpotLight.ambientIntensity * attenuation * spotIntensity * albedo;
    }

    // ========================================================
    // 4. Image-Based Lighting (IBL) Indirect Lighting Pass
    // ========================================================
    float NdotV = max(dot(N, V), 0.0);
    vec3 F_IBL = FresnelSchlickRoughness(NdotV, F0, roughness);
    
    vec3 kS_IBL = F_IBL;
    vec3 kD_IBL = (1.0 - kS_IBL) * (1.0 - metallic);
    
    // 4.1 Indirect Diffuse (Hemisphere Irradiance)
    vec3 irradiance = GetHemisphereIrradiance(N);
    vec3 diffuseIBL = irradiance * albedo;

    // 4.2 Indirect Specular (Environment Reflection based on Roughness)
    vec3 prefilteredColor = SampleEnvironmentAtmosphere(R, roughness);
    vec2 envBRDF = vec2(1.0 - roughness, roughness * 0.5);
    vec3 specularIBL = prefilteredColor * (F_IBL * envBRDF.x + envBRDF.y);

    vec3 ambient = (kD_IBL * diffuseIBL + specularIBL);

    // Final Radiance
    vec3 hdrColor = (ambient + Lo) * u_Exposure;

    // ACES Filmic Tonemapping & Gamma 2.2 Correction
    vec3 ldrColor = ACESFilm(hdrColor);
    ldrColor = pow(ldrColor, vec3(1.0 / 2.2));

    FragColor = vec4(ldrColor, 1.0);
}
