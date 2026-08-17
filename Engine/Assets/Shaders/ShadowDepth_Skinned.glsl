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

out vec2 v_TexCoord;

layout(std140, binding = 2) uniform BonePalette {
    mat4 u_Bones[128];
};

uniform mat4 u_LightSpaceMatrix;
uniform mat4 u_Model;
uniform vec2 u_UVTiling = vec2(1.0, 1.0);
uniform vec2 u_UVOffset = vec2(0.0, 0.0);

mat4 SkinMatrix() {
    return u_Bones[aBoneIndices.x] * aBoneWeights.x
         + u_Bones[aBoneIndices.y] * aBoneWeights.y
         + u_Bones[aBoneIndices.z] * aBoneWeights.z
         + u_Bones[aBoneIndices.w] * aBoneWeights.w;
}

void main() {
    v_TexCoord = aTexCoord * u_UVTiling + u_UVOffset;
    vec4 skinnedPos = SkinMatrix() * vec4(aPos, 1.0);
    gl_Position = u_LightSpaceMatrix * u_Model * skinnedPos;
}

#type fragment
#version 450 core

in vec2 v_TexCoord;

uniform int u_AlphaMode = 0; // 0 = Opaque, 1 = Mask, 2 = Blend
uniform float u_AlphaCutoff = 0.5;
uniform int u_UseAlbedoMap = 0;
layout(binding = 0) uniform sampler2D u_AlbedoMap;

void main() {
    if (u_AlphaMode == 1 && u_UseAlbedoMap == 1) {
        float alpha = texture(u_AlbedoMap, v_TexCoord).a;
        if (alpha < u_AlphaCutoff) {
            discard;
        }
    }
}
