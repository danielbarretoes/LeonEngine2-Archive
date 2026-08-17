#type vertex
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec4 aTangent;
layout(location = 4) in vec3 aColor;
layout(location = 5) in vec2 aLightmapUV;
layout(location = 6) in ivec4 aBoneIndices;
layout(location = 7) in vec4 aBoneWeights;

layout(std140) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_LightSpaceMatrices[4];
    mat4 u_SpotLightSpaceMatrix;
    vec4 u_ViewPos;
    vec4 u_CameraForward;
    vec4 u_CascadeSplits;
    vec4 u_ShadowParams;
    ivec4 u_ShadowSettings;
};

layout(std140, binding = 2) uniform BonePalette {
    mat4 u_Bones[128];
};

uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;
uniform float u_OutlineWidth = 0.016;

mat4 SkinMatrix() {
    return u_Bones[aBoneIndices.x] * aBoneWeights.x
         + u_Bones[aBoneIndices.y] * aBoneWeights.y
         + u_Bones[aBoneIndices.z] * aBoneWeights.z
         + u_Bones[aBoneIndices.w] * aBoneWeights.w;
}

void main() {
    mat4 skin = SkinMatrix();
    vec4 skinnedPos = skin * vec4(aPos, 1.0);
    vec3 skinnedN = normalize(mat3(skin) * aNormal);
    vec3 worldN = normalize(u_NormalMatrix * skinnedN);
    vec4 worldPos = u_Model * skinnedPos;
    worldPos.xyz += worldN * u_OutlineWidth;
    gl_Position = u_ViewProjection * worldPos;
}

#type fragment
#version 450 core

layout(location = 0) out vec4 FragColor;

uniform vec3 u_OutlineColor = vec3(1.0, 0.14, 0.1);

void main() {
    FragColor = vec4(u_OutlineColor, 1.0);
}
