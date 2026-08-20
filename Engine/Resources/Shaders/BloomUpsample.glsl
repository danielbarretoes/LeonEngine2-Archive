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

layout(binding = 0) uniform sampler2D u_SourceTexture;
uniform float u_FilterRadius = 1.0;
uniform vec2 u_TexelSize;

void main() {
    float x = u_TexelSize.x * u_FilterRadius;
    float y = u_TexelSize.y * u_FilterRadius;

    // 9-tap 3x3 Tent Filter
    // 1   2   1
    // 2   4   2   x (1/16)
    // 1   2   1
    vec3 a = texture(u_SourceTexture, vec2(v_TexCoord.x - x, v_TexCoord.y + y)).rgb;
    vec3 b = texture(u_SourceTexture, vec2(v_TexCoord.x,     v_TexCoord.y + y)).rgb;
    vec3 c = texture(u_SourceTexture, vec2(v_TexCoord.x + x, v_TexCoord.y + y)).rgb;

    vec3 d = texture(u_SourceTexture, vec2(v_TexCoord.x - x, v_TexCoord.y)).rgb;
    vec3 e = texture(u_SourceTexture, vec2(v_TexCoord.x,     v_TexCoord.y)).rgb;
    vec3 f = texture(u_SourceTexture, vec2(v_TexCoord.x + x, v_TexCoord.y)).rgb;

    vec3 g = texture(u_SourceTexture, vec2(v_TexCoord.x - x, v_TexCoord.y - y)).rgb;
    vec3 h = texture(u_SourceTexture, vec2(v_TexCoord.x,     v_TexCoord.y - y)).rgb;
    vec3 i = texture(u_SourceTexture, vec2(v_TexCoord.x + x, v_TexCoord.y - y)).rgb;

    vec3 upsample = e * 4.0;
    upsample += (b + d + f + h) * 2.0;
    upsample += (a + c + g + i) * 1.0;
    upsample *= (1.0 / 16.0);

    FragColor = vec4(upsample, 1.0);
}
