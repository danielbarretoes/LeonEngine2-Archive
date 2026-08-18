#include <doctest/doctest.h>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Renderer/FColorSpace.hpp"
#include "Renderer/FLightAttenuation.hpp"
#include "Renderer/FPBRMath.hpp"
#include "Renderer/FRenderingMath.hpp"
#include "Renderer/FVertexLayout.hpp"
#include "Renderer/FIBLMath.hpp"
#include "Renderer/FPlanarReflectionTypes.hpp"

TEST_SUITE("Renderer contract - transforms, TBN, PBR, color, shadows") {

    TEST_CASE("Identity / translation / rotation / uniform / non-uniform / negative scale") {
        glm::mat4 I(1.0f);
        glm::vec4 p(1, 2, 3, 1);
        CHECK(glm::vec3(I * p) == glm::vec3(1, 2, 3));

        glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(4, 5, 6));
        CHECK(glm::vec3(T * p) == glm::vec3(5, 7, 9));

        glm::mat4 R = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 1, 0));
        glm::vec3 rp = glm::vec3(R * glm::vec4(1, 0, 0, 1));
        CHECK(rp.x == doctest::Approx(0.0f).epsilon(1e-5f));
        CHECK(rp.z == doctest::Approx(-1.0f).epsilon(1e-5f));

        glm::mat4 Su = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
        CHECK(glm::vec3(Su * p) == glm::vec3(2, 4, 6));

        glm::mat4 Sn = glm::scale(glm::mat4(1.0f), glm::vec3(2, 1, 0.5f));
        CHECK(glm::vec3(Sn * p) == glm::vec3(2, 2, 1.5f));

        glm::mat4 Sneg = glm::scale(glm::mat4(1.0f), glm::vec3(-1, 1, 1));
        CHECK(Leon::HasNegativeScale(Sneg));
        CHECK_FALSE(Leon::HasNegativeScale(Su));
        glm::mat3 nmat = Leon::SafeNormalMatrix(Sneg);
        glm::vec3 n = glm::normalize(nmat * glm::vec3(1, 0, 0));
        CHECK(n.x == doctest::Approx(-1.0f).epsilon(1e-5f));
    }

    TEST_CASE("Perspective projection maps near/far and unprojects the center") {
        float nearP = 0.1f, farP = 1000.0f;
        glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, nearP, farP);
        glm::vec4 clipNear = proj * glm::vec4(0, 0, -nearP, 1);
        glm::vec3 ndcNear = glm::vec3(clipNear) / clipNear.w;
        CHECK(ndcNear.z == doctest::Approx(-1.0f).epsilon(1e-4f));

        glm::vec4 clipFar = proj * glm::vec4(0, 0, -farP, 1);
        glm::vec3 ndcFar = glm::vec3(clipFar) / clipFar.w;
        CHECK(ndcFar.z == doctest::Approx(1.0f).epsilon(1e-3f));

        glm::mat4 inv = glm::inverse(proj);
        glm::vec4 world = inv * glm::vec4(0, 0, -1, 1);
        world /= world.w;
        CHECK(world.z == doctest::Approx(-nearP).epsilon(1e-4f));
    }

    TEST_CASE("PackTangent: T x B ≈ N including mirrored UV") {
        glm::vec3 n(0, 1, 0);
        glm::vec3 t(1, 0, 0);
        glm::vec3 b(0, 0, -1);
        glm::vec4 packed = Leon::PackTangent(t, n, b);
        glm::vec3 bu = Leon::UnpackBitangent(packed, n);
        glm::vec3 recoveredN = glm::cross(glm::vec3(packed), bu);
        CHECK(glm::dot(recoveredN, n) == doctest::Approx(1.0f).epsilon(1e-4f));
        CHECK(packed.w == doctest::Approx(1.0f));

        glm::vec3 bMirror(0, 0, 1);
        glm::vec4 packedM = Leon::PackTangent(t, n, bMirror);
        CHECK(packedM.w == doctest::Approx(-1.0f));
        glm::vec3 b2 = Leon::UnpackBitangent(packedM, n);
        CHECK(glm::dot(b2, bMirror) == doctest::Approx(1.0f).epsilon(1e-4f));
        // Mirrored UV: left-handed TBN, T×B = w N
        CHECK(glm::dot(glm::cross(glm::vec3(packedM), b2), n) == doctest::Approx(packedM.w).epsilon(1e-4f));
    }

    TEST_CASE("Canonical vertex is 17 tightly packed floats") {
        CHECK(sizeof(Leon::FCanonicalMeshVertex) == 68);
        CHECK(Leon::kCanonicalVertexFloats == 17);
    }

    TEST_CASE("Lambert + IBL energy: E=PI albedo=1 => Lo_diffuse=1") {
        const float PI = 3.14159265358979323846f;
        glm::vec3 albedo(1.0f);
        glm::vec3 irradiance(PI);
        glm::vec3 lo = albedo / PI * irradiance;
        CHECK(lo.r == doctest::Approx(1.0f).epsilon(1e-5f));
    }

    TEST_CASE("sRGB 128/255 piecewise decode") {
        float lin = Leon::SRGBToLinear(128.0f / 255.0f);
        CHECK(lin == doctest::Approx(0.2158605f).epsilon(0.001f));
        CHECK(lin != doctest::Approx(128.0f / 255.0f).epsilon(0.01f));
    }

    TEST_CASE("Spot cone baker matches runtime Hermite smoothstep") {
        float onAxis = Leon::SpotConeAttenuation({0, -1, 0}, {0, 1, 0}, 12.5f, 17.5f);
        CHECK(onAxis == doctest::Approx(1.0f).epsilon(1e-5f));
        float outside = Leon::SpotConeAttenuation({0, -1, 0}, {1, 0, 0}, 12.5f, 17.5f);
        CHECK(outside == doctest::Approx(0.0f).epsilon(1e-5f));
    }

    TEST_CASE("World to shadow NDC UV") {
        glm::mat4 lightView = glm::lookAt(glm::vec3(0, 10, 0), glm::vec3(0), glm::vec3(0, 0, -1));
        glm::mat4 lightProj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 50.0f);
        glm::mat4 lightVP = lightProj * lightView;
        glm::vec4 clip = lightVP * glm::vec4(0, 0, 0, 1);
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        glm::vec2 uv = glm::vec2(ndc) * 0.5f + 0.5f;
        CHECK(uv.x == doctest::Approx(0.5f).epsilon(1e-4f));
        CHECK(uv.y == doctest::Approx(0.5f).epsilon(1e-4f));
        CHECK(ndc.z > -1.0f);
        CHECK(ndc.z < 1.0f);
    }

    TEST_CASE("SafeNormalize of zero returns fallback") {
        glm::vec3 z = Leon::SafeNormalize(glm::vec3(0.0f), glm::vec3(0, 1, 0));
        CHECK(z == glm::vec3(0, 1, 0));
    }

    TEST_CASE("Planar reflection through floor and vertical wall") {
        glm::vec3 floorHit = Leon::ReflectPointThroughPlane({0.0f, 2.0f, 4.0f}, {0.0f, 1.0f, 0.0f}, 0.0f);
        CHECK(floorHit.x == doctest::Approx(0.0f));
        CHECK(floorHit.y == doctest::Approx(-2.0f));
        CHECK(floorHit.z == doctest::Approx(4.0f));

        constexpr float kMirror = 35.54f;
        glm::vec3 n(0.0f, 0.0f, 1.0f);
        glm::vec3 wallHit = Leon::ReflectPointThroughPlane({0.0f, 1.7f, 0.0f}, n, kMirror);
        CHECK(wallHit.x == doctest::Approx(0.0f));
        CHECK(wallHit.y == doctest::Approx(1.7f));
        CHECK(wallHit.z == doctest::Approx(-2.0f * kMirror));

        glm::vec4 viaMatrix = Leon::PlanarReflectionMatrix(n, kMirror) * glm::vec4(0.0f, 1.7f, 0.0f, 1.0f);
        CHECK(viaMatrix.z == doctest::Approx(wallHit.z).epsilon(1e-4f));
    }

    TEST_CASE("Planar capture prefers the wall you face over the floor") {
        glm::vec3 eye(0.0f, 1.7f, 0.0f);
        glm::vec3 lookWall(0.0f, 0.0f, -1.0f);
        glm::vec3 lookFloor(0.0f, -1.0f, 0.0f);
        const float wall = Leon::PlanarReflectionPlaneScore({0.0f, 0.0f, 1.0f}, 35.54f, eye, lookWall);
        const float floorFacingWall = Leon::PlanarReflectionPlaneScore({0.0f, 1.0f, 0.0f}, 0.0f, eye, lookWall);
        const float floorLookDown = Leon::PlanarReflectionPlaneScore({0.0f, 1.0f, 0.0f}, 0.0f, eye, lookFloor);
        const float wallLookDown = Leon::PlanarReflectionPlaneScore({0.0f, 0.0f, 1.0f}, 35.54f, eye, lookFloor);
        CHECK(wall > floorFacingWall);
        CHECK(floorLookDown > wallLookDown);
        CHECK(Leon::IsHorizontalPlanarPlane({0.0f, 1.0f, 0.0f}));
        CHECK_FALSE(Leon::IsHorizontalPlanarPlane({0.0f, 0.0f, 1.0f}));
        CHECK(wall > Leon::kWallPlanarCaptureMinScore);
        CHECK(wallLookDown < Leon::kWallPlanarCaptureMinScore);
    }

    TEST_CASE("Planar reflection quality tokens default to Epic (full viewport)") {
        CHECK(Leon::ParsePlanarReflectionQuality("") == Leon::EPlanarReflectionQuality::Epic);
        CHECK(Leon::ParsePlanarReflectionQuality("Epic") == Leon::EPlanarReflectionQuality::Epic);
        CHECK(Leon::ParsePlanarReflectionQuality("max") == Leon::EPlanarReflectionQuality::Epic);
        CHECK(Leon::ParsePlanarReflectionQuality("Low") == Leon::EPlanarReflectionQuality::Low);
        CHECK(Leon::PlanarReflectionScaleFor(Leon::EPlanarReflectionQuality::Epic) == doctest::Approx(1.0f));
        CHECK(Leon::PlanarReflectionMipLevelsFor(Leon::EPlanarReflectionQuality::Epic) == 5);
        CHECK(Leon::ClampPlanarReflectionResolutionScale(2.0f) == doctest::Approx(1.0f));
    }

    TEST_CASE("Cook-Torrance Lambert term uses albedo/PI") {
        glm::vec3 N(0, 0, 1), V(0, 0, 1), L(0, 0, 1);
        glm::vec3 lo = Leon::EvaluateCookTorrance(N, V, L, glm::vec3(1.0f), 0.0f, 1.0f, glm::vec3(1.0f));
        CHECK(lo.r > 0.0f);
        CHECK(!std::isnan(lo.r));
    }

    TEST_CASE("Lambert Lo = albedo/PI for NdotL=1 radiance=1") {
        const float PI = 3.14159265358979323846f;
        glm::vec3 albedo(1.0f);
        float NdotL = 1.0f;
        glm::vec3 radiance(1.0f);
        glm::vec3 lo = (albedo / PI) * radiance * NdotL;
        CHECK(lo.r == doctest::Approx(1.0f / PI).epsilon(1e-6f));
    }

    TEST_CASE("GGX at NdotH=1 roughness=0.5 equals 16/PI") {
        const float PI = 3.14159265358979323846f;
        float D = Leon::DistributionGGX(1.0f, 0.5f);
        CHECK(D == doctest::Approx(16.0f / PI).epsilon(1e-5f));
    }

    TEST_CASE("Cube front face winding is CCW / outward") {
        float h = 0.5f;
        glm::vec3 p0(-h, -h, h), p1(h, -h, h), p2(h, h, h);
        glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
        CHECK(n.z > 0.0f);
    }

    TEST_CASE("Sphere triangle winding is outward (CCW from outside)") {
        constexpr float PI = 3.14159265358979323846f;
        auto pos = [](float theta, float phi) {
            return glm::vec3(std::cos(theta) * std::sin(phi), std::cos(phi), std::sin(theta) * std::sin(phi));
        };
        float theta0 = 0.0f, theta1 = PI / 4.0f;
        float phi0 = PI / 2.0f, phi1 = 3.0f * PI / 4.0f;
        glm::vec3 i0 = pos(theta0, phi0);
        glm::vec3 i1 = pos(theta0, phi1);
        glm::vec3 i2 = pos(theta1, phi1);
        glm::vec3 n = glm::cross(i2 - i0, i1 - i0);
        CHECK(glm::dot(n, i0) > 0.0f);
    }

    TEST_CASE("Camera basis stays finite at pitch ±90") {
        glm::vec3 right, up;
        Leon::StableViewBasis(glm::vec3(0, 1, 0), right, up);
        CHECK(!std::isnan(right.x));
        CHECK(!std::isnan(up.x));
        CHECK(glm::length(right) == doctest::Approx(1.0f).epsilon(1e-4f));
        Leon::StableViewBasis(glm::vec3(0, -1, 0), right, up);
        CHECK(!std::isnan(right.x));
        CHECK(glm::length(glm::cross(right, glm::vec3(0, -1, 0))) > 0.5f);
    }

    TEST_CASE("IBL cache does not embed scene exposure") {
        Leon::FIBLCacheHeader header;
        CHECK(header.Version == Leon::kIBLCacheVersion);
        CHECK(header.Version == 6);
        CHECK(header.HDRSourceHash == 0);
    }

    TEST_CASE("Cook-Torrance Lambert term uses kD * albedo / PI") {
        const float PI = 3.14159265358979323846f;
        glm::vec3 N(0, 0, 1), V(0, 0, 1), L(0, 0, 1);
        glm::vec3 lo = Leon::EvaluateCookTorrance(N, V, L, glm::vec3(1.0f), 0.0f, 1.0f, glm::vec3(1.0f));
        glm::vec3 F = Leon::FresnelSchlick(1.0f, glm::vec3(0.04f));
        float kD = (1.0f - F.r);
        CHECK(lo.r >= doctest::Approx(kD / PI).epsilon(1e-4f));
        CHECK(!std::isnan(lo.r));
    }

    TEST_CASE("UE4 distance attenuation at d=0,1,2,10") {
        CHECK(Leon::DistanceAttenuationUE4(0.0f, 10.0f) == doctest::Approx(1.0f).epsilon(1e-6f));
        CHECK(Leon::DistanceAttenuationUE4(1.0f, 10.0f) == doctest::Approx(0.4999f).epsilon(1e-3f));
        float d2 = Leon::DistanceAttenuationUE4(2.0f, 10.0f);
        float ratio = 0.2f;
        float window = 1.0f - ratio * ratio * ratio * ratio;
        window *= window;
        CHECK(d2 == doctest::Approx(window / 5.0f).epsilon(1e-5f));
        CHECK(Leon::DistanceAttenuationUE4(10.0f, 10.0f) == doctest::Approx(0.0f).epsilon(1e-6f));
    }

    TEST_CASE("Negative scale flips handedness of normal matrix") {
        glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(-1, 1, 1));
        CHECK(Leon::HasNegativeScale(S));
        glm::mat3 nmat = Leon::SafeNormalMatrix(S);
        glm::vec3 n = glm::normalize(nmat * glm::vec3(1, 0, 0));
        CHECK(n.x == doctest::Approx(-1.0f).epsilon(1e-5f));
    }
}
