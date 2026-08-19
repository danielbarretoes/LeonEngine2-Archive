#include "Renderer/FMeshPrimitives.hpp"
#include "Renderer/FVertexLayout.hpp"
#include "RHI/FBuffer.hpp"

#include <cmath>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <vector>

namespace Leon {

    namespace {

        std::unordered_map<std::string, TRef<FVertexArray>> GPrimitiveCache;

        void ClearPrimitiveCache() {
            GPrimitiveCache.clear();
        }

        TRef<FVertexArray> CachePrimitive(const std::string& InKey, TRef<FVertexArray> InVA) {
            if (InVA)
                GPrimitiveCache[InKey] = InVA;
            return InVA;
        }

        TRef<FVertexArray> FindCachedPrimitive(const std::string& InKey) {
            auto it = GPrimitiveCache.find(InKey);
            return it != GPrimitiveCache.end() ? it->second : nullptr;
        }

        TRef<FVertexArray> BuildCanonicalMesh(const std::vector<float>& InVertices,
                                              const std::vector<uint32_t>& InIndices) {
            TRef<FVertexArray> vertexArray = FVertexArray::Create();
            if (!vertexArray)
                return nullptr;

            TRef<FVertexBuffer> vertexBuffer =
                FVertexBuffer::Create(InVertices.data(), static_cast<uint32_t>(InVertices.size() * sizeof(float)));
            if (!vertexBuffer)
                return nullptr;
            vertexBuffer->SetLayout(MakeCanonicalMeshLayout());
            vertexArray->AddVertexBuffer(vertexBuffer);

            TRef<FIndexBuffer> indexBuffer =
                FIndexBuffer::Create(InIndices.data(), static_cast<uint32_t>(InIndices.size()));
            if (!indexBuffer)
                return nullptr;
            vertexArray->SetIndexBuffer(indexBuffer);
            return vertexArray;
        }

        void PushQuad(std::vector<float>& Out, const glm::vec3 InP[4], const glm::vec2 InUV[4], const glm::vec3& InN,
                      const glm::vec3& InT, const glm::vec3& InB, int InLightmapFace = -1) {
            for (int i = 0; i < 4; ++i) {
                glm::vec2 lm = InUV[i] * 0.96f + glm::vec2(0.02f);
                if (InLightmapFace >= 0)
                    lm = PackLightmapCell(InUV[i], InLightmapFace % 3, InLightmapFace / 3, 3, 2);
                AppendCanonicalVertex(Out, InP[i], InN, InUV[i], InT, InB, glm::vec3(1.0f), lm);
            }
        }

    } // namespace

    void FMeshPrimitives::ReleaseStaticCaches() {
        ClearPrimitiveCache();
    }

