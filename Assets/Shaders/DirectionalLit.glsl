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

// Material Sampler
uniform sampler2D u_DiffuseMap;
uniform bool u_UseTexture;

// Directional Light Uniforms
uniform vec3 u_LightDirection;
uniform vec3 u_LightColor;
uniform float u_AmbientIntensity;

void main() {
    // Determine Base Color (from Texture or Vertex Color)
    vec4 baseColor = u_UseTexture ? texture(u_DiffuseMap, v_TexCoord) : vec4(v_Color, 1.0);

    // Ambient Component
    vec3 ambient = u_AmbientIntensity * u_LightColor;

    // Diffuse Component (Lambertian)
    vec3 norm = normalize(v_Normal);
    vec3 lightDir = normalize(-u_LightDirection);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * u_LightColor;

    // Specular Component (Blinn-Phong)
    vec3 viewDir = normalize(u_ViewPos - v_FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
    vec3 specular = 0.4 * spec * u_LightColor;

    // Combine all lighting terms with base color
    vec3 result = (ambient + diffuse + specular) * baseColor.rgb;
    FragColor = vec4(result, baseColor.a);
}
