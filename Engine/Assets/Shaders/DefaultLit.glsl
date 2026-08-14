#type vertex
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;
layout (location = 5) in vec3 aColor;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;
uniform mat4 u_LightSpaceMatrix;

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

uniform vec3 u_ViewPos;

// Material / Texturing
uniform sampler2D u_DiffuseMap;
uniform sampler2D u_ShadowMap;
uniform bool u_UseTexture;
uniform bool u_UseShadows;
uniform float u_Shininess = 32.0;

// Directional Light
struct DirLight {
    bool enabled;
    vec3 direction;
    vec3 color;
    float ambientIntensity;
    float diffuseIntensity;
    float specularIntensity;
};
uniform DirLight u_DirLight;

// Point Light
struct PointLight {
    bool enabled;
    vec3 position;
    vec3 color;
    float constant;
    float linear;
    float quadratic;
    float ambientIntensity;
    float diffuseIntensity;
    float specularIntensity;
};
uniform PointLight u_PointLight;

// Spot Light (Flashlight / Cone light)
struct SpotLight {
    bool enabled;
    vec3 position;
    vec3 direction;
    vec3 color;
    float cutOff;      // cos(innerAngle)
    float outerCutOff; // cos(outerAngle)
    float constant;
    float linear;
    float quadratic;
    float ambientIntensity;
    float diffuseIntensity;
    float specularIntensity;
};
uniform SpotLight u_SpotLight;

float CalculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    if (!u_UseShadows) return 0.0;

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

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 baseColor, float shadow) {
    if (!light.enabled) return vec3(0.0);

    vec3 lightDir = normalize(-light.direction);
    
    // Ambient
    vec3 ambient = light.ambientIntensity * light.color * baseColor;
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.diffuseIntensity * diff * light.color * baseColor;
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), u_Shininess);
    vec3 specular = light.specularIntensity * spec * light.color;
    
    return ambient + (diffuse + specular) * (1.0 - shadow);
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 baseColor) {
    if (!light.enabled) return vec3(0.0);

    vec3 lightDir = normalize(light.position - fragPos);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    
    // Ambient
    vec3 ambient = light.ambientIntensity * light.color * baseColor;
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.diffuseIntensity * diff * light.color * baseColor;
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), u_Shininess);
    vec3 specular = light.specularIntensity * spec * light.color;
    
    return (ambient + diffuse + specular) * attenuation;
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 baseColor) {
    if (!light.enabled) return vec3(0.0);

    vec3 lightDir = normalize(light.position - fragPos);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    
    // Spotlight cone intensity
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    
    // Ambient
    vec3 ambient = light.ambientIntensity * light.color * baseColor;
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.diffuseIntensity * diff * light.color * baseColor;
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), u_Shininess);
    vec3 specular = light.specularIntensity * spec * light.color;
    
    return (ambient + (diffuse + specular) * intensity) * attenuation;
}

void main() {
    vec4 texColor = u_UseTexture ? texture(u_DiffuseMap, v_TexCoord) : vec4(1.0);
    vec3 baseColor = texColor.rgb * v_Color;
    
    vec3 norm = normalize(v_Normal);
    vec3 viewDir = normalize(u_ViewPos - v_FragPos);
    vec3 lightDir = normalize(-u_DirLight.direction);
    
    float shadow = CalculateShadow(v_FragPosLightSpace, norm, lightDir);
    
    // Combine 3 lighting types (Directional with Shadows, Point, Spot)
    vec3 result = vec3(0.0);
    result += CalcDirLight(u_DirLight, norm, viewDir, baseColor, shadow);
    result += CalcPointLight(u_PointLight, norm, v_FragPos, viewDir, baseColor);
    result += CalcSpotLight(u_SpotLight, norm, v_FragPos, viewDir, baseColor);
    
    FragColor = vec4(result, texColor.a);
}
