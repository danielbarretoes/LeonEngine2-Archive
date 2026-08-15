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
SHADER_PATH = ROOT_DIR / "Engine" / "Assets" / "Shaders" / "PBR_Lit.glsl"
RUN_TESTS_SCRIPT = ROOT_DIR / "Scripts" / "run_tests.py"
EXE_PATH = ROOT_DIR / "build" / "Tests" / "RendererTests.exe"

MUTATIONS = [
    {
        "id": "MUTATION_SHADER_A",
        "name": "Fresnel-Schlick: Exponent 5.0 -> 4.0",
        "expected_test": "Exact Oblique Angle Test (cosTheta = 0.5)",
        "target": "return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);",
        "replacement": "return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 4.0);"
    },
    {
        "id": "MUTATION_SHADER_B",
        "name": "GGX NDF: Drop Denominator Squared Power",
        "expected_test": "Exact Hardware GPU Numerical Output vs Analytical Formula",
        "target": "denom = PI * denom * denom;",
        "replacement": "denom = PI * denom;"
    },
    {
        "id": "MUTATION_SHADER_C",
        "name": "Smith Geometry: Drop G2 Masking (G1 only)",
        "expected_test": "Exact Hardware GPU Numerical Output vs Analytical Formula",
        "target": "return ggx1 * ggx2;",
        "replacement": "return ggx1;"
    },
    {
        "id": "MUTATION_SHADER_D",
        "name": "Energy Conservation: Invert Metallic in kD",
        "expected_test": "Exact Hardware GPU Numerical Output vs Analytical Formula",
        "target": "vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);",
        "replacement": "vec3 kD = (vec3(1.0) - kS) * metallic;"
    },
    {
        "id": "MUTATION_SHADER_E",
        "name": "Cook-Torrance: Eliminate Diffuse Reflectance in Direct Lighting",
        "expected_test": "Exact Hardware GPU Numerical Output vs Analytical Formula",
        "target": "Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);",
        "replacement": "Lo += specular * radiance * NdotL * (1.0 - shadow);"
    },
    {
        "id": "MUTATION_SHADER_F",
        "name": "Directional Light: Ignore Cosine Angle NdotL",
        "expected_test": "Directional Light Angle Response (NdotL in {1.0, 0.5, 0.0})",
        "target": "float NdotL = max(dot(N, L), 0.0);",
        "replacement": "float NdotL = 1.0;"
    },
    {
        "id": "MUTATION_SHADER_G",
        "name": "Point Light: Disable UE4 Inverse-Square Radius Attenuation",
        "expected_test": "Point Light UE4 Inverse-Square Radius Attenuation Response",
        "target": "float attenuation = (factor * factor) / (distSq + 1.0);",
        "replacement": "float attenuation = 1.0 / (distSq + 1.0);"
    },
    {
        "id": "MUTATION_SHADER_H",
        "name": "Spot Light: Invert Conical Cutoff Penumbra",
        "expected_test": "Spot Light Conical Cutoff and Smoothstep Penumbra Response",
        "target": "float spotFactor  = smoothstep(0.0, 1.0, clamp((theta - outerCutOff) / max(epsilon, 0.0001), 0.0, 1.0));",
        "replacement": "float spotFactor  = 1.0 - smoothstep(0.0, 1.0, clamp((theta - outerCutOff) / max(epsilon, 0.0001), 0.0, 1.0));"
    },
    {
        "id": "MUTATION_SHADER_I",
        "name": "IBL Pipeline: Zero Out Diffuse Irradiance",
        "expected_test": "PBR_Lit.glsl Real Hardware GPU Image-Based Lighting Pipeline",
        "target": "diffuseIBL = irradiance * albedo;",
        "replacement": "diffuseIBL = vec3(0.0);"
    },
    {
        "id": "MUTATION_SHADER_J",
        "name": "IBL Pipeline: Force Dummy Split-Sum BRDF LUT",
        "expected_test": "PBR_Lit.glsl Real Hardware GPU Image-Based Lighting Pipeline",
        "target": "envBRDF = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;",
        "replacement": "envBRDF = vec2(0.0, 0.0);"
    },
    {
        "id": "MUTATION_SHADER_K",
        "name": "Planar Reflections: Invert Enable Toggle Logic",
        "expected_test": "PBR_Lit.glsl Real-Time Planar Reflection Integration (Toggle 0 vs 1)",
        "target": "if (u_UsePlanarReflection == 1) {",
        "replacement": "if (u_UsePlanarReflection == 0) {"
    },
    {
        "id": "MUTATION_SHADER_L",
        "name": "GGX NDF: Invert Roughness Scaling in Alpha",
        "expected_test": "GGX NDF Hardware On-Axis Monotonic Scaling with Roughness",
        "target": "float a = roughness * roughness;",
        "replacement": "float a = 1.0 - roughness * roughness;"
    },
    {
        "id": "MUTATION_SHADER_M",
        "name": "Direct Light: Corrupt PI Normalization in Lambertian Diffuse",
        "expected_test": "Exact Hardware GPU Numerical Output vs Analytical Formula",
        "target": "kD * albedo / PI",
        "replacement": "kD * albedo"
    },
    {
        "id": "MUTATION_SHADER_N",
        "name": "Fresnel-Schlick: Invert Grazing Angle Limit (F(pi/2) -> 0.0)",
        "expected_test": "Exact Hardware GPU Numerical Output vs Analytical Formula",
        "target": "return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);",
        "replacement": "return F0 * (1.0 - pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0));"
    },
    {
        "id": "MUTATION_SHADER_O",
        "name": "HDR Output: Drop Direct Radiance Lo Accumulator",
        "expected_test": "Exact Hardware GPU Numerical Output vs Analytical Formula",
        "target": "vec3 hdrColor = ambient + Lo + emissive;",
        "replacement": "vec3 hdrColor = ambient + emissive;"
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

    if not SHADER_PATH.exists():
        print(f"[ERROR] Shader path not found: {SHADER_PATH}")
        sys.exit(1)

    original_code = SHADER_PATH.read_text(encoding="utf-8")

    # Baseline check
    passed, out = run_tests()
    if not passed:
        print("[ERROR] Baseline test suite failed before mutation testing!")
        print(out)
        sys.exit(1)
    print("[BASELINE] Baseline shader test passed with 0 failures.\n")

    results = []

    for m in MUTATIONS:
        m_id = m["id"]
        m_name = m["name"]
        target = m["target"]
        replacement = m["replacement"]

        if target not in original_code:
            print(f"[WARN] Target string not found for {m_id}: '{target[:30]}...'")
            results.append((m_id, m_name, "SKIPPED_NOT_FOUND", "N/A"))
            continue

        # Apply mutation
        mutated_code = original_code.replace(target, replacement, 1)
        SHADER_PATH.write_text(mutated_code, encoding="utf-8")

        print(f"[TESTING] {m_id}: {m_name}")
        test_passed, output = run_tests()

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

    # Restore original shader code
    SHADER_PATH.write_text(original_code, encoding="utf-8")
    print("[CLEANUP] Restored original PBR_Lit.glsl")

    # Final summary table
    print("\n" + "=" * 80)
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
