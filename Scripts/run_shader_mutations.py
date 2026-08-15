#!/usr/bin/env python3
"""
LeonEngine2 - Automated GLSL Shader Mutation Testing Suite
Executes controlled mutations against Engine/Assets/Shaders/PBR_Lit.glsl
and verifies that the GPU Headless test suite catches every regression.
"""

import os
import sys
import subprocess
import time
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
EXE_PATH = ROOT_DIR / "build" / "Tests" / "RendererTests.exe"

MUTATIONS = [
    # --- PBR / IBL Shaders (PBR_Lit.glsl) ---
    {
        "id": "MUT_PBR_A",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Fresnel-Schlick: Exponent 5.0 -> 4.0",
        "target": "return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);",
        "replacement": "return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 4.0);"
    },
    {
        "id": "MUT_PBR_B",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "GGX NDF: Drop Denominator Squared Power",
        "target": "denom = PI * denom * denom;",
        "replacement": "denom = PI * denom;"
    },
    {
        "id": "MUT_PBR_C",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Smith Geometry: Drop G2 Masking (G1 only)",
        "target": "return ggx1 * ggx2;",
        "replacement": "return ggx1;"
    },
    {
        "id": "MUT_PBR_D",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Energy Conservation: Invert Metallic in kD",
        "target": "kD *= 1.0 - metallic;",
        "replacement": "kD *= metallic;"
    },
    {
        "id": "MUT_PBR_E",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Cook-Torrance: Eliminate Diffuse Reflectance in Direct Lighting",
        "target": "Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - totalShadow);",
        "replacement": "Lo += specular * radiance * NdotL * (1.0 - totalShadow);"
    },
    {
        "id": "MUT_PBR_F",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Directional Light: Ignore Cosine Angle NdotL",
        "target": "float NdotL = max(dot(N, L), 0.0);",
        "replacement": "float NdotL = 1.0;"
    },
    {
        "id": "MUT_PBR_G",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Point Light: Disable UE4 Inverse-Square Radius Attenuation",
        "target": "float attenuation = (windowFactor * windowFactor) / (distance * distance + 1.0);",
        "replacement": "float attenuation = 1.0 / (distance * distance + 1.0);"
    },
    {
        "id": "MUT_PBR_H",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Spot Light: Invert Conical Cutoff Penumbra",
        "target": "spotIntensity = spotIntensity * spotIntensity * (3.0 - 2.0 * spotIntensity);",
        "replacement": "spotIntensity = 1.0 - spotIntensity * spotIntensity * (3.0 - 2.0 * spotIntensity);"
    },
    {
        "id": "MUT_PBR_I",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "IBL Pipeline: Zero Out Diffuse Irradiance",
        "target": "diffuseIBL = irradiance * albedo;",
        "replacement": "diffuseIBL = vec3(0.0);"
    },
    {
        "id": "MUT_PBR_J",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "IBL Pipeline: Force Dummy Split-Sum BRDF LUT",
        "target": "envBRDF = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;",
        "replacement": "envBRDF = vec2(0.0, 0.0);"
    },
    {
        "id": "MUT_PBR_K",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Planar Reflections: Invert Enable Toggle Logic",
        "target": "if (u_UsePlanarReflection == 1) {",
        "replacement": "if (u_UsePlanarReflection == 0) {"
    },
    {
        "id": "MUT_PBR_L",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "GGX NDF: Invert Roughness Scaling in Alpha",
        "target": "float a = roughness * roughness;",
        "replacement": "float a = 1.0 - roughness * roughness;"
    },
    {
        "id": "MUT_PBR_M",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Direct Light: Corrupt PI Normalization in Lambertian Diffuse",
        "target": "kD * albedo / PI",
        "replacement": "kD * albedo"
    },
    {
        "id": "MUT_PBR_N",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Fresnel-Schlick: Invert Grazing Angle Limit (F(pi/2) -> 0.0)",
        "target": "return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);",
        "replacement": "return F0 * (1.0 - pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0));"
    },
    {
        "id": "MUT_PBR_O",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "HDR Output: Drop Direct Radiance Lo Accumulator",
        "target": "vec3 hdrColor = ambient + Lo + emissive;",
        "replacement": "vec3 hdrColor = ambient + emissive;"
    },

    # --- Material Subsystem Mutations (PBR_Lit.glsl) ---
    {
        "id": "MUT_MATERIAL_A",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Normal Mapping: Corrupt Tangent Normal Range Decoding (* 1.0 - 1.0)",
        "target": "vec3 normalTS = normalSample * 2.0 - 1.0;",
        "replacement": "vec3 normalTS = normalSample * 1.0 - 1.0;"
    },
    {
        "id": "MUT_MATERIAL_B",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Normal Mapping: Drop u_NormalScale Modulation",
        "target": "normalTS.xy *= u_NormalScale;",
        "replacement": "normalTS.xy *= 1.0;"
    },
    {
        "id": "MUT_MATERIAL_C",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Normal Mapping: Force Normal Map Flag When Disabled",
        "target": "if (u_UseNormalMap == 1) {",
        "replacement": "if (true) {"
    },
    {
        "id": "MUT_MATERIAL_D",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Albedo sRGB Decompression: Drop Gamma 2.2 Power (Raw Linear Leak)",
        "target": "pow(albedoSample.rgb, vec3(2.2))",
        "replacement": "albedoSample.rgb"
    },
    {
        "id": "MUT_MATERIAL_E",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Albedo: Drop Scalar Multiplier u_AlbedoColor",
        "target": "vec3 albedo = u_AlbedoColor * ((u_UseAlbedoMap == 1) ? pow(albedoSample.rgb, vec3(2.2)) : vec3(1.0));",
        "replacement": "vec3 albedo = (u_UseAlbedoMap == 1) ? pow(albedoSample.rgb, vec3(2.2)) : vec3(1.0);"
    },
    {
        "id": "MUT_MATERIAL_F",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Metallic: Ignore Texture Map Sampling",
        "target": "float metallic = u_Metallic * ((u_UseMetallicMap == 1) ? texture(u_MetallicMap, uv).r : 1.0);",
        "replacement": "float metallic = u_Metallic;"
    },
    {
        "id": "MUT_MATERIAL_G",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Roughness: Ignore Texture Map Sampling",
        "target": "float roughness = u_Roughness * ((u_UseRoughnessMap == 1) ? texture(u_RoughnessMap, uv).r : 1.0);",
        "replacement": "float roughness = u_Roughness;"
    },
    {
        "id": "MUT_MATERIAL_H",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "AO: Ignore u_OcclusionStrength Modulation",
        "target": "ao *= mix(1.0, aoSample, u_OcclusionStrength);",
        "replacement": "ao *= aoSample;"
    },
    {
        "id": "MUT_MATERIAL_I",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Emissive: Drop sRGB Decompression (Sample as Linear)",
        "target": "pow(texture(u_EmissiveMap, uv).rgb, vec3(2.2))",
        "replacement": "texture(u_EmissiveMap, uv).rgb"
    },
    {
        "id": "MUT_MATERIAL_J",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Emissive: Multiply by Surface Angle (Corrupt Decoupling)",
        "target": "vec3 emissive = u_EmissiveColor * u_EmissiveIntensity * emissiveMapSample;",
        "replacement": "vec3 emissive = u_EmissiveColor * u_EmissiveIntensity * emissiveMapSample * max(dot(N, vec3(0,1,0)), 0.0);"
    },
    {
        "id": "MUT_MATERIAL_K",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Alpha Masking: Invert Cutoff Inequality (< -> >)",
        "target": "if (u_AlphaMode == 1 && alpha < u_AlphaCutoff) {",
        "replacement": "if (u_AlphaMode == 1 && alpha > u_AlphaCutoff) {"
    },
    {
        "id": "MUT_MATERIAL_L",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Alpha Masking: Discard Unconditionally when AlphaMode Active",
        "target": "if (u_AlphaMode == 1 && alpha < u_AlphaCutoff) {",
        "replacement": "if (u_AlphaMode == 1) {"
    },
    {
        "id": "MUT_MATERIAL_M",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "UV Transform: Drop u_UVOffset from Coordinate Mapping",
        "target": "vec2 uv = v_TexCoord * u_UVTiling + u_UVOffset;",
        "replacement": "vec2 uv = v_TexCoord * u_UVTiling;"
    },
    {
        "id": "MUT_MATERIAL_N",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "UV Transform: Drop u_UVTiling Multiplier",
        "target": "vec2 uv = v_TexCoord * u_UVTiling + u_UVOffset;",
        "replacement": "vec2 uv = v_TexCoord + u_UVOffset;"
    },
    {
        "id": "MUT_MATERIAL_O",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Tangent Space: Drop Gram-Schmidt Orthogonalization on Tangent",
        "target": "T = normalize(T - N * dot(N, T));",
        "replacement": "// T = normalize(T - N * dot(N, T));"
    },
    {
        "id": "MUT_MATERIAL_P",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Tangent Space: Corrupt Bitangent Orthogonalization Sign",
        "target": "B = normalize(B - N * dot(N, B) - T * dot(T, B));",
        "replacement": "B = normalize(B + N * dot(N, B) + T * dot(T, B));"
    },

    # --- Advanced Shadow System Shaders (v0.9.0) ---
    {
        "id": "MUT_SHADOW_A",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Cascade Selection: Invert Split 0 Depth Threshold",
        "target": "if (depth < u_CascadeSplits.x) {",
        "replacement": "if (depth > u_CascadeSplits.x) {"
    },
    {
        "id": "MUT_SHADOW_B",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Cascade Selection: Invert Cascade Index Initialization",
        "target": "int cascadeIndex = 3;",
        "replacement": "int cascadeIndex = 0;"
    },
    {
        "id": "MUT_SHADOW_C",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Normal Offset Bias: Invert Surface Normal Direction",
        "target": "vec3 normalOffset = normal * (normalBias * slopeFactor);",
        "replacement": "vec3 normalOffset = -normal * (normalBias * slopeFactor);"
    },
    {
        "id": "MUT_SHADOW_D",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Depth Bias: Drop Constant Bias Offset",
        "target": "float bias = constBias + slopeBias * slopeFactor;",
        "replacement": "float bias = slopeBias * slopeFactor;"
    },
    {
        "id": "MUT_SHADOW_E",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Depth Bias: Drop Dynamic Slope Scale Factor",
        "target": "float bias = constBias + slopeBias * slopeFactor;",
        "replacement": "float bias = constBias;"
    },
    {
        "id": "MUT_SHADOW_F",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Hard Shadow: Invert Hardware Depth Comparison Output",
        "target": "vec4 coord = vec4(projCoords.xy, float(cascadeIndex), currentDepth);\n        shadow = texture(shadowMap, coord);\n        return 1.0 - shadow;",
        "replacement": "vec4 coord = vec4(projCoords.xy, float(cascadeIndex), currentDepth);\n        shadow = texture(shadowMap, coord);\n        return shadow;"
    },
    {
        "id": "MUT_SHADOW_G",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "PCF 3x3: Corrupt Kernel Tap Divisor (9.0 -> 3.0)",
        "target": "return 1.0 - (shadow / 9.0);",
        "replacement": "return 1.0 - (shadow / 3.0);"
    },
    {
        "id": "MUT_SHADOW_H",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "PCF 5x5: Corrupt Kernel Tap Divisor (25.0 -> 5.0)",
        "target": "return 1.0 - (shadow / 25.0);",
        "replacement": "return 1.0 - (shadow / 5.0);"
    },
    {
        "id": "MUT_SHADOW_I",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Poisson Disk: Corrupt Kernel Sample Normalization Divisor",
        "target": "return 1.0 - (shadow / 16.0);",
        "replacement": "return 1.0 - (shadow / 4.0);"
    },
    {
        "id": "MUT_SHADOW_J",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Cascade Blending: Zero Out Interpolation Weight",
        "target": "shadow = mix(shadow, nextShadow, alpha);",
        "replacement": "shadow = nextShadow;"
    },
    {
        "id": "MUT_SHADOW_K",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Spot Shadow: Force Full Occlusion",
        "target": "return SampleSpotShadowMap(u_SpotShadowMap, fragPos, normal, lightDir);",
        "replacement": "return 1.0;"
    },
    {
        "id": "MUT_SHADOW_L",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Contact Shadows: Default Unoccluded Factor to Zero",
        "target": "float shadowFactor = 1.0;",
        "replacement": "float shadowFactor = 0.0;"
    },
    {
        "id": "MUT_SHADOW_M",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Shadow Integration: Drop Contact Shadow Factor Combination",
        "target": "float totalShadow = 1.0 - (1.0 - dirShadow) * contactShadowFactor;",
        "replacement": "float totalShadow = 1.0 - (1.0 - dirShadow) * 0.0;"
    },
    {
        "id": "MUT_SHADOW_N",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Shadow False-Color Debug: Corrupt Cascade 0 Color Palette",
        "target": "vec3(1.0, 0.15, 0.15),",
        "replacement": "vec3(0.0, 0.15, 0.15),"
    },
    {
        "id": "MUT_SHADOW_O",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Far Shadow Fadeout: Invert Fadeout Toward Shadowed",
        "target": "shadow = mix(shadow, 0.0, fade);",
        "replacement": "shadow = mix(shadow, 1.0, fade);"
    },
    {
        "id": "MUT_SHADOW_P",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Poisson Disk: Bypass 16-Tap Sampling Accumulation Loop",
        "target": "for (int i = 0; i < 16; ++i) {",
        "replacement": "for (int i = 0; i < 0; ++i) {"
    },

    # --- Bloom Shaders ---
    {
        "id": "MUT_BLOOM_A",
        "file": "Engine/Assets/Shaders/BloomBrightPass.glsl",
        "name": "Bloom Bright Pass: Drop Soft-Knee Quadratic Calculation",
        "target": "soft = (soft * soft) / (4.0 * knee + 0.00001);",
        "replacement": "soft = soft;"
    },
    {
        "id": "MUT_BLOOM_B",
        "file": "Engine/Assets/Shaders/BloomBrightPass.glsl",
        "name": "Bloom Bright Pass: Ignore Threshold Extraction (Pass Through All Radiance)",
        "target": "vec3 brightColor = color * max(contribution, 0.0);",
        "replacement": "vec3 brightColor = color;"
    },
    {
        "id": "MUT_BLOOM_C",
        "file": "Engine/Assets/Shaders/BloomDownsample.glsl",
        "name": "Bloom Downsample: Corrupt Jimenez 13-Tap Center Box Weights",
        "target": "vec3 centerBox = (d + e + i + j) * 0.125;",
        "replacement": "vec3 centerBox = (d + e + i + j) * 0.5;"
    },
    {
        "id": "MUT_BLOOM_D",
        "file": "Engine/Assets/Shaders/BloomUpsample.glsl",
        "name": "Bloom Upsample: Corrupt 3x3 Tent Filter Center Weight",
        "target": "vec3 upsample = e * 4.0;",
        "replacement": "vec3 upsample = e * 16.0;"
    },

    # --- Tone Mapping Shaders ---
    {
        "id": "MUT_TONE_A",
        "file": "Engine/Assets/Shaders/ToneMapping.glsl",
        "name": "ACES Filmic: Corrupt Linear Numerator Term (b = 0.03 -> 0.35)",
        "target": "float b = 0.03;",
        "replacement": "float b = 0.35;"
    },
    {
        "id": "MUT_TONE_B",
        "file": "Engine/Assets/Shaders/ToneMapping.glsl",
        "name": "Tone Mapping: Invert Camera Exposure Multiplier (Scale Down Instead of Up)",
        "target": "vec3 exposedColor = hdrColor * max(u_Exposure, 0.0);",
        "replacement": "vec3 exposedColor = hdrColor / max(u_Exposure, 0.001);"
    },
    {
        "id": "MUT_TONE_C",
        "file": "Engine/Assets/Shaders/ToneMapping.glsl",
        "name": "Tone Mapping: Invert Gamma 2.2 Correction Exponent",
        "target": "vec3 srgbColor = pow(ldrColor, vec3(1.0 / max(u_Gamma, 0.0001)));",
        "replacement": "vec3 srgbColor = pow(ldrColor, vec3(u_Gamma));"
    },

    # --- FXAA Shaders ---
    {
        "id": "MUT_FXAA_A",
        "file": "Engine/Assets/Shaders/FXAA.glsl",
        "name": "FXAA: Disable Contrast Edge Threshold Early-Exit",
        "target": "if (lumaRange < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD_MAX)) {",
        "replacement": "if (false) {"
    },
    {
        "id": "MUT_FXAA_B",
        "file": "Engine/Assets/Shaders/FXAA.glsl",
        "name": "FXAA: Invert Horizontal / Vertical Edge Orientation Decision",
        "target": "bool isHorizontal = (edgeHorz >= edgeVert);",
        "replacement": "bool isHorizontal = (edgeHorz < edgeVert);"
    }
]

