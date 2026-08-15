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
layout(binding = 1) uniform sampler2D u_BloomTexture;

uniform float u_Exposure = 1.0;
uniform float u_BloomIntensity = 0.05;
uniform int   u_UseBloom = 1;
uniform int   u_ToneMapper = 0; // 0 = ACES, 1 = Reinhard, 2 = Neutral Clamp, 3 = Uncharted 2
uniform float u_Gamma = 2.2;
uniform int   u_DebugMode = 0; // 0 = Full Composite, 1 = Raw HDR, 2 = Bloom Only, 3 = Bright Pass Only

// 1. ACES Filmic Curve (Narkowicz / Unreal Engine Standard Fit)
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// 2. Extended Reinhard Operator
vec3 ReinhardExtended(vec3 x) {
    float whitePoint = 4.0;
    return (x * (vec3(1.0) + (x / (whitePoint * whitePoint)))) / (vec3(1.0) + x);
}

// 3. Uncharted 2 (John Hable) Operator
vec3 Uncharted2Partial(vec3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 Uncharted2(vec3 x) {
    float W = 11.2;
    vec3 curr = Uncharted2Partial(x * 2.0);
    vec3 whiteScale = vec3(1.0) / Uncharted2Partial(vec3(W));
    return clamp(curr * whiteScale, 0.0, 1.0);
}

void main() {
    vec3 sceneHDR = texture(u_HDRSceneTexture, v_TexCoord).rgb;
    vec3 bloomHDR = (u_UseBloom == 1) ? texture(u_BloomTexture, v_TexCoord).rgb : vec3(0.0);

    // Forensic Debug Modes
    if (u_DebugMode == 1) {
        // Raw HDR Linear Scene (bypass tone mapping)
        FragColor = vec4(sceneHDR, 1.0);
        return;
    } else if (u_DebugMode == 2) {
        // Bloom Glow Output Only
        vec3 bloomView = bloomHDR * u_BloomIntensity;
        FragColor = vec4(bloomView, 1.0);
        return;
    }

    // HDR Radiance Composite
    vec3 hdrColor = sceneHDR + bloomHDR * u_BloomIntensity;

    // Apply Camera Exposure Multiplier
    vec3 exposedColor = hdrColor * max(u_Exposure, 0.0);

    // Apply Selectable Tone Mapping Operator
    vec3 ldrColor = vec3(0.0);
    if (u_ToneMapper == 0) {
        ldrColor = ACESFilm(exposedColor);
    } else if (u_ToneMapper == 1) {
        ldrColor = clamp(ReinhardExtended(exposedColor), 0.0, 1.0);
    } else if (u_ToneMapper == 2) {
        ldrColor = clamp(exposedColor, 0.0, 1.0);
    } else if (u_ToneMapper == 3) {
        ldrColor = Uncharted2(exposedColor);
    } else {
        ldrColor = ACESFilm(exposedColor);
    }

    // Gamma Correction: Linear Color Space -> sRGB Display Space
    vec3 srgbColor = pow(ldrColor, vec3(1.0 / max(u_Gamma, 0.0001)));

    // Encode Perceptual Luma into Alpha channel for FXAA Pass
    float luma = dot(srgbColor, vec3(0.299, 0.587, 0.114));

    FragColor = vec4(srgbColor, luma);
}
