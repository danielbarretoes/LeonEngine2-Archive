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

layout(binding = 0) uniform sampler2D u_DepthTexture;

uniform mat4 u_Projection;
uniform mat4 u_InverseProjection;
uniform float u_Radius;
uniform float u_Bias;
uniform int u_KernelSize;
uniform vec3 u_Samples[16];

float InterleavedGradientNoise(vec2 InPixel) {
    return fract(52.9829189 * fract(dot(InPixel, vec2(0.06711056, 0.00583715))));
}

vec3 ReconstructViewPos(vec2 InUV, float InDepth) {
    vec4 ndc = vec4(InUV * 2.0 - 1.0, InDepth * 2.0 - 1.0, 1.0);
    vec4 view = u_InverseProjection * ndc;
    return view.xyz / view.w;
}

float SampleDepth(vec2 InUV) {
    ivec2 size = textureSize(u_DepthTexture, 0);
    ivec2 pix = ivec2(floor(clamp(InUV, 0.0, 0.99999) * vec2(size)));
    pix = clamp(pix, ivec2(0), size - ivec2(1));
    return texelFetch(u_DepthTexture, pix, 0).r;
}

void main() {
    // Flow: reconstruct view position → hemisphere samples → occlude only if the
    // hit sits in front of the local plane. A z-buffer compare self-occludes large
    // floors (derivative normals bowl, camera-facing line, wet floor looks dry).
    float depth = SampleDepth(v_TexCoord);
    if (depth >= 0.999) {
        FragColor = vec4(1.0);
        return;
    }

    vec3 viewPos = ReconstructViewPos(v_TexCoord, depth);
    vec3 dx = dFdx(viewPos);
    vec3 dy = dFdy(viewPos);
    vec3 normal = normalize(cross(dx, dy));
    if (dot(normal, -viewPos) < 0.0)
        normal = -normal;

    float ign = InterleavedGradientNoise(gl_FragCoord.xy);
    float angle = ign * 6.28318530718;
    float ca = cos(angle);
    float sa = sin(angle);
    vec3 rotatedX = vec3(ca, sa, 0.0);

    vec3 tangent = normalize(rotatedX - normal * dot(rotatedX, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tbn = mat3(tangent, bitangent, normal);

    float radius = max(u_Radius, 1e-4);
    float planeBias = max(u_Bias, radius * 0.18);
    float occlusion = 0.0;
    int kernelSize = clamp(u_KernelSize, 1, 16);
    for (int i = 0; i < kernelSize; ++i) {
        vec3 samplePos = viewPos + tbn * u_Samples[i] * radius;
        vec4 clip = u_Projection * vec4(samplePos, 1.0);
        if (abs(clip.w) < 1e-5)
            continue;
        vec3 ndc = clip.xyz / clip.w;
        vec2 sampleUV = ndc.xy * 0.5 + 0.5;
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        float sampleDepth = SampleDepth(sampleUV);
        if (sampleDepth >= 0.999)
            continue;

        vec3 occluderView = ReconstructViewPos(sampleUV, sampleDepth);
        vec3 toOccluder = occluderView - viewPos;
        float dist = length(toOccluder);
        if (dist < 1e-4 || dist > radius)
            continue;
        // Coplanar floors have ~0 offset along N. Counting them paints a moving horizon band.
        if (dot(toOccluder, normal) <= planeBias)
            continue;

        float rangeCheck = 1.0 - smoothstep(radius * 0.55, radius, dist);
        occlusion += rangeCheck;
    }

    float ao = 1.0 - occlusion / float(kernelSize);
    FragColor = vec4(ao, ao, ao, 1.0);
}
