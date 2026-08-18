// Shared PBR fragment helpers for PBR_Lit / PBR_Skinned (included via FOpenGLShader).
// Requires uniforms/UBOs declared in the including shader before this include.

// -----------------------------------------------------------------------------
// Poisson Disk Offsets (16 Samples - Vogel spiral distribution)
// -----------------------------------------------------------------------------
vec3 SafeNormalize3(vec3 v, vec3 fallback) {
    float len2 = dot(v, v);
    return len2 > 1e-8 ? v * inversesqrt(len2) : fallback;
}

const vec2 POISSON_DISK[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870),
    vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845),
    vec2( 0.97484398,  0.75648377),
    vec2( 0.44323325, -0.97511554),
    vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507),
    vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367),
    vec2( 0.14383161, -0.14100790)
);

float InterleavedGradientNoise(vec2 screenPos) {
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(screenPos, magic.xy)));
}

// -----------------------------------------------------------------------------
// 1. Physically-Based Shading Equations (PBR Cook-Torrance)
// -----------------------------------------------------------------------------
const float PI = 3.14159265358979323846;

// 2. Normal Distribution Function (Trowbridge-Reitz GGX)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float f = (NdotH2 * (a2 - 1.0) + 1.0);
    float denom = PI * f * f;
    return denom > 1e-20 ? nom / denom : 0.0;
}

// 3. Geometry Function (Smith Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, 0.0000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// 4. Fresnel Function with Roughness
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 SafeHalfVector(vec3 V, vec3 L) {
    vec3 h = V + L;
    float h2 = dot(h, h);
    return h2 > 1e-8 ? h * inversesqrt(h2) : vec3(0.0);
}

// 5. Image-Based Lighting (IBL) Atmospheric Environment Fallback
vec3 SampleEnvironmentAtmosphere(vec3 dir, float roughness) {
    float height = dir.y;
    vec3 sky;
    if (height >= 0.0) {
        float horizonFactor = pow(1.0 - height, 4.0);
        sky = mix(u_EnvSkyColor.rgb, u_EnvHorizonColor.rgb, horizonFactor);
    } else {
        float groundFactor = clamp(-height * 3.0, 0.0, 1.0);
        sky = mix(u_EnvHorizonColor.rgb, u_EnvGroundColor.rgb, groundFactor);
    }
    return sky * u_EnvSkyColor.w;
}

vec3 GetHemisphereIrradiance(vec3 normal) {
    float upFraction = normal.y * 0.5 + 0.5;
    vec3 groundToSky = mix(u_EnvGroundColor.rgb, u_EnvSkyColor.rgb, upFraction);
    return groundToSky * u_EnvSkyColor.w * PI;
}

// -----------------------------------------------------------------------------
// Advanced Cascaded Shadow Mapping (Multi-Filter, Normal Offset Bias & Blending)
// -----------------------------------------------------------------------------
float ShadowSlopeTan(vec3 normal, vec3 lightDir) {
    float NdotL = clamp(dot(normal, lightDir), 0.0, 1.0);
    float sinTheta = sqrt(max(0.0, 1.0 - NdotL * NdotL));
    // Clamp extreme grazing: tan(θ) → ∞ would peter-pan the contact shadow.
    return min(sinTheta / max(NdotL, 0.08), 8.0);
}

// Rasterized triangle plane (matches the shadow-map depth). Phong-interpolated vertex
// normals under-bias smooth/concave panels. Degenerate dFdx (1px tests) falls back.
vec3 ShadowFaceNormal(vec3 fragPos, vec3 vertexN) {
    vec3 fn = cross(dFdx(fragPos), dFdy(fragPos));
    float len2 = dot(fn, fn);
    if (len2 < 1e-12)
        return vertexN;
    fn *= inversesqrt(len2);
    return fn * sign(dot(fn, vertexN) + 1e-8);
}

// GPU Gems 3: ∂z/∂u, ∂z/∂v in shadow UV so PCF taps follow the receiver plane.
vec2 ReceiverPlaneDzDuv(vec3 dpdx, vec3 dpdy) {
    vec2 dzDuv;
    dzDuv.x = dpdy.y * dpdx.z - dpdx.y * dpdy.z;
    dzDuv.y = dpdx.x * dpdy.z - dpdy.x * dpdx.z;
    float det = dpdx.x * dpdy.y - dpdx.y * dpdy.x;
    float invDet = 1.0 / (sign(det) * max(abs(det), 1e-6));
    return clamp(dzDuv * invDet, vec2(-16.0), vec2(16.0));
}

