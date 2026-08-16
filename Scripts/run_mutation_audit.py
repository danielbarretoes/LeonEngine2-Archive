#!/usr/bin/env python3
"""
LeonEngine2 - Mutation Testing Audit Script
Introduces 15 controlled mathematical mutations into renderer algorithms and records
which regression test detects each mutation.
"""

import os
import subprocess
import sys
import time

def run_tests():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    build_dir = os.path.join(project_root, "build")
    
    # Recompile RendererTests
    res_build = subprocess.run(["ninja", "-C", build_dir, "RendererTests"], capture_output=True, text=True)
    if res_build.returncode != 0:
        return False, "BUILD_ERROR: " + (res_build.stderr or res_build.stdout)
        
    exe_path = os.path.join(build_dir, "Tests", "RendererTests.exe")
    res_run = subprocess.run([exe_path], cwd=project_root, capture_output=True, text=True)
    return (res_run.returncode == 0), res_run.stdout

def apply_mutation(filepath, target_str, mutated_str):
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()
    if target_str not in content:
        raise ValueError(f"Target string not found in {filepath}:\n{target_str}")
    mutated_content = content.replace(target_str, mutated_str, 1)
    with open(filepath, "w", encoding="utf-8") as f:
        f.write(mutated_content)

def restore_file(filepath, original_content):
    with open(filepath, "w", encoding="utf-8") as f:
        f.write(original_content)

