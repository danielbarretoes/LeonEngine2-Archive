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

layout(binding = 0) uniform sampler2D u_LDRTexture;
uniform vec2 u_InverseScreenSize; // 1.0 / (width, height)
uniform int  u_FXAAEnabled = 1;

// FXAA 3.11 Quality Settings
#define EDGE_THRESHOLD_MIN 0.0312
#define EDGE_THRESHOLD_MAX 0.125
#define SUBPIXEL_QUALITY 0.75
#define ITERATIONS 12

const float QUALITY[12] = float[12](1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0);

float GetLuma(vec3 rgb) {
    return dot(rgb, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec4 centerSample = texture(u_LDRTexture, v_TexCoord);
    vec3 colorCenter = centerSample.rgb;

    if (u_FXAAEnabled == 0) {
        FragColor = vec4(colorCenter, 1.0);
        return;
    }

    // Luma of center and 4 direct orthogonal neighbors
    float lumaM = (centerSample.a > 0.0) ? centerSample.a : GetLuma(colorCenter);
    float lumaN = textureOffset(u_LDRTexture, v_TexCoord, ivec2( 0,  1)).a;
    float lumaS = textureOffset(u_LDRTexture, v_TexCoord, ivec2( 0, -1)).a;
    float lumaE = textureOffset(u_LDRTexture, v_TexCoord, ivec2( 1,  0)).a;
    float lumaW = textureOffset(u_LDRTexture, v_TexCoord, ivec2(-1,  0)).a;

    // Minimum and maximum luma around the current pixel
    float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaE, lumaW)));
    float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaE, lumaW)));

    // Contrast delta
    float lumaRange = lumaMax - lumaMin;

    // Early exit if contrast is below local threshold (not an edge)
    if (lumaRange < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD_MAX)) {
        FragColor = vec4(colorCenter, 1.0);
        return;
    }

    // 4 diagonal neighbors for 3x3 filter
    float lumaNW = textureOffset(u_LDRTexture, v_TexCoord, ivec2(-1,  1)).a;
    float lumaNE = textureOffset(u_LDRTexture, v_TexCoord, ivec2( 1,  1)).a;
    float lumaSW = textureOffset(u_LDRTexture, v_TexCoord, ivec2(-1, -1)).a;
    float lumaSE = textureOffset(u_LDRTexture, v_TexCoord, ivec2( 1, -1)).a;

    // Combine corners and edges
    float lumaDownUp = lumaN + lumaS;
    float lumaLeftRight = lumaE + lumaW;
    float lumaCorners = lumaNW + lumaNE + lumaSW + lumaSE;

    // Horizontal vs Vertical Edge Detection
    float edgeHorz = abs(-2.0 * lumaM + lumaDownUp) * 2.0 +
                     abs(-2.0 * lumaE + (lumaNE + lumaSE)) +
                     abs(-2.0 * lumaW + (lumaNW + lumaSW));

    float edgeVert = abs(-2.0 * lumaM + lumaLeftRight) * 2.0 +
                     abs(-2.0 * lumaN + (lumaNW + lumaNE)) +
                     abs(-2.0 * lumaS + (lumaSW + lumaSE));

    bool isHorizontal = (edgeHorz >= edgeVert);

    // Select step direction based on edge orientation
    float luma1 = isHorizontal ? lumaS : lumaW;
    float luma2 = isHorizontal ? lumaN : lumaE;
    float gradient1 = luma1 - lumaM;
    float gradient2 = luma2 - lumaM;

    bool is1Steeper = abs(gradient1) >= abs(gradient2);
    float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));

    float stepLength = isHorizontal ? u_InverseScreenSize.y : u_InverseScreenSize.x;
    float lumaLocalAverage = 0.0;

    if (is1Steeper) {
        stepLength = -stepLength;
        lumaLocalAverage = 0.5 * (luma1 + lumaM);
    } else {
        lumaLocalAverage = 0.5 * (luma2 + lumaM);
    }

    vec2 currentUv = v_TexCoord;
    if (isHorizontal) {
        currentUv.y += stepLength * 0.5;
    } else {
        currentUv.x += stepLength * 0.5;
    }

    // Step search along the edge in both directions
    vec2 offset = isHorizontal ? vec2(u_InverseScreenSize.x, 0.0) : vec2(0.0, u_InverseScreenSize.y);
    vec2 uvPos = currentUv + offset;
    vec2 uvNeg = currentUv - offset;

    float lumaEndPos = texture(u_LDRTexture, uvPos).a - lumaLocalAverage;
    float lumaEndNeg = texture(u_LDRTexture, uvNeg).a - lumaLocalAverage;

    bool reachedPos = abs(lumaEndPos) >= gradientScaled;
    bool reachedNeg = abs(lumaEndNeg) >= gradientScaled;
    bool reachedBoth = reachedPos && reachedNeg;

    if (!reachedPos) uvPos += offset;
    if (!reachedNeg) uvNeg -= offset;

    if (!reachedBoth) {
        for (int i = 2; i < ITERATIONS; ++i) {
            if (!reachedPos) lumaEndPos = texture(u_LDRTexture, uvPos).a - lumaLocalAverage;
            if (!reachedNeg) lumaEndNeg = texture(u_LDRTexture, uvNeg).a - lumaLocalAverage;

            reachedPos = abs(lumaEndPos) >= gradientScaled;
            reachedNeg = abs(lumaEndNeg) >= gradientScaled;
            reachedBoth = reachedPos && reachedNeg;

            if (!reachedPos) uvPos += offset * QUALITY[i];
            if (!reachedNeg) uvNeg -= offset * QUALITY[i];

            if (reachedBoth) break;
        }
    }

    // Estimate distance to edge endpoints
    float distPos = isHorizontal ? (uvPos.x - v_TexCoord.x) : (uvPos.y - v_TexCoord.y);
    float distNeg = isHorizontal ? (v_TexCoord.x - uvNeg.x) : (v_TexCoord.y - uvNeg.y);

    bool isDirectionPos = distPos < distNeg;
    float distFinal = min(distPos, distNeg);
    float edgeThickness = distPos + distNeg;

    float pixelOffset = -distFinal / edgeThickness + 0.5;

    // Check if luma at endpoint has matching sign
    bool isLumaCenterSmaller = lumaM < lumaLocalAverage;
    bool correctVariation = ((isDirectionPos ? lumaEndPos : lumaEndNeg) < 0.0) != isLumaCenterSmaller;
    float finalOffset = correctVariation ? pixelOffset : 0.0;

    // Sub-pixel antialiasing
    float lumaAverage = (1.0 / 12.0) * (2.0 * (lumaDownUp + lumaLeftRight) + lumaCorners);
    float subPixelOffset1 = clamp(abs(lumaAverage - lumaM) / lumaRange, 0.0, 1.0);
    float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;
    float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * SUBPIXEL_QUALITY;

    finalOffset = max(finalOffset, subPixelOffsetFinal);

    // Final sample coordinates
    vec2 finalUv = v_TexCoord;
    if (isHorizontal) {
        finalUv.y += finalOffset * stepLength;
    } else {
        finalUv.x += finalOffset * stepLength;
    }

    vec3 finalColor = texture(u_LDRTexture, finalUv).rgb;
    FragColor = vec4(finalColor, 1.0);
}