def run_tests():
    cmd = [str(EXE_PATH), "-ts=*Shader*"]
    try:
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=10)
        return proc.returncode == 0, proc.stdout
    except Exception as e:
        return False, str(e)

def main():
    print("=" * 80)
    print("   LeonEngine2 - GLSL Shader Mutation Testing Suite")
    print("=" * 80)

    # Baseline check
    passed, out = run_tests()
    if not passed:
        print("[ERROR] Baseline test suite failed before mutation testing!")
        print(out)
        sys.exit(1)
    print("[BASELINE] Baseline shader test passed with 0 failures.\n")

    # Cache original contents of all relevant shader files
    shader_files = set(m["file"] for m in MUTATIONS)
    originals = {}
    for rel_path in shader_files:
        full_path = ROOT_DIR / rel_path
        if not full_path.exists():
            print(f"[ERROR] Shader path not found: {full_path}")
            sys.exit(1)
        originals[rel_path] = full_path.read_text(encoding="utf-8")

    results = []

    try:
        for m in MUTATIONS:
            m_id = m["id"]
            m_name = m["name"]
            rel_file = m["file"]
            target = m["target"]
            replacement = m["replacement"]
            full_path = ROOT_DIR / rel_file

            orig_text = originals[rel_file]

            if target not in orig_text:
                print(f"[WARN] Target string not found for {m_id}: '{target[:30]}...' in {rel_file}")
                results.append((m_id, m_name, "SKIPPED_NOT_FOUND", "N/A"))
                continue

            # Apply mutation
            mutated_code = orig_text.replace(target, replacement, 1)
            full_path.write_text(mutated_code, encoding="utf-8")

            print(f"[TESTING] {m_id}: {m_name}")
            test_passed, output = run_tests()

            # Immediately restore original shader code
            full_path.write_text(orig_text, encoding="utf-8")

            if not test_passed:
                # Caught!
                first_fail = "Unknown Test"
                for line in output.splitlines():
                    if "TEST CASE:" in line:
                        first_fail = line.split("TEST CASE:")[-1].strip()
                        break
                print(f"  --> [CAUGHT] Shader mutation detected by GPU test!")
                print(f"      TEST CASE:  {first_fail}\n")
                results.append((m_id, m_name, "CAUGHT", first_fail))
            else:
                print(f"  --> [ESCAPE / WEAK] Shader mutation NOT detected! GPU test passed.\n")
                results.append((m_id, m_name, "ESCAPED", "N/A"))

    finally:
        # Guarantee all original shader files are restored
        for rel_path, orig_text in originals.items():
            (ROOT_DIR / rel_path).write_text(orig_text, encoding="utf-8")
        print("[CLEANUP] Restored all original shader files.\n")

    # Final summary table
    print("=" * 80)
    print("   GLSL SHADER MUTATION TESTING SUMMARY")
    print("=" * 80)
    caught_count = sum(1 for r in results if r[2] == "CAUGHT")
    total_tested = len(results)
    print(f"Mutations Tested:  {total_tested}")
    print(f"Mutations Caught:  {caught_count} / {total_tested} ({caught_count / total_tested * 100:.1f}%)")
    print(f"Mutations Escaped: {total_tested - caught_count}")
    print("-" * 80)
    for r in results:
        status_str = "[PASS: CAUGHT] " if r[2] == "CAUGHT" else "[FAIL: ESCAPED]"
        print(f"{r[0]:<18} | {status_str} | {r[1]}")
    print("=" * 80 + "\n")

    return 0 if caught_count == total_tested else 1

if __name__ == "__main__":
    sys.exit(main())
