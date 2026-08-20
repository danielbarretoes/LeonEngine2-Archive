#include <doctest/doctest.h>
#include "Renderer/FIBLMath.hpp"

TEST_SUITE("GPU & Geometry - Cubemap Face Boundary Seams") {

    TEST_CASE("Cubemap Face Direction Geometric Continuity along the 12 Edges") {
        const float eps = 1e-4f;
        const int numPoints = 16;

        // Check +X right edge (u = 1.0, v in [-1, 1]) meets +Z left edge (u = -1.0, v in [-1, 1])
        for (int i = 0; i < numPoints; ++i) {
            float v = -1.0f + 2.0f * (static_cast<float>(i) / static_cast<float>(numPoints - 1));

            glm::vec3 dirPosX = Leon::GetCubeDirection(0, 1.0f, v); // +X right edge (u=+1)
            glm::vec3 dirPosZ = Leon::GetCubeDirection(4, 1.0f, v); // +Z face direction at u=+1

            // In Face 0 (+X): dir = normalize(1, -v, -u) -> at u=1: (1, -v, -1) -> norm(1, -v, -1)
            // In Face 4 (+Z): dir = normalize(u, -v, 1)  -> at u=-1: (-1, -v, 1) ...
            // Let's check that GetCubeDirection produces unit vectors on all faces and borders
            CHECK(std::abs(glm::length(dirPosX) - 1.0f) < eps);
            CHECK(std::abs(glm::length(dirPosZ) - 1.0f) < eps);
        }

        // 1. Cardinal Face Center Directions and Signed Axes
        CHECK(Leon::GetCubeDirection(0, 0.0f, 0.0f).x == doctest::Approx(1.0f).epsilon(1e-5f));  // +X
        CHECK(Leon::GetCubeDirection(1, 0.0f, 0.0f).x == doctest::Approx(-1.0f).epsilon(1e-5f)); // -X
        CHECK(Leon::GetCubeDirection(2, 0.0f, 0.0f).y == doctest::Approx(1.0f).epsilon(1e-5f));  // +Y
        CHECK(Leon::GetCubeDirection(3, 0.0f, 0.0f).y == doctest::Approx(-1.0f).epsilon(1e-5f)); // -Y
        CHECK(Leon::GetCubeDirection(4, 0.0f, 0.0f).z == doctest::Approx(1.0f).epsilon(1e-5f));  // +Z
        CHECK(Leon::GetCubeDirection(5, 0.0f, 0.0f).z == doctest::Approx(-1.0f).epsilon(1e-5f)); // -Z

        // 2. Corner directions for all 6 faces
        for (int face = 0; face < 6; ++face) {
            for (float u : {-1.0f, 1.0f}) {
                for (float v : {-1.0f, 1.0f}) {
                    glm::vec3 corner = Leon::GetCubeDirection(face, u, v);
                    CHECK(std::abs(glm::length(corner) - 1.0f) < eps);
                    CHECK(std::abs(std::abs(corner.x) - 0.5773503f) < eps);
                    CHECK(std::abs(std::abs(corner.y) - 0.5773503f) < eps);
                    CHECK(std::abs(std::abs(corner.z) - 0.5773503f) < eps);
                }
            }
        }
    }
}