float ShadowTapArray(sampler2DArrayShadow shadowMap, int cascadeIndex, vec2 uv, float refZ) {
    return texture(shadowMap, vec4(uv, float(cascadeIndex), refZ));
}

float ShadowTap2D(sampler2DShadow shadowMap, vec2 uv, float refZ) {
    return texture(shadowMap, vec3(uv, refZ));
}

float FilterShadowArray(sampler2DArrayShadow shadowMap, int cascadeIndex, vec3 projCoords, float refZ) {
    vec2 texelSize = vec2(1.0) / vec2(textureSize(shadowMap, 0).xy);
    vec2 dzDuv = ReceiverPlaneDzDuv(dFdx(projCoords), dFdy(projCoords));
    int filterMode = u_ShadowSettings.x;
    float shadow = 0.0;

    if (filterMode == 0) {
        return 1.0 - ShadowTapArray(shadowMap, cascadeIndex, projCoords.xy, refZ);
    } else if (filterMode == 1) {
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                vec2 uvOff = vec2(x, y) * texelSize;
                shadow += ShadowTapArray(shadowMap, cascadeIndex, projCoords.xy + uvOff, refZ + dot(dzDuv, uvOff));
            }
        }
        return 1.0 - (shadow / 9.0);
    } else if (filterMode == 2) {
        for (int x = -2; x <= 2; ++x) {
            for (int y = -2; y <= 2; ++y) {
                vec2 uvOff = vec2(x, y) * texelSize;
                shadow += ShadowTapArray(shadowMap, cascadeIndex, projCoords.xy + uvOff, refZ + dot(dzDuv, uvOff));
            }
        }
        return 1.0 - (shadow / 25.0);
    }

    float noise = InterleavedGradientNoise(gl_FragCoord.xy);
    float angle = noise * 2.0 * PI;
    mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
    float diskRadius = 1.75;
    for (int i = 0; i < 16; ++i) {
        vec2 uvOff = rot * POISSON_DISK[i] * diskRadius * texelSize;
        shadow += ShadowTapArray(shadowMap, cascadeIndex, projCoords.xy + uvOff, refZ + dot(dzDuv, uvOff));
    }
    return 1.0 - (shadow / 16.0);
}

float FilterShadow2D(sampler2DShadow shadowMap, vec3 projCoords, float refZ) {
    vec2 texelSize = vec2(1.0) / vec2(textureSize(shadowMap, 0));
    vec2 dzDuv = ReceiverPlaneDzDuv(dFdx(projCoords), dFdy(projCoords));
    int filterMode = u_ShadowSettings.x;
    float shadow = 0.0;

    if (filterMode == 0) {
        return 1.0 - ShadowTap2D(shadowMap, projCoords.xy, refZ);
    } else if (filterMode == 1) {
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                vec2 uvOff = vec2(x, y) * texelSize;
                shadow += ShadowTap2D(shadowMap, projCoords.xy + uvOff, refZ + dot(dzDuv, uvOff));
            }
        }
        return 1.0 - (shadow / 9.0);
    } else if (filterMode == 2) {
        for (int x = -2; x <= 2; ++x) {
            for (int y = -2; y <= 2; ++y) {
                vec2 uvOff = vec2(x, y) * texelSize;
                shadow += ShadowTap2D(shadowMap, projCoords.xy + uvOff, refZ + dot(dzDuv, uvOff));
            }
        }
        return 1.0 - (shadow / 25.0);
    }

    float noise = InterleavedGradientNoise(gl_FragCoord.xy);
    float angle = noise * 2.0 * PI;
    mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
    float diskRadius = 1.75;
    for (int i = 0; i < 16; ++i) {
        vec2 uvOff = rot * POISSON_DISK[i] * diskRadius * texelSize;
        shadow += ShadowTap2D(shadowMap, projCoords.xy + uvOff, refZ + dot(dzDuv, uvOff));
    }
    return 1.0 - (shadow / 16.0);
}

