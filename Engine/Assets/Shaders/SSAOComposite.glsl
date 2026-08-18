#type vertex
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec2 v_TexCoord;

void main() {
    v_TexCoord = aTexCoord;
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 FragColor;

in vec2 v_TexCoord;

layout(binding = 0) uniform sampler2D u_HDRSceneTexture;
layout(binding = 1) uniform sampler2D u_SSAOTexture;

uniform float u_Intensity = 1.0;
uniform int u_DebugAO = 0;

void main() {
    vec3 hdr = texture(u_HDRSceneTexture, v_TexCoord).rgb;
    float ao = texture(u_SSAOTexture, v_TexCoord).r;
    ao = mix(1.0, ao, clamp(u_Intensity, 0.0, 2.0));
    if (u_DebugAO == 1) {
        FragColor = vec4(vec3(ao), 1.0);
        return;
    }
    // Forward path has no G-buffer. Multiplying the whole HDR by AO flattens planar /
    // IBL specular on glossy floors. Keep highlights; darken dimmer irradiance.
    float lum = max(dot(hdr, vec3(0.2126, 0.7152, 0.0722)), 0.0);
    float specKeep = smoothstep(0.2, 1.25, lum);
    float aoLit = mix(ao, 1.0, specKeep);
    FragColor = vec4(hdr * aoLit, 1.0);
}