    TRef<FVertexArray> FMeshPrimitives::CreateCube(float InSize) {
        const std::string key = "cube:" + std::to_string(InSize);
        if (auto cached = FindCachedPrimitive(key))
            return cached;
        float h = InSize * 0.5f;
        std::vector<float> vertices;
        vertices.reserve(24 * kCanonicalVertexFloats);

        const glm::vec2 uv[4] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

        // Front (+Z): T×B = N
        {
            glm::vec3 p[4] = {{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}};
            PushQuad(vertices, p, uv, {0, 0, 1}, {1, 0, 0}, {0, 1, 0}, 0);
        }
        // Back (-Z)
        {
            glm::vec3 p[4] = {{h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}};
            PushQuad(vertices, p, uv, {0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, 1);
        }
        // Top (+Y)
        {
            glm::vec3 p[4] = {{-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}};
            PushQuad(vertices, p, uv, {0, 1, 0}, {1, 0, 0}, {0, 0, -1}, 2);
        }
        // Bottom (-Y)
        {
            glm::vec3 p[4] = {{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}};
            PushQuad(vertices, p, uv, {0, -1, 0}, {1, 0, 0}, {0, 0, 1}, 3);
        }
        // Left (-X)
        {
            glm::vec3 p[4] = {{-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}};
            PushQuad(vertices, p, uv, {-1, 0, 0}, {0, 0, 1}, {0, 1, 0}, 4);
        }
        // Right (+X)
        {
            glm::vec3 p[4] = {{h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}};
            PushQuad(vertices, p, uv, {1, 0, 0}, {0, 0, -1}, {0, 1, 0}, 5);
        }

        std::vector<uint32_t> indices = {0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,  8,  9,  10, 10, 11, 8,
                                         12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20};
        return CachePrimitive(key, BuildCanonicalMesh(vertices, indices));
    }

    TRef<FVertexArray> FMeshPrimitives::CreateQuad(float InWidth, float InHeight) {
        const std::string key = "quad:" + std::to_string(InWidth) + ":" + std::to_string(InHeight);
        if (auto cached = FindCachedPrimitive(key))
            return cached;
        float hx = InWidth * 0.5f;
        float hy = InHeight * 0.5f;
        std::vector<float> vertices;
        glm::vec3 p[4] = {{-hx, -hy, 0.0f}, {hx, -hy, 0.0f}, {hx, hy, 0.0f}, {-hx, hy, 0.0f}};
        glm::vec2 uv[4] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
        PushQuad(vertices, p, uv, {0, 0, 1}, {1, 0, 0}, {0, 1, 0});
        std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
        return CachePrimitive(key, BuildCanonicalMesh(vertices, indices));
    }

    TRef<FVertexArray> FMeshPrimitives::CreateSphere(float InRadius, unsigned int InSegments, unsigned int InRings) {
        const std::string key =
            "sphere:" + std::to_string(InRadius) + ":" + std::to_string(InSegments) + ":" + std::to_string(InRings);
        if (auto cached = FindCachedPrimitive(key))
            return cached;
        std::vector<float> vertices;
        std::vector<uint32_t> indices;

        constexpr float PI = 3.14159265358979323846f;

        for (unsigned int y = 0; y <= InRings; ++y) {
            float v = static_cast<float>(y) / static_cast<float>(InRings);
            float phi = v * PI;

            for (unsigned int x = 0; x <= InSegments; ++x) {
                float u = static_cast<float>(x) / static_cast<float>(InSegments);
                float theta = u * (PI * 2.0f);

                float nx = std::cos(theta) * std::sin(phi);
                float ny = std::cos(phi);
                float nz = std::sin(theta) * std::sin(phi);

                glm::vec3 normal(nx, ny, nz);
                glm::vec3 tangent(-std::sin(theta), 0.0f, std::cos(theta));
                if (glm::dot(tangent, tangent) < 1e-8f)
                    tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                else
                    tangent = glm::normalize(tangent);
                glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));

                AppendCanonicalVertex(vertices, glm::vec3(InRadius * nx, InRadius * ny, InRadius * nz), normal,
                                      glm::vec2(u, v), tangent, bitangent, glm::vec3(1.0f),
                                      glm::vec2(u * 0.96f + 0.02f, v * 0.96f + 0.02f));
            }
        }