vec3 ApplyShadowUvNormalOffset(vec3 projCoords, mat4 lightMatrix, vec3 normal, float slopeFactor, vec2 texelSize) {
    // Vertical receivers under a near-vertical light occupy ~1 shadow texel (the caster silhouette).
    // Depth bias cannot move the sample off that sliver; a UV push along N.xy can.
    vec2 nUV = (lightMatrix * vec4(normal, 0.0)).xy;
    float nLen = length(nUV);
    if (nLen < 1e-6)
        return projCoords;
    float taps = 4.0 + 4.0 * slopeFactor;
    vec2 uvShift = (nUV / nLen) * min(texelSize * taps, vec2(0.02));
    projCoords.xy += uvShift;
    return projCoords;
}

float SampleCascadeShadowSlice(sampler2DArrayShadow shadowMap, int cascadeIndex, vec3 fragPos, vec3 vertexN, vec3 lightDir) {
    vec3 normal = ShadowFaceNormal(fragPos, vertexN);
    float NdotL = max(dot(normal, lightDir), 0.0);
    float slopeFactor = max(1.0 - NdotL, 0.0);
    float tanTheta = ShadowSlopeTan(normal, lightDir);

    float constBias  = u_ShadowParams.x;
    float slopeBias  = u_ShadowParams.y;
    float normalBias = u_ShadowParams.z;

    vec3 normalOffset = normal * (normalBias * slopeFactor);
    vec4 biasedFragPos = vec4(fragPos + normalOffset, 1.0);
    vec4 fragPosLightSpace = u_LightSpaceMatrices[cascadeIndex] * biasedFragPos;
    if (fragPosLightSpace.w <= 0.0)
        return 0.0;

    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    vec2 texelSize = vec2(1.0) / vec2(textureSize(shadowMap, 0).xy);
    projCoords = ApplyShadowUvNormalOffset(projCoords, u_LightSpaceMatrices[cascadeIndex], normal, slopeFactor, texelSize);

    if (projCoords.z > 1.0 || projCoords.z < 0.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float bias = constBias + slopeBias * tanTheta;
    return FilterShadowArray(shadowMap, cascadeIndex, projCoords, projCoords.z - bias);
}

float SampleSpotShadowMap(sampler2DShadow shadowMap, vec3 fragPos, vec3 vertexN, vec3 lightDir) {
    vec3 normal = ShadowFaceNormal(fragPos, vertexN);
    float NdotL = max(dot(normal, lightDir), 0.0);
    float slopeFactor = max(1.0 - NdotL, 0.0);
    float tanTheta = ShadowSlopeTan(normal, lightDir);

    float constBias  = u_ShadowParams.x * 1.5;
    float slopeBias  = u_ShadowParams.y * 1.5;
    float normalBias = u_ShadowParams.z;

    vec3 normalOffset = normal * (normalBias * slopeFactor);
    vec4 fragPosSpotLightSpace = u_SpotLightSpaceMatrix * vec4(fragPos + normalOffset, 1.0);
    if (fragPosSpotLightSpace.w <= 0.0)
        return 0.0;

    vec3 projCoords = fragPosSpotLightSpace.xyz / fragPosSpotLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    vec2 texelSize = vec2(1.0) / vec2(textureSize(shadowMap, 0));
    projCoords = ApplyShadowUvNormalOffset(projCoords, u_SpotLightSpaceMatrix, normal, slopeFactor, texelSize);

    if (projCoords.z > 1.0 || projCoords.z < 0.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float bias = constBias + slopeBias * tanTheta;
    return FilterShadow2D(shadowMap, projCoords, projCoords.z - bias);
}

float CalculateCascadedDirectionalShadow(vec3 fragPos, vec3 normal, vec3 lightDir, out int outCascadeIndex) {
    if (u_UseShadows == 0) {
        outCascadeIndex = 0;
        return 0.0;
    }

    // Planar view depth along camera forward axis (matches perspective frustum slices exactly)
    float depth = dot(fragPos - u_ViewPos.xyz, u_CameraForward.xyz);
    int cascadeIndex = 3;
    if (depth < u_CascadeSplits.x) {
        cascadeIndex = 0;
    } else if (depth < u_CascadeSplits.y) {
        cascadeIndex = 1;
    } else if (depth < u_CascadeSplits.z) {
        cascadeIndex = 2;
    }
    outCascadeIndex = cascadeIndex;

    float shadow = SampleCascadeShadowSlice(u_CascadeShadowMap, cascadeIndex, fragPos, normal, lightDir);

    // Cascade smooth blend transition
    float blendWidth = u_ShadowParams.w;
    if (blendWidth > 0.001 && cascadeIndex < 3) {
        float splitDist = (cascadeIndex == 0) ? u_CascadeSplits.x :
                          (cascadeIndex == 1) ? u_CascadeSplits.y : u_CascadeSplits.z;
        float blendThreshold = splitDist * (1.0 - blendWidth);
        if (depth > blendThreshold) {
            float nextShadow = SampleCascadeShadowSlice(u_CascadeShadowMap, cascadeIndex + 1, fragPos, normal, lightDir);
            float alpha = clamp((depth - blendThreshold) / max(splitDist - blendThreshold, 0.001), 0.0, 1.0);
            shadow = mix(shadow, nextShadow, alpha);
        }
    }

    // Soft fadeout at far shadow distance (split3 / cascadeSplits.w)
    if (depth > u_CascadeSplits.w) {
        float fade = clamp((depth - u_CascadeSplits.w) / 15.0, 0.0, 1.0);
        shadow = mix(shadow, 0.0, fade);
    }

    return shadow;
}

float CalculateSpotShadow(vec3 fragPos, vec3 normal, vec3 lightDir) {
    if (u_UseSpotShadows == 0) return 0.0;
    return SampleSpotShadowMap(u_SpotShadowMap, fragPos, normal, lightDir);
}

// Project the fragment through a mirrored camera used to fill a planar map.
// Screen-space UVs turn every mirror into a zoomed portal of the framebuffer.
vec2 PlanarReflectionUVFrom(mat4 planarVP, vec3 worldPos, vec3 N, float roughness, out float clipW) {
    vec4 planarClip = planarVP * vec4(worldPos, 1.0);
    clipW = planarClip.w;
    float invW = 1.0 / max(abs(planarClip.w), 1e-5);
    vec2 uv = planarClip.xy * invW * 0.5 + 0.5;
    uv += vec2(N.x, N.z) * 0.03 * (1.0 - roughness);
    return uv;
}

vec2 PlanarReflectionUV(vec3 worldPos, vec3 N, float roughness, out float clipW) {
    return PlanarReflectionUVFrom(u_PlanarViewProjection, worldPos, N, roughness, clipW);
}

vec3 SampleOnePlanarLi(sampler2D planarMap, mat4 planarVP, vec3 planeN, float planeD, vec3 worldPos, vec3 N,
                       float roughness, out float weight) {
    float clipW = 0.0;
    vec2 uv = PlanarReflectionUVFrom(planarVP, worldPos, N, roughness, clipW);
    float inFront = step(1e-5, clipW);
    float inBounds = float(uv.x > 0.0 && uv.x < 1.0 && uv.y > 0.0 && uv.y < 1.0);
    vec3 n = normalize(planeN);
    float planeAlign = smoothstep(0.35, 0.70, abs(dot(N, n)));
    // Capture is a flat mirrored camera. Off-plane meshes (spheres, car bodies) must keep IBL.
    float planeDist = abs(dot(worldPos, n) + planeD);
    float onPlane = 1.0 - smoothstep(0.08, 0.40, planeDist);
    weight = clamp(1.0 - roughness * 1.1, 0.0, 1.0) * inFront * inBounds * planeAlign * onPlane;
    const float MAX_PLANAR_LOD = 4.0;
    return textureLod(planarMap, clamp(uv, 0.001, 0.999), roughness * MAX_PLANAR_LOD).rgb;
}

vec3 SamplePlanarReflectionLi(vec3 worldPos, vec3 N, float roughness, out float weight) {
    float w0 = 0.0;
    vec3 li0 = SampleOnePlanarLi(u_PlanarReflectionMap, u_PlanarViewProjection, u_PlanarPlaneNormal,
                                 u_PlanarPlaneDistance, worldPos, N, roughness, w0);
    float w1 = 0.0;
    vec3 li1 = vec3(0.0);
    if (u_UsePlanarReflection1 == 1)
        li1 = SampleOnePlanarLi(u_PlanarReflectionMap1, u_PlanarViewProjection1, u_PlanarPlaneNormal1,
                                u_PlanarPlaneDistance1, worldPos, N, roughness, w1);
    weight = max(w0, w1);
    return w1 > w0 ? li1 : li0;
}
