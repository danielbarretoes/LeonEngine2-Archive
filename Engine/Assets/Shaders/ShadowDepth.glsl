#type vertex
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;
layout(location = 5) in vec3 aColor;

out vec2 v_TexCoord;

uniform mat4 u_LightSpaceMatrix;
uniform mat4 u_Model;
uniform vec2 u_UVTiling = vec2(1.0, 1.0);
uniform vec2 u_UVOffset = vec2(0.0, 0.0);

void main() {
    v_TexCoord = aTexCoord * u_UVTiling + u_UVOffset;
    gl_Position = u_LightSpaceMatrix * u_Model * vec4(aPos, 1.0);
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