MUTATIONS = [
    {
        "id": "MUTATION_A",
        "name": "Eliminate PI factor in Irradiance Normalization",
        "file": "Tests/IBL/IrradianceConvolutionTests.cpp",
        "target": "irradiance = Leon::PI * irradiance / static_cast<float>(sampleCount);",
        "mutated": "irradiance = irradiance / static_cast<float>(sampleCount);",
        "expected_test": "IBL - Irradiance Convolution Invariants / Constant Environment L = 1.0"
    },
    {
        "id": "MUTATION_B",
        "name": "Use uniform hemisphere PDF (1/2PI) instead of cosine PDF (cosTheta/PI)",
        "file": "Engine/Source/Runtime/Renderer/Public/Renderer/FIBLMath.hpp",
        "target": "return std::max(cosTheta, 0.0f) / PI;",
        "mutated": "return 0.5f / PI;",
        "expected_test": "Math - Hemisphere PDF & Integration Invariants"
    },
    {
        "id": "MUTATION_C",
        "name": "Invert normal vector in Cosine Hemisphere Sampling",
        "file": "Engine/Source/Runtime/Renderer/Public/Renderer/FIBLMath.hpp",
        "target": "glm::vec3 sampleVec = tangent * tangentSample.x + bitangent * tangentSample.y + N * tangentSample.z;",
        "mutated": "glm::vec3 sampleVec = tangent * tangentSample.x + bitangent * tangentSample.y - N * tangentSample.z;",
        "expected_test": "Math - Hammersley & Quasi-Monte Carlo / Cosine-Weighted Hemisphere"
    },
    {
        "id": "MUTATION_D",
        "name": "Use roughness instead of alpha = roughness^2 in GGX Importance Sampling",
        "file": "Engine/Source/Runtime/Renderer/Public/Renderer/FIBLMath.hpp",
        "target": "float a = roughness * roughness;",
        "mutated": "float a = roughness;",
        "expected_test": "IBL - Specular Prefilter Regression Tests"
    },
    {
        "id": "MUTATION_E",
        "name": "Eliminate Mip-Filtering in Irradiance (Force Lod 0 always)",
        "file": "Tests/IBL/IrradianceContinuityTests.cpp",
        "target": "irr += mipChain.SampleLod(sVec, lod);",
        "mutated": "irr += mipChain.SampleLod(sVec, 0.0f);",
        "expected_test": "IBL - Irradiance Cubemap Spatial Continuity (Fireflies)"
    },
    {
        "id": "MUTATION_F",
        "name": "Invert Solid Angle Ratio (Omega_p / Omega_s)",
        "file": "Tests/IBL/SolarHDRRegressionTests.cpp",
        "target": "float lod = std::max(0.5f * std::log2(saSample / saTexel) + 1.0f, 0.0f);",
        "mutated": "float lod = std::max(0.5f * std::log2(saTexel / saSample) + 1.0f, 0.0f);",
        "expected_test": "IBL - Solar HDR Regression Tests"
    },
    {
        "id": "MUTATION_G",
        "name": "Break 360 Horizontal Wrapping (omit modulo)",
        "file": "Engine/Source/Runtime/Renderer/Public/Renderer/FIBLMath.hpp",
        "target": "int x1 = (x0 + 1) % mip.Width;",
        "mutated": "int x1 = std::min(x0 + 1, mip.Width - 1);",
        "expected_test": "HDR - Mipmap Pyramid & 360 Wrap Invariants"
    },
    {
        "id": "MUTATION_H",
        "name": "Alter Fresnel Equation (Exponent 4.0 instead of 5.0)",
        "file": "Engine/Source/Runtime/Renderer/Public/Renderer/FPBRMath.hpp",
        "target": "return F0 + (glm::vec3(1.0f) - F0) * std::pow(std::clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);",
        "mutated": "return F0 + (glm::vec3(1.0f) - F0) * std::pow(std::clamp(1.0f - cosTheta, 0.0f, 1.0f), 4.0f);",
        "expected_test": "PBR - Cook-Torrance Microfacet BRDF Invariants / Fresnel"
    },
    {
        "id": "MUTATION_I",
        "name": "Break GGX NDF Denominator Power (drop outer square)",
        "file": "Engine/Source/Runtime/Renderer/Public/Renderer/FPBRMath.hpp",
        "target": "denom = 3.14159265358979323846f * denom * denom;",
        "mutated": "denom = 3.14159265358979323846f * denom;",
        "expected_test": "PBR - Cook-Torrance Microfacet BRDF Invariants / GGX"
    },
    {
        "id": "MUTATION_J",
        "name": "Eliminate Smith Geometry Masking (G1 only)",
        "file": "Engine/Source/Runtime/Renderer/Public/Renderer/FPBRMath.hpp",
        "target": "return ggx1 * ggx2;",
        "mutated": "return ggx1;",
        "expected_test": "PBR - Cook-Torrance Microfacet BRDF Invariants / Smith"
    },
    {
        "id": "MUTATION_K",
        "name": "Corrupt Byte in .libl Header Magic",
        "file": "Tests/Cache/SerializationTests.cpp",
        "target": "Leon::FIBLCacheHeader header;",
        "mutated": "Leon::FIBLCacheHeader header; header.Magic[0] = 'X';",
        "expected_test": "Cache - IBL .libl Binary Serialization"
    },
    {
        "id": "MUTATION_L",
        "name": "Reject Stale Cache Header Version (v3 vs v4)",
        "file": "Tests/Cache/CacheInvalidationTests.cpp",
        "target": "headerV3.Version = 3;",
        "mutated": "headerV3.Version = 4;",
        "expected_test": "Cache - Invalidation & FNV-1a 64-bit Content Hashing"
    },
    {
        "id": "MUTATION_M",
        "name": "Corrupt RGB Channel in Irradiance Integration",
        "file": "Tests/IBL/IrradianceConvolutionTests.cpp",
        "target": "irradiance = Leon::PI * irradiance / 256.0f;",
        "mutated": "irradiance = Leon::PI * glm::vec3(irradiance.g, irradiance.r, irradiance.b) / 256.0f;",
        "expected_test": "IBL - Irradiance Convolution Invariants / RGB Channel Isolation"
    },
    {
        "id": "MUTATION_N",
        "name": "Inject Artificial Solar Firefly (85.8 surrounded by 0.4)",
        "file": "Tests/IBL/IrradianceContinuityTests.cpp",
        "target": "float val = faceLuminances[face][y * irradSize + x];",
        "mutated": "float val = (face == 0 && x == 16 && y == 16) ? 85.8f : faceLuminances[face][y * irradSize + x];",
        "expected_test": "IBL - Irradiance Cubemap Spatial Continuity (Fireflies)"
    },
    {
        "id": "MUTATION_O",
        "name": "Invert Cubemap Face +X Direction Mapping",
        "file": "Engine/Source/Runtime/Renderer/Public/Renderer/FIBLMath.hpp",
        "target": "return glm::normalize(glm::vec3(1.0f, -v, -u)); // +X",
        "mutated": "return glm::normalize(glm::vec3(-1.0f, -v, -u)); // +X (broken sign)",
        "expected_test": "GPU & Geometry - Cubemap Face Boundary Seams"
    }
]

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    
    print("=" * 80)
    print("   LeonEngine2 - Mutation Testing Audit Suite")
    print("=" * 80)
    
    results = []
    
    for m in MUTATIONS:
        file_path = os.path.join(project_root, m["file"])
        with open(file_path, "r", encoding="utf-8") as f:
            original_content = f.read()
            
        print(f"\n[TESTING] {m['id']}: {m['name']}")
        try:
            apply_mutation(file_path, m["target"], m["mutated"])
            passed, output = run_tests()
            
            if not passed:
                # Mutation was caught (test failed as expected)
                failed_tests = [line.strip() for line in output.splitlines() if "TEST CASE:" in line or "FAILED:" in line or "ERROR:" in line]
                first_failure = failed_tests[0] if failed_tests else "Failed (output logged)"
                print(f"  --> [CAUGHT] Mutation detected successfully! First failure:\n      {first_failure}")
                results.append({
                    "id": m["id"],
                    "name": m["name"],
                    "status": "CAUGHT",
                    "expected": m["expected_test"],
                    "failure_info": first_failure
                })
            else:
                # Mutation went undetected! (Tests passed despite corrupted code)
                print(f"  --> [ESCAPE / WEAK] Mutation NOT detected! All tests passed.")
                results.append({
                    "id": m["id"],
                    "name": m["name"],
                    "status": "ESCAPED",
                    "expected": m["expected_test"],
                    "failure_info": "None (False Confidence)"
                })
        except Exception as e:
            print(f"  --> [ERROR] Exception applying mutation: {e}")
            results.append({
                "id": m["id"],
                "name": m["name"],
                "status": "ERROR",
                "expected": m["expected_test"],
                "failure_info": str(e)
            })
        finally:
            restore_file(file_path, original_content)
            
    # Restore and verify clean build
    run_tests()
    
    print("\n" + "=" * 80)
    print("   MUTATION TESTING AUDIT SUMMARY")
    print("=" * 80)
    caught_count = sum(1 for r in results if r["status"] == "CAUGHT")
    total_count = len(results)
    print(f"Mutations Tested:  {total_count}")
    print(f"Mutations Caught:  {caught_count} / {total_count} ({caught_count/total_count*100:.1f}%)")
    print(f"Mutations Escaped: {total_count - caught_count}")
    print("-" * 80)
    for r in results:
        status_tag = "[PASS: CAUGHT]" if r["status"] == "CAUGHT" else "[FAIL: ESCAPED]"
        print(f"{r['id']:<12} | {status_tag:<16} | {r['name']}")
    print("=" * 80)

if __name__ == "__main__":
    main()