        // CCW when viewed from outside (outward normals)
        for (unsigned int y = 0; y < InRings; ++y) {
            for (unsigned int x = 0; x < InSegments; ++x) {
                uint32_t i0 = y * (InSegments + 1) + x;
                uint32_t i1 = (y + 1) * (InSegments + 1) + x;
                uint32_t i2 = (y + 1) * (InSegments + 1) + (x + 1);
                uint32_t i3 = y * (InSegments + 1) + (x + 1);

                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i1);

                indices.push_back(i0);
                indices.push_back(i3);
                indices.push_back(i2);
            }
        }

        return CachePrimitive(key, BuildCanonicalMesh(vertices, indices));
    }

    TRef<FVertexArray> FMeshPrimitives::CreatePlane(float InWidth, float InDepth, unsigned int InSubdivisionsX,
                                                    unsigned int InSubdivisionsZ) {
        const std::string key = "plane:n+y:" + std::to_string(InWidth) + ":" + std::to_string(InDepth) + ":" +
                                std::to_string(InSubdivisionsX) + ":" + std::to_string(InSubdivisionsZ);
        if (auto cached = FindCachedPrimitive(key))
            return cached;
        std::vector<float> vertices;
        std::vector<uint32_t> indices;

        float hx = InWidth * 0.5f;
        float hz = InDepth * 0.5f;
        float dx = InWidth / static_cast<float>(InSubdivisionsX);
        float dz = InDepth / static_cast<float>(InSubdivisionsZ);

        glm::vec3 n(0.0f, 1.0f, 0.0f);
        glm::vec3 t(1.0f, 0.0f, 0.0f);
        glm::vec3 b(0.0f, 0.0f, -1.0f); // T×B = N; UV0 V increases toward -Z so dP/dV = B

        for (unsigned int z = 0; z <= InSubdivisionsZ; ++z) {
            float posZ = -hz + z * dz;
            float zNorm = static_cast<float>(z) / static_cast<float>(InSubdivisionsZ);
            // Viewed from +Y: +X right, -Z screen-up. V=1 at the -Z edge so albedo is not upside-down.
            float v = (1.0f - zNorm) * (InDepth / 4.0f);

            for (unsigned int x = 0; x <= InSubdivisionsX; ++x) {
                float posX = -hx + x * dx;
                float u = (static_cast<float>(x) / static_cast<float>(InSubdivisionsX)) * (InWidth / 4.0f);
                float u1 = (static_cast<float>(x) / static_cast<float>(InSubdivisionsX)) * 0.96f + 0.02f;
                // UV1 keeps V along +Z so existing lightmap atlases stay aligned.
                float v1 = zNorm * 0.96f + 0.02f;
                AppendCanonicalVertex(vertices, glm::vec3(posX, 0.0f, posZ), n, glm::vec2(u, v), t, b, glm::vec3(1.0f),
                                      glm::vec2(u1, v1));
            }
        }

        // Front face +Y: i0→i1 is +Z, i1→i2 is +X, so +Z × +X = +Y (CCW from above).
        for (unsigned int z = 0; z < InSubdivisionsZ; ++z) {
            for (unsigned int x = 0; x < InSubdivisionsX; ++x) {
                uint32_t i0 = z * (InSubdivisionsX + 1) + x;
                uint32_t i1 = (z + 1) * (InSubdivisionsX + 1) + x;
                uint32_t i2 = (z + 1) * (InSubdivisionsX + 1) + (x + 1);
                uint32_t i3 = z * (InSubdivisionsX + 1) + (x + 1);

                indices.push_back(i0);
                indices.push_back(i1);
                indices.push_back(i2);

                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i3);
            }
        }

        return CachePrimitive(key, BuildCanonicalMesh(vertices, indices));
    }

    TRef<FVertexArray> FMeshPrimitives::CreateCylinder(float InBottomRadius, float InTopRadius, float InHeight,
                                                       unsigned int InSegments, bool InbCaps) {
        const std::string key = "cyl:" + std::to_string(InBottomRadius) + ":" + std::to_string(InTopRadius) + ":" +
                                std::to_string(InHeight) + ":" + std::to_string(InSegments) + ":" +
                                (InbCaps ? "1" : "0");
        if (auto cached = FindCachedPrimitive(key))
            return cached;
        std::vector<float> vertices;
        std::vector<uint32_t> indices;

        constexpr float PI = 3.14159265358979323846f;
        float h = InHeight * 0.5f;

        float dr = InBottomRadius - InTopRadius;
        float sideLen = std::sqrt(dr * dr + InHeight * InHeight);
        float ny = (sideLen > 0.0001f) ? (dr / sideLen) : 0.0f;
        float nr = (sideLen > 0.0001f) ? (InHeight / sideLen) : 1.0f;

        uint32_t sideBaseVertex = 0;
        for (unsigned int x = 0; x <= InSegments; ++x) {
            float u = static_cast<float>(x) / static_cast<float>(InSegments);
            float theta = u * (PI * 2.0f);
            float cosTheta = std::cos(theta);
            float sinTheta = std::sin(theta);

            glm::vec3 normal(nr * cosTheta, ny, nr * sinTheta);
            glm::vec3 tangent(-sinTheta, 0.0f, cosTheta);
            glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));

            AppendCanonicalVertex(vertices, glm::vec3(InBottomRadius * cosTheta, -h, InBottomRadius * sinTheta), normal,
                                  glm::vec2(u, 0.0f), tangent, bitangent, glm::vec3(1.0f),
                                  PackLightmapCell(glm::vec2(u, 0.0f), 0, 0, 1, 2));
            AppendCanonicalVertex(vertices, glm::vec3(InTopRadius * cosTheta, h, InTopRadius * sinTheta), normal,
                                  glm::vec2(u, 1.0f), tangent, bitangent, glm::vec3(1.0f),
                                  PackLightmapCell(glm::vec2(u, 1.0f), 0, 0, 1, 2));
        }

        for (unsigned int x = 0; x < InSegments; ++x) {
            uint32_t b0 = sideBaseVertex + x * 2;
            uint32_t t0 = b0 + 1;
            uint32_t b1 = sideBaseVertex + (x + 1) * 2;
            uint32_t t1 = b1 + 1;
            indices.push_back(b0);
            indices.push_back(t0);
            indices.push_back(t1);
            indices.push_back(b0);
            indices.push_back(t1);
            indices.push_back(b1);
        }

        if (InbCaps && InTopRadius > 0.0001f) {
            uint32_t topCenterIndex = static_cast<uint32_t>(vertices.size() / kCanonicalVertexFloats);
            glm::vec3 n(0.0f, 1.0f, 0.0f);
            glm::vec3 t(1.0f, 0.0f, 0.0f);
            glm::vec3 b(0.0f, 0.0f, -1.0f);
            AppendCanonicalVertex(vertices, glm::vec3(0.0f, h, 0.0f), n, glm::vec2(0.5f, 0.5f), t, b, glm::vec3(1.0f),
                                  PackLightmapCell(glm::vec2(0.5f, 0.5f), 0, 1, 2, 2));

            uint32_t ringStart = static_cast<uint32_t>(vertices.size() / kCanonicalVertexFloats);
            for (unsigned int x = 0; x <= InSegments; ++x) {
                float u = static_cast<float>(x) / static_cast<float>(InSegments);
                float theta = u * (PI * 2.0f);
                float cosTheta = std::cos(theta);
                float sinTheta = std::sin(theta);
                AppendCanonicalVertex(
                    vertices, glm::vec3(InTopRadius * cosTheta, h, InTopRadius * sinTheta), n,
                    glm::vec2(0.5f + 0.5f * cosTheta, 0.5f + 0.5f * sinTheta), t, b, glm::vec3(1.0f),
                    PackLightmapCell(glm::vec2(0.5f + 0.5f * cosTheta, 0.5f + 0.5f * sinTheta), 0, 1, 2, 2));
            }

            for (unsigned int x = 0; x < InSegments; ++x) {
                indices.push_back(topCenterIndex);
                indices.push_back(ringStart + x + 1);
                indices.push_back(ringStart + x);
            }
        }

        if (InbCaps && InBottomRadius > 0.0001f) {
            uint32_t botCenterIndex = static_cast<uint32_t>(vertices.size() / kCanonicalVertexFloats);
            glm::vec3 n(0.0f, -1.0f, 0.0f);
            glm::vec3 t(1.0f, 0.0f, 0.0f);
            glm::vec3 b(0.0f, 0.0f, 1.0f);
            AppendCanonicalVertex(vertices, glm::vec3(0.0f, -h, 0.0f), n, glm::vec2(0.5f, 0.5f), t, b, glm::vec3(1.0f),
                                  PackLightmapCell(glm::vec2(0.5f, 0.5f), 1, 1, 2, 2));

            uint32_t ringStart = static_cast<uint32_t>(vertices.size() / kCanonicalVertexFloats);
            for (unsigned int x = 0; x <= InSegments; ++x) {
                float u = static_cast<float>(x) / static_cast<float>(InSegments);
                float theta = u * (PI * 2.0f);
                float cosTheta = std::cos(theta);
                float sinTheta = std::sin(theta);
                AppendCanonicalVertex(
                    vertices, glm::vec3(InBottomRadius * cosTheta, -h, InBottomRadius * sinTheta), n,
                    glm::vec2(0.5f + 0.5f * cosTheta, 0.5f - 0.5f * sinTheta), t, b, glm::vec3(1.0f),
                    PackLightmapCell(glm::vec2(0.5f + 0.5f * cosTheta, 0.5f - 0.5f * sinTheta), 1, 1, 2, 2));
            }

            for (unsigned int x = 0; x < InSegments; ++x) {
                indices.push_back(botCenterIndex);
                indices.push_back(ringStart + x);
                indices.push_back(ringStart + x + 1);
            }
        }

        return CachePrimitive(key, BuildCanonicalMesh(vertices, indices));
    }

    TRef<FVertexArray> FMeshPrimitives::CreateRamp(float InWidth, float InHeight, float InDepth) {
        const std::string key =
            "ramp:" + std::to_string(InWidth) + ":" + std::to_string(InHeight) + ":" + std::to_string(InDepth);
        if (auto cached = FindCachedPrimitive(key))
            return cached;
        float w = InWidth * 0.5f;
        float h = InHeight * 0.5f;
        float d = InDepth * 0.5f;

        float slopeLen = std::sqrt(4.0f * h * h + 4.0f * d * d);
        float ny = (2.0f * d) / slopeLen;
        float nz = (2.0f * h) / slopeLen;
        float by = (2.0f * h) / slopeLen;
        float bz = (-2.0f * d) / slopeLen;

        std::vector<float> vertices;
        auto quad = [&](const glm::vec3 p[4], const glm::vec3& n, const glm::vec3& t, const glm::vec3& b) {
            const glm::vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            PushQuad(vertices, p, uv, n, t, b);
        };

        {
            glm::vec3 p[4] = {{-w, -h, -d}, {w, -h, -d}, {w, -h, d}, {-w, -h, d}};
            quad(p, {0, -1, 0}, {1, 0, 0}, {0, 0, 1});
        }
        {
            glm::vec3 p[4] = {{w, -h, -d}, {-w, -h, -d}, {-w, h, -d}, {w, h, -d}};
            quad(p, {0, 0, -1}, {-1, 0, 0}, {0, 1, 0});
        }
        {
            glm::vec3 p[4] = {{-w, -h, d}, {w, -h, d}, {w, h, -d}, {-w, h, -d}};
            quad(p, {0, ny, nz}, {1, 0, 0}, {0, by, bz});
        }

        AppendCanonicalVertex(vertices, {-w, -h, -d}, {-1, 0, 0}, {0, 0}, {0, 0, 1}, {0, 1, 0});
        AppendCanonicalVertex(vertices, {-w, -h, d}, {-1, 0, 0}, {1, 0}, {0, 0, 1}, {0, 1, 0});
        AppendCanonicalVertex(vertices, {-w, h, -d}, {-1, 0, 0}, {0, 1}, {0, 0, 1}, {0, 1, 0});

        AppendCanonicalVertex(vertices, {w, -h, d}, {1, 0, 0}, {0, 0}, {0, 0, -1}, {0, 1, 0});
        AppendCanonicalVertex(vertices, {w, -h, -d}, {1, 0, 0}, {1, 0}, {0, 0, -1}, {0, 1, 0});
        AppendCanonicalVertex(vertices, {w, h, -d}, {1, 0, 0}, {1, 1}, {0, 0, -1}, {0, 1, 0});

        std::vector<uint32_t> indices = {0, 1, 2,  2,  3,  0, 4,  5,  6,  6,  7,  4,
                                         8, 9, 10, 10, 11, 8, 12, 13, 14, 15, 16, 17};
        return CachePrimitive(key, BuildCanonicalMesh(vertices, indices));
    }

    TRef<FVertexArray> FMeshPrimitives::CreatePyramid(float InWidth, float InHeight, float InDepth) {
        const std::string key =
            "pyr:" + std::to_string(InWidth) + ":" + std::to_string(InHeight) + ":" + std::to_string(InDepth);
        if (auto cached = FindCachedPrimitive(key))
            return cached;
        float w = InWidth * 0.5f;
        float h = InHeight * 0.5f;
        float d = InDepth * 0.5f;

        float zSlopeLen = std::sqrt(d * d + 4.0f * h * h);
        float fnY = d / zSlopeLen;
        float fnZ = (2.0f * h) / zSlopeLen;
        float fbY = (2.0f * h) / zSlopeLen;
        float fbZ = -d / zSlopeLen;

        float xSlopeLen = std::sqrt(w * w + 4.0f * h * h);
        float rnX = (2.0f * h) / xSlopeLen;
        float rnY = w / xSlopeLen;
        float rbX = -w / xSlopeLen;
        float rbY = (2.0f * h) / xSlopeLen;

        std::vector<float> vertices;
        {
            glm::vec3 p[4] = {{-w, -h, -d}, {w, -h, -d}, {w, -h, d}, {-w, -h, d}};
            glm::vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            PushQuad(vertices, p, uv, {0, -1, 0}, {1, 0, 0}, {0, 0, 1});
        }

        auto tri = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& n,
                       const glm::vec3& t, const glm::vec3& bit) {
            AppendCanonicalVertex(vertices, a, n, {0, 0}, t, bit);
            AppendCanonicalVertex(vertices, b, n, {1, 0}, t, bit);
            AppendCanonicalVertex(vertices, c, n, {0.5f, 1}, t, bit);
        };

        tri({-w, -h, d}, {w, -h, d}, {0, h, 0}, {0, fnY, fnZ}, {1, 0, 0}, {0, fbY, fbZ});
        tri({w, -h, d}, {w, -h, -d}, {0, h, 0}, {rnX, rnY, 0}, {0, 0, -1}, {rbX, rbY, 0});
        tri({w, -h, -d}, {-w, -h, -d}, {0, h, 0}, {0, fnY, -fnZ}, {-1, 0, 0}, {0, fbY, -fbZ});
        tri({-w, -h, -d}, {-w, -h, d}, {0, h, 0}, {-rnX, rnY, 0}, {0, 0, 1}, {-rbX, rbY, 0});

        std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        return CachePrimitive(key, BuildCanonicalMesh(vertices, indices));
    }

} // namespace Leon
