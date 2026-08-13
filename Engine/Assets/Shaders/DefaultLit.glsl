#type vertex
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aColor;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec3 v_FragPos;
out vec3 v_Normal;
out vec2 v_TexCoord;
out vec3 v_Color;

void main() {
    v_FragPos = vec3(u_Model * vec4(aPos, 1.0));
    v_Normal = mat3(transpose(inverse(u_Model))) * aNormal;
    v_TexCoord = aTexCoord;
    v_Color = aColor;

    gl_Position = u_ViewProjection * vec4(v_FragPos, 1.0);
}

#type fragment
#version 330 core

in vec3 v_FragPos;
in vec3 v_Normal;
in vec2 v_TexCoord;
in vec3 v_Color;

out vec4 FragColor;

uniform vec3 u_ViewPos;

// Material / Texturing
uniform sampler2D u_DiffuseMap;
uniform bool u_UseTexture;
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

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 baseColor) {
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
    
    return ambient + diffuse + specular;
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
    
    // Combine 3 lighting types (Directional, Point, Spot)
    vec3 result = vec3(0.0);
    result += CalcDirLight(u_DirLight, norm, viewDir, baseColor);
    result += CalcPointLight(u_PointLight, norm, v_FragPos, viewDir, baseColor);
    result += CalcSpotLight(u_SpotLight, norm, v_FragPos, viewDir, baseColor);
    
    FragColor = vec4(result, texColor.a);
}
