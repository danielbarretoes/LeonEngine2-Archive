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

uniform sampler2D u_SceneTexture;
uniform float u_Exposure = 1.0;

// ACES Film Tonemapping Curve (Academy Color Encoding System / Unreal Engine Standard)
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec4 hdrSample = texture(u_SceneTexture, v_TexCoord);
    vec3 hdrColor = hdrSample.rgb * u_Exposure;

    // Tonemap HDR values to [0, 1] range using ACES Filmic curve
    vec3 ldrColor = ACESFilm(hdrColor);

    // Gamma correction: Linear color space -> sRGB Display (Gamma 2.2)
    vec3 srgbColor = pow(ldrColor, vec3(1.0 / 2.2));

    FragColor = vec4(srgbColor, hdrSample.a);
}
