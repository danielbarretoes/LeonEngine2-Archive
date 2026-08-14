#type vertex
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;
layout (location = 5) in vec3 aColor;

layout(std140) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_LightSpaceMatrix;
    vec4 u_ViewPos;
};

uniform mat4 u_Model;

out vec3 v_FragPos;
out vec3 v_Normal;
out vec2 v_TexCoord;
out vec3 v_Color;
out vec4 v_FragPosLightSpace;

void main() {
    vec4 worldPos = u_Model * vec4(aPos, 1.0);
    v_FragPos = worldPos.xyz;
    v_Normal = mat3(transpose(inverse(u_Model))) * aNormal;
    v_TexCoord = aTexCoord;
    v_Color = aColor;
    v_FragPosLightSpace = u_LightSpaceMatrix * worldPos;

    gl_Position = u_ViewProjection * worldPos;
}

#type fragment
#version 330 core

in vec3 v_FragPos;
in vec3 v_Normal;
in vec2 v_TexCoord;
in vec3 v_Color;
in vec4 v_FragPosLightSpace;

out vec4 FragColor;

layout(std140) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_LightSpaceMatrix;
    vec4 u_ViewPos;
};

struct DirectionalLight {
    vec4 direction;
    vec4 color;
    vec4 intensities;
};

struct PointLight {
    vec4 position;
    vec4 color;
    vec4 attenuation;
    vec4 params;
};

struct SpotLight {
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 attenuation;
    vec4 params;
};

#define MAX_POINT_LIGHTS 16
#define MAX_SPOT_LIGHTS 8

layout(std140) uniform LightingData {
    DirectionalLight u_DirLight;
    PointLight u_PointLights[MAX_POINT_LIGHTS];
    SpotLight u_SpotLights[MAX_SPOT_LIGHTS];
    ivec4 u_LightCounts;
    vec4 u_EnvSkyColor;
    vec4 u_EnvHorizonColor;
    vec4 u_EnvGroundColor;
};

// Material / Texturing
uniform sampler2D u_DiffuseMap;
uniform sampler2DShadow u_ShadowMap;
uniform int u_UseTexture;
uniform int u_UseShadows;
uniform float u_Shininess = 32.0;

float CalculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    if (u_UseShadows == 0) return 0.0;

    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float bias = max(0.0015 * (1.0 - dot(normal, lightDir)), 0.0003);
    float currentDepth = projCoords.z - bias;

    float shadow = 0.0;
    vec2 texelSize = vec2(1.0) / vec2(textureSize(u_ShadowMap, 0));
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            shadow += texture(u_ShadowMap, vec3(projCoords.xy + vec2(x, y) * texelSize, currentDepth));
        }
    }

    return 1.0 - (shadow / 9.0);
}

void main() {
    vec4 texColor = (u_UseTexture == 1) ? texture(u_DiffuseMap, v_TexCoord) : vec4(1.0);
    vec3 baseColor = texColor.rgb * v_Color;
    
    vec3 norm = normalize(v_Normal);
    vec3 viewDir = normalize(u_ViewPos.xyz - v_FragPos);
    
    vec3 result = vec3(0.0);
    
    // Directional light
    if (u_DirLight.direction.w > 0.5) {
        vec3 lightDir = normalize(-u_DirLight.direction.xyz);
        float shadow = CalculateShadow(v_FragPosLightSpace, norm, lightDir);
        
        vec3 ambient = u_DirLight.color.w * u_DirLight.color.rgb * baseColor;
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = u_DirLight.intensities.x * diff * u_DirLight.color.rgb * baseColor;
        
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), u_Shininess);
        vec3 specular = u_DirLight.intensities.y * spec * u_DirLight.color.rgb;
        
        result += ambient + (diffuse + specular) * (1.0 - shadow);
    }
    
    // Point lights
    int pointCount = min(u_LightCounts.x, MAX_POINT_LIGHTS);
    for (int i = 0; i < pointCount; ++i) {
        if (u_PointLights[i].position.w < 0.5) continue;
        
        vec3 lightDir = normalize(u_PointLights[i].position.xyz - v_FragPos);
        float distance = length(u_PointLights[i].position.xyz - v_FragPos);
        float attenuation = 1.0 / (u_PointLights[i].attenuation.x + u_PointLights[i].attenuation.y * distance + u_PointLights[i].attenuation.z * (distance * distance));
        
        vec3 ambient = u_PointLights[i].color.w * u_PointLights[i].color.rgb * baseColor;
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = u_PointLights[i].attenuation.w * diff * u_PointLights[i].color.rgb * baseColor;
        
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), u_Shininess);
        vec3 specular = u_PointLights[i].params.x * spec * u_PointLights[i].color.rgb;
        
        result += (ambient + diffuse + specular) * attenuation;
    }
    
    // Spotlights
    int spotCount = min(u_LightCounts.y, MAX_SPOT_LIGHTS);
    for (int i = 0; i < spotCount; ++i) {
        if (u_SpotLights[i].position.w < 0.5) continue;
        
        vec3 lightDir = normalize(u_SpotLights[i].position.xyz - v_FragPos);
        float distance = length(u_SpotLights[i].position.xyz - v_FragPos);
        float attenuation = 1.0 / (u_SpotLights[i].attenuation.x + u_SpotLights[i].attenuation.y * distance + u_SpotLights[i].attenuation.z * (distance * distance));
        
        float theta = dot(lightDir, normalize(-u_SpotLights[i].direction.xyz));
        float cutOff = u_SpotLights[i].direction.w;
        float outerCutOff = u_SpotLights[i].color.w;
        float epsilon = cutOff - outerCutOff;
        float intensity = clamp((theta - outerCutOff) / max(epsilon, 0.0001), 0.0, 1.0);
        
        vec3 ambient = u_SpotLights[i].params.x * u_SpotLights[i].color.rgb * baseColor;
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = u_SpotLights[i].attenuation.w * diff * u_SpotLights[i].color.rgb * baseColor;
        
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), u_Shininess);
        vec3 specular = u_SpotLights[i].params.y * spec * u_SpotLights[i].color.rgb;
        
        result += (ambient + (diffuse + specular) * intensity) * attenuation;
    }
    
    FragColor = vec4(result, texColor.a);
}
