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
uniform vec2 u_TexelSize; // 1.0 / Source Dimensions
uniform int u_MipLevel = 0;

// Karis Luminance Weighting to eliminate subpixel fireflies
float KarisWeight(vec3 c) {
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    return 1.0 / (1.0 + luma);
}

void main() {
    float x = u_TexelSize.x;
    float y = u_TexelSize.y;

    // 13-tap Jimenez / Karis filter pattern (36 bilinear sample points)
    // a - b - c
    // - d - e -
    // f - g - h
    // - i - j -
    // k - l - m
    vec3 a = texture(u_SourceTexture, vec2(v_TexCoord.x - 2.0 * x, v_TexCoord.y + 2.0 * y)).rgb;
    vec3 b = texture(u_SourceTexture, vec2(v_TexCoord.x,           v_TexCoord.y + 2.0 * y)).rgb;
    vec3 c = texture(u_SourceTexture, vec2(v_TexCoord.x + 2.0 * x, v_TexCoord.y + 2.0 * y)).rgb;

    vec3 d = texture(u_SourceTexture, vec2(v_TexCoord.x - x,       v_TexCoord.y + y)).rgb;
    vec3 e = texture(u_SourceTexture, vec2(v_TexCoord.x + x,       v_TexCoord.y + y)).rgb;

    vec3 f = texture(u_SourceTexture, vec2(v_TexCoord.x - 2.0 * x, v_TexCoord.y)).rgb;
    vec3 g = texture(u_SourceTexture, vec2(v_TexCoord.x,           v_TexCoord.y)).rgb;
    vec3 h = texture(u_SourceTexture, vec2(v_TexCoord.x + 2.0 * x, v_TexCoord.y)).rgb;

    vec3 i = texture(u_SourceTexture, vec2(v_TexCoord.x - x,       v_TexCoord.y - y)).rgb;
    vec3 j = texture(u_SourceTexture, vec2(v_TexCoord.x + x,       v_TexCoord.y - y)).rgb;

    vec3 k = texture(u_SourceTexture, vec2(v_TexCoord.x - 2.0 * x, v_TexCoord.y - 2.0 * y)).rgb;
    vec3 l = texture(u_SourceTexture, vec2(v_TexCoord.x,           v_TexCoord.y - 2.0 * y)).rgb;
    vec3 m = texture(u_SourceTexture, vec2(v_TexCoord.x + 2.0 * x, v_TexCoord.y - 2.0 * y)).rgb;

    vec3 downsample = vec3(0.0);

    if (u_MipLevel == 0) {
        // Karis partial weighting on first downsample pass
        vec3 box1 = (a + b + d + g) * 0.25;
        vec3 box2 = (b + c + g + e) * 0.25;
        vec3 box3 = (d + g + f + k) * 0.25;
        vec3 box4 = (g + e + l + m) * 0.25;
        vec3 box5 = (d + e + i + j) * 0.25;

        float w1 = KarisWeight(box1);
        float w2 = KarisWeight(box2);
        float w3 = KarisWeight(box3);
        float w4 = KarisWeight(box4);
        float w5 = KarisWeight(box5);

        downsample = (box1 * w1 + box2 * w2 + box3 * w3 + box4 * w4 + box5 * w5) / (w1 + w2 + w3 + w4 + w5 + 0.00001);
    } else {
        // Linear box weighting for subsequent mips (Jimenez 13-tap filter pattern)
        vec3 centerBox = (d + e + i + j) * 0.125;
        vec3 corner1   = (a + b + f + g) * 0.03125;
        vec3 corner2   = (b + c + g + h) * 0.03125;
        vec3 corner3   = (f + g + k + l) * 0.03125;
        vec3 corner4   = (g + h + l + m) * 0.03125;

        downsample = centerBox + corner1 + corner2 + corner3 + corner4;
    }

    FragColor = vec4(downsample, 1.0);
}
