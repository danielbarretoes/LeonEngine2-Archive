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

layout(binding = 0) uniform sampler2D u_SSAOTexture;
layout(binding = 1) uniform sampler2D u_DepthTexture;

uniform vec2 u_TexelSize;
uniform int u_Horizontal = 1;

void main() {
    ivec2 depthSize = textureSize(u_DepthTexture, 0);
    ivec2 centerPix = clamp(ivec2(floor(v_TexCoord * vec2(depthSize))), ivec2(0), depthSize - ivec2(1));
    float centerDepth = texelFetch(u_DepthTexture, centerPix, 0).r;
    float result = 0.0;
    float weightSum = 0.0;
    const float kernel[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

    for (int i = -4; i <= 4; ++i) {
        vec2 offset = (u_Horizontal == 1) ? vec2(float(i) * u_TexelSize.x, 0.0)
                                          : vec2(0.0, float(i) * u_TexelSize.y);
        float sampleAO = texture(u_SSAOTexture, v_TexCoord + offset).r;
        vec2 sampleUV = clamp(v_TexCoord + offset, 0.0, 1.0);
        ivec2 samplePix = clamp(ivec2(floor(sampleUV * vec2(depthSize))), ivec2(0), depthSize - ivec2(1));
        float sampleDepth = texelFetch(u_DepthTexture, samplePix, 0).r;
        float w = kernel[min(abs(i), 4)] * (1.0 / (1.0 + abs(centerDepth - sampleDepth) * 80.0));
        result += sampleAO * w;
        weightSum += w;
    }

    float ao = result / max(weightSum, 1e-4);
    FragColor = vec4(ao, ao, ao, 1.0);
}
