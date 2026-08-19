#type vertex
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec4 aTangent;
layout(location = 4) in vec3 aColor;
layout(location = 5) in vec2 aLightmapUV;

out vec2 v_TexCoord;
out vec3 v_WorldPos;

uniform mat4 u_LightSpaceMatrix;
uniform mat4 u_Model;
uniform vec2 u_UVTiling = vec2(1.0, 1.0);
uniform vec2 u_UVOffset = vec2(0.0, 0.0);

void main() {
    v_TexCoord = aTexCoord * u_UVTiling + u_UVOffset;
    vec4 world = u_Model * vec4(aPos, 1.0);
    v_WorldPos = world.xyz;
    gl_Position = u_LightSpaceMatrix * world;
}

#type fragment
#version 450 core

in vec2 v_TexCoord;
in vec3 v_WorldPos;

uniform int u_AlphaMode = 0; // 0 = Opaque, 1 = Mask, 2 = Blend
uniform float u_AlphaCutoff = 0.5;
uniform int u_UseAlbedoMap = 0;
layout(binding = 0) uniform sampler2D u_AlbedoMap;
uniform vec3 u_PointLightWorldPosition = vec3(0.0);
uniform float u_PointShadowFarPlane = 0.0;

void main() {
    if (u_AlphaMode == 1 && u_UseAlbedoMap == 1) {
        float alpha = texture(u_AlbedoMap, v_TexCoord).a;
        if (alpha < u_AlphaCutoff) {
            discard;
        }
    }
    if (u_PointShadowFarPlane > 0.0) {
        gl_FragDepth = length(v_WorldPos - u_PointLightWorldPosition) / u_PointShadowFarPlane;
    }
}
