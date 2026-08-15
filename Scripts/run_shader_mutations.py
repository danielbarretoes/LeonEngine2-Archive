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
        "target": "vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);",
        "replacement": "vec3 kD = (vec3(1.0) - kS) * metallic;"
    },
    {
        "id": "MUT_PBR_E",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Cook-Torrance: Eliminate Diffuse Reflectance in Direct Lighting",
        "target": "Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);",
        "replacement": "Lo += specular * radiance * NdotL * (1.0 - shadow);"
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
        "target": "float attenuation = (factor * factor) / (distSq + 1.0);",
        "replacement": "float attenuation = 1.0 / (distSq + 1.0);"
    },
    {
        "id": "MUT_PBR_H",
        "file": "Engine/Assets/Shaders/PBR_Lit.glsl",
        "name": "Spot Light: Invert Conical Cutoff Penumbra",
        "target": "float spotFactor  = smoothstep(0.0, 1.0, clamp((theta - outerCutOff) / max(epsilon, 0.0001), 0.0, 1.0));",
        "replacement": "float spotFactor  = 1.0 - smoothstep(0.0, 1.0, clamp((theta - outerCutOff) / max(epsilon, 0.0001), 0.0, 1.0));"
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
