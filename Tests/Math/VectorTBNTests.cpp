#include <doctest/doctest.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include "Renderer/FIBLMath.hpp"

TEST_SUITE("Math - Vector & TBN Invariants") {

    TEST_CASE("TBN Basis Orthonormality across Cardinal and Oblique Normals") {
        std::vector<glm::vec3> testNormals = {glm::vec3(1.0f, 0.0f, 0.0f),
                                              glm::vec3(-1.0f, 0.0f, 0.0f),
                                              glm::vec3(0.0f, 1.0f, 0.0f),
                                              glm::vec3(0.0f, -1.0f, 0.0f),
                                              glm::vec3(0.0f, 0.0f, 1.0f),
                                              glm::vec3(0.0f, 0.0f, -1.0f),
                                              glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)),
                                              glm::normalize(glm::vec3(-0.707f, 0.5f, 0.5f)),
                                              glm::normalize(glm::vec3(0.123f, -0.456f, 0.789f))};

        const float eps = 1e-5f;

        for (const auto& N : testNormals) {
            REQUIRE(std::abs(glm::length(N) - 1.0f) < eps);

            // Construct Tangent and Bitangent identical to IBLGenerator
            glm::vec3 up = (std::abs(N.z) < 0.999f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
            glm::vec3 T = glm::normalize(glm::cross(up, N));
            glm::vec3 B = glm::normalize(glm::cross(N, T));

            // 1. Length invariants
            CHECK(std::abs(glm::length(T) - 1.0f) < eps);
            CHECK(std::abs(glm::length(B) - 1.0f) < eps);
            CHECK(std::abs(glm::length(N) - 1.0f) < eps);

            // 2. Orthogonality invariants (dot products must be 0)
            CHECK(std::abs(glm::dot(T, N)) < eps);
            CHECK(std::abs(glm::dot(B, N)) < eps);
            CHECK(std::abs(glm::dot(T, B)) < eps);

            // 3. Right-handed coordinate frame (T x B == N)
            glm::vec3 computedN = glm::cross(T, B);
            CHECK(glm::distance(computedN, N) < eps);

            // 4. Local-to-world transform isometry (length preservation)
            glm::vec3 localSample(0.3f, -0.4f, 0.8660254f);
            localSample = glm::normalize(localSample);
            glm::vec3 worldSample = T * localSample.x + B * localSample.y + N * localSample.z;
            CHECK(std::abs(glm::length(worldSample) - 1.0f) < eps);
        }
    }
}
