#include "Renderer/FMeshPrimitives.hpp"
#include "RHI/FBuffer.hpp"

#include <cmath>
#include <glm/glm.hpp>
#include <vector>

namespace Leon {

    TRef<FVertexArray> FMeshPrimitives::CreateCube(float InSize) {
        float h = InSize * 0.5f;

        // 24 vertices (4 per face x 6 faces) for crisp per-face normals, UVs, tangents & bitangents
        // Format: Pos (3), Normal (3), TexCoord (2), Tangent (3), Bitangent (3), Color (3) = 17 floats per vertex
        float vertices[] = {
            // Front Face (+Z) - Normal (0,0,1), Tangent (1,0,0), Bitangent (0,1,0)
            -h, -h, h, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, h, -h, h,
            0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, h, h, h, 0.0f, 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, -h, h, h, 0.0f, 0.0f, 1.0f, 0.0f,
            1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f,

            // Back Face (-Z) - Normal (0,0,-1), Tangent (-1,0,0), Bitangent (0,1,0)
            h, -h, -h, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, -h, -h, -h,
            0.0f, 0.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, -h, h, -h, 0.0f, 0.0f,
            -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, h, h, -h, 0.0f, 0.0f, -1.0f, 0.0f,
            1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f,

            // Top Face (+Y) - Normal (0,1,0), Tangent (1,0,0), Bitangent (0,0,-1)
            -h, h, h, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f, h, h, h,
            0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f, h, h, -h, 0.0f, 1.0f,
            0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f, -h, h, -h, 0.0f, 1.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f,

            // Bottom Face (-Y) - Normal (0,-1,0), Tangent (1,0,0), Bitangent (0,0,1)
            -h, -h, -h, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, h, -h, -h,
            0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, h, -h, h, 0.0f, -1.0f,
            0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, -h, -h, h, 0.0f, -1.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,

            // Left Face (-X) - Normal (-1,0,0), Tangent (0,0,1), Bitangent (0,1,0)
            -h, -h, -h, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, -h, -h, h,
            -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, -h, h, h, -1.0f, 0.0f,
            0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, -h, h, -h, -1.0f, 0.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f,

            // Right Face (+X) - Normal (1,0,0), Tangent (0,0,-1), Bitangent (0,1,0)
            h, -h, h, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, h, -h, -h,
            1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, h, h, -h, 1.0f, 0.0f,
            0.0f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, h, h, h, 1.0f, 0.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f};

        uint32_t indices[] = {
            0,  1,  2,  2,  3,  0,  // Front
            4,  5,  6,  6,  7,  4,  // Back
            8,  9,  10, 10, 11, 8,  // Top
            12, 13, 14, 14, 15, 12, // Bottom
            16, 17, 18, 18, 19, 16, // Left
            20, 21, 22, 22, 23, 20  // Right
        };

        TRef<FVertexArray> vertexArray = FVertexArray::Create();

        TRef<FVertexBuffer> vertexBuffer = FVertexBuffer::Create(vertices, sizeof(vertices));
        vertexBuffer->SetLayout({{EShaderDataType::Float3, "aPos"},
                                 {EShaderDataType::Float3, "aNormal"},
                                 {EShaderDataType::Float2, "aTexCoord"},
                                 {EShaderDataType::Float3, "aTangent"},
                                 {EShaderDataType::Float3, "aBitangent"},
                                 {EShaderDataType::Float3, "aColor"}});
        vertexArray->AddVertexBuffer(vertexBuffer);

        TRef<FIndexBuffer> indexBuffer = FIndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t));
        vertexArray->SetIndexBuffer(indexBuffer);

        return vertexArray;
    }

    TRef<FVertexArray> FMeshPrimitives::CreateQuad(float InWidth, float InHeight) {
        float hx = InWidth * 0.5f;
        float hy = InHeight * 0.5f;

        float vertices[] = {-hx,  -hy,  0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                            1.0f, 1.0f, 1.0f, hx,   -hy,  0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                            0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, hx,   hy,   0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
                            1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, -hx,  hy,   0.0f, 0.0f, 0.0f,
                            1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f};

        uint32_t indices[] = {0, 1, 2, 2, 3, 0};

        TRef<FVertexArray> vertexArray = FVertexArray::Create();

        TRef<FVertexBuffer> vertexBuffer = FVertexBuffer::Create(vertices, sizeof(vertices));
        vertexBuffer->SetLayout({{EShaderDataType::Float3, "aPos"},
                                 {EShaderDataType::Float3, "aNormal"},
                                 {EShaderDataType::Float2, "aTexCoord"},
                                 {EShaderDataType::Float3, "aTangent"},
                                 {EShaderDataType::Float3, "aBitangent"},
                                 {EShaderDataType::Float3, "aColor"}});
        vertexArray->AddVertexBuffer(vertexBuffer);

        TRef<FIndexBuffer> indexBuffer = FIndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t));
        vertexArray->SetIndexBuffer(indexBuffer);

        return vertexArray;
    }

    TRef<FVertexArray> FMeshPrimitives::CreateSphere(float InRadius, unsigned int InSegments, unsigned int InRings) {
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

                float px = InRadius * nx;
                float py = InRadius * ny;
                float pz = InRadius * nz;

                glm::vec3 normal(nx, ny, nz);
                glm::vec3 tangent(-std::sin(theta), 0.0f, std::cos(theta));
                if (glm::length(tangent) < 0.0001f) {
                    tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                } else {
                    tangent = glm::normalize(tangent);
                }
                glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));

                // Position (3)
                vertices.push_back(px);
                vertices.push_back(py);
                vertices.push_back(pz);

                // Normal (3)
                vertices.push_back(nx);
                vertices.push_back(ny);
                vertices.push_back(nz);

                // TexCoord (2)
                vertices.push_back(u);
                vertices.push_back(v);

                // Tangent (3)
                vertices.push_back(tangent.x);
                vertices.push_back(tangent.y);
                vertices.push_back(tangent.z);

                // Bitangent (3)
                vertices.push_back(bitangent.x);
                vertices.push_back(bitangent.y);
                vertices.push_back(bitangent.z);

                // Color (3)
                vertices.push_back(1.0f);
                vertices.push_back(1.0f);
                vertices.push_back(1.0f);
            }
        }

        for (unsigned int y = 0; y < InRings; ++y) {
            for (unsigned int x = 0; x < InSegments; ++x) {
                uint32_t i0 = y * (InSegments + 1) + x;
                uint32_t i1 = (y + 1) * (InSegments + 1) + x;
                uint32_t i2 = (y + 1) * (InSegments + 1) + (x + 1);
                uint32_t i3 = y * (InSegments + 1) + (x + 1);

                indices.push_back(i0);
                indices.push_back(i1);
                indices.push_back(i2);

                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i3);
            }
        }

        TRef<FVertexArray> vertexArray = FVertexArray::Create();

        TRef<FVertexBuffer> vertexBuffer =
            FVertexBuffer::Create(vertices.data(), static_cast<unsigned int>(vertices.size() * sizeof(float)));
        vertexBuffer->SetLayout({{EShaderDataType::Float3, "aPos"},
                                 {EShaderDataType::Float3, "aNormal"},
                                 {EShaderDataType::Float2, "aTexCoord"},
                                 {EShaderDataType::Float3, "aTangent"},
                                 {EShaderDataType::Float3, "aBitangent"},
                                 {EShaderDataType::Float3, "aColor"}});
        vertexArray->AddVertexBuffer(vertexBuffer);

        TRef<FIndexBuffer> indexBuffer =
            FIndexBuffer::Create(indices.data(), static_cast<unsigned int>(indices.size()));
        vertexArray->SetIndexBuffer(indexBuffer);

        return vertexArray;
    }

    TRef<FVertexArray> FMeshPrimitives::CreatePlane(float InWidth, float InDepth, unsigned int InSubdivisionsX,
                                                    unsigned int InSubdivisionsZ) {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;

        float hx = InWidth * 0.5f;
        float hz = InDepth * 0.5f;

        float dx = InWidth / static_cast<float>(InSubdivisionsX);
        float dz = InDepth / static_cast<float>(InSubdivisionsZ);

        for (unsigned int z = 0; z <= InSubdivisionsZ; ++z) {
            float posZ = -hz + z * dz;
            float v = (static_cast<float>(z) / static_cast<float>(InSubdivisionsZ)) * (InDepth / 4.0f);

            for (unsigned int x = 0; x <= InSubdivisionsX; ++x) {
                float posX = -hx + x * dx;
                float u = (static_cast<float>(x) / static_cast<float>(InSubdivisionsX)) * (InWidth / 4.0f);

                // Position (3)
                vertices.push_back(posX);
                vertices.push_back(0.0f);
                vertices.push_back(posZ);

                // Normal (pointing +Y) (3)
                vertices.push_back(0.0f);
                vertices.push_back(1.0f);
                vertices.push_back(0.0f);

                // TexCoord (2)
                vertices.push_back(u);
                vertices.push_back(v);

                // Tangent (pointing +X) (3)
                vertices.push_back(1.0f);
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);

                // Bitangent (pointing +Z) (3)
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);
                vertices.push_back(1.0f);

                // Color (3)
                vertices.push_back(1.0f);
                vertices.push_back(1.0f);
                vertices.push_back(1.0f);
            }
        }

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

        TRef<FVertexArray> vertexArray = FVertexArray::Create();

        TRef<FVertexBuffer> vertexBuffer =
            FVertexBuffer::Create(vertices.data(), static_cast<unsigned int>(vertices.size() * sizeof(float)));
        vertexBuffer->SetLayout({{EShaderDataType::Float3, "aPos"},
                                 {EShaderDataType::Float3, "aNormal"},
                                 {EShaderDataType::Float2, "aTexCoord"},
                                 {EShaderDataType::Float3, "aTangent"},
                                 {EShaderDataType::Float3, "aBitangent"},
                                 {EShaderDataType::Float3, "aColor"}});
        vertexArray->AddVertexBuffer(vertexBuffer);

        TRef<FIndexBuffer> indexBuffer =
            FIndexBuffer::Create(indices.data(), static_cast<unsigned int>(indices.size()));
        vertexArray->SetIndexBuffer(indexBuffer);

        return vertexArray;
    }

    TRef<FVertexArray> FMeshPrimitives::CreateCylinder(float InBottomRadius, float InTopRadius, float InHeight,
                                                       unsigned int InSegments, bool InbCaps) {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;

        constexpr float PI = 3.14159265358979323846f;
        float h = InHeight * 0.5f;

        // 1. Generate Side Surface Vertices
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

            float nx = nr * cosTheta;
            float nz = nr * sinTheta;

            glm::vec3 normal(nx, ny, nz);
            glm::vec3 tangent(-sinTheta, 0.0f, cosTheta);
            glm::vec3 bitangent = glm::normalize(glm::cross(tangent, normal));

            // Bottom Ring Vertex
            float bx = InBottomRadius * cosTheta;
            float bz = InBottomRadius * sinTheta;
            vertices.push_back(bx);
            vertices.push_back(-h);
            vertices.push_back(bz);
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);
            vertices.push_back(u);
            vertices.push_back(0.0f);
            vertices.push_back(tangent.x);
            vertices.push_back(tangent.y);
            vertices.push_back(tangent.z);
            vertices.push_back(bitangent.x);
            vertices.push_back(bitangent.y);
            vertices.push_back(bitangent.z);
            vertices.push_back(1.0f);
            vertices.push_back(1.0f);
            vertices.push_back(1.0f);

            // Top Ring Vertex
            float tx = InTopRadius * cosTheta;
            float tz = InTopRadius * sinTheta;
            vertices.push_back(tx);
            vertices.push_back(h);
            vertices.push_back(tz);
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);
            vertices.push_back(u);
            vertices.push_back(1.0f);
            vertices.push_back(tangent.x);
            vertices.push_back(tangent.y);
            vertices.push_back(tangent.z);
            vertices.push_back(bitangent.x);
            vertices.push_back(bitangent.y);
            vertices.push_back(bitangent.z);
            vertices.push_back(1.0f);
            vertices.push_back(1.0f);
            vertices.push_back(1.0f);
        }

        // Side Indices (CCW front-facing winding for OpenGL Backface Culling)
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

        // 2. Top Cap (if enabled and radius > 0)
        if (InbCaps && InTopRadius > 0.0001f) {
            uint32_t topCenterIndex = static_cast<uint32_t>(vertices.size() / 17);
            // Center vertex
            vertices.push_back(0.0f);
            vertices.push_back(h);
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);
            vertices.push_back(1.0f);
            vertices.push_back(0.0f);
            vertices.push_back(0.5f);
            vertices.push_back(0.5f);
            vertices.push_back(1.0f);
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);
            vertices.push_back(-1.0f);
            vertices.push_back(1.0f);
            vertices.push_back(1.0f);
            vertices.push_back(1.0f);

            uint32_t ringStart = static_cast<uint32_t>(vertices.size() / 17);
            for (unsigned int x = 0; x <= InSegments; ++x) {
                float u = static_cast<float>(x) / static_cast<float>(InSegments);
                float theta = u * (PI * 2.0f);
                float cosTheta = std::cos(theta);
                float sinTheta = std::sin(theta);

                vertices.push_back(InTopRadius * cosTheta);
                vertices.push_back(h);
                vertices.push_back(InTopRadius * sinTheta);
                vertices.push_back(0.0f);
                vertices.push_back(1.0f);
                vertices.push_back(0.0f);
                vertices.push_back(0.5f + 0.5f * cosTheta);
                vertices.push_back(0.5f + 0.5f * sinTheta);
                vertices.push_back(1.0f);
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);
                vertices.push_back(-1.0f);
                vertices.push_back(1.0f);
                vertices.push_back(1.0f);
                vertices.push_back(1.0f);
            }

            for (unsigned int x = 0; x < InSegments; ++x) {
                indices.push_back(topCenterIndex);
                indices.push_back(ringStart + x + 1);
                indices.push_back(ringStart + x);
            }
        }

        // 3. Bottom Cap (if enabled and radius > 0)
        if (InbCaps && InBottomRadius > 0.0001f) {
            uint32_t botCenterIndex = static_cast<uint32_t>(vertices.size() / 17);
            // Center vertex
            vertices.push_back(0.0f);
            vertices.push_back(-h);
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);
            vertices.push_back(-1.0f);
            vertices.push_back(0.0f);
            vertices.push_back(0.5f);
            vertices.push_back(0.5f);
            vertices.push_back(1.0f);
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);
            vertices.push_back(1.0f);
            vertices.push_back(1.0f);
            vertices.push_back(1.0f);
            vertices.push_back(1.0f);

            uint32_t ringStart = static_cast<uint32_t>(vertices.size() / 17);
            for (unsigned int x = 0; x <= InSegments; ++x) {
                float u = static_cast<float>(x) / static_cast<float>(InSegments);
                float theta = u * (PI * 2.0f);
                float cosTheta = std::cos(theta);
                float sinTheta = std::sin(theta);

                vertices.push_back(InBottomRadius * cosTheta);
                vertices.push_back(-h);
                vertices.push_back(InBottomRadius * sinTheta);
                vertices.push_back(0.0f);
                vertices.push_back(-1.0f);
                vertices.push_back(0.0f);
                vertices.push_back(0.5f + 0.5f * cosTheta);
                vertices.push_back(0.5f - 0.5f * sinTheta);
                vertices.push_back(1.0f);
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);
                vertices.push_back(1.0f);
                vertices.push_back(1.0f);
                vertices.push_back(1.0f);
                vertices.push_back(1.0f);
            }

            for (unsigned int x = 0; x < InSegments; ++x) {
                indices.push_back(botCenterIndex);
                indices.push_back(ringStart + x);
                indices.push_back(ringStart + x + 1);
            }
        }

        TRef<FVertexArray> vertexArray = FVertexArray::Create();

        TRef<FVertexBuffer> vertexBuffer =
            FVertexBuffer::Create(vertices.data(), static_cast<unsigned int>(vertices.size() * sizeof(float)));
        vertexBuffer->SetLayout({{EShaderDataType::Float3, "aPos"},
                                 {EShaderDataType::Float3, "aNormal"},
                                 {EShaderDataType::Float2, "aTexCoord"},
                                 {EShaderDataType::Float3, "aTangent"},
                                 {EShaderDataType::Float3, "aBitangent"},
                                 {EShaderDataType::Float3, "aColor"}});
        vertexArray->AddVertexBuffer(vertexBuffer);

        TRef<FIndexBuffer> indexBuffer =
            FIndexBuffer::Create(indices.data(), static_cast<unsigned int>(indices.size()));
        vertexArray->SetIndexBuffer(indexBuffer);

        return vertexArray;
    }

    TRef<FVertexArray> FMeshPrimitives::CreateRamp(float InWidth, float InHeight, float InDepth) {
        float w = InWidth * 0.5f;
        float h = InHeight * 0.5f;
        float d = InDepth * 0.5f;

        // Sloped face normal, tangent, bitangent
        // Ramp slopes from (z = +d, y = -h) up to (z = -d, y = +h)
        float slopeLen = std::sqrt(4.0f * h * h + 4.0f * d * d);
        float ny = (2.0f * d) / slopeLen;
        float nz = (2.0f * h) / slopeLen;
        float by = (2.0f * h) / slopeLen;
        float bz = (-2.0f * d) / slopeLen;

        // 18 vertices across 5 faces:
        // Format: Pos(3), Normal(3), UV(2), Tangent(3), Bitangent(3), Color(3) = 17 floats
        float vertices[] = {
            // 1. Bottom Face (-Y) - Normal (0, -1, 0), Tangent (1, 0, 0), Bitangent (0, 0, 1)
            -w, -h, -d, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, w, -h, -d,
            0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, w, -h, d, 0.0f, -1.0f,
            0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, -w, -h, d, 0.0f, -1.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,

            // 2. Back Vertical Face (-Z) - Normal (0, 0, -1), Tangent (-1, 0, 0), Bitangent (0, 1, 0)
            w, -h, -d, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, -w, -h, -d,
            0.0f, 0.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, -w, h, -d, 0.0f, 0.0f,
            -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, w, h, -d, 0.0f, 0.0f, -1.0f, 0.0f,
            1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f,

            // 3. Sloped / Ramp Face - Normal (0, ny, nz), Tangent (1, 0, 0), Bitangent (0, by, bz)
            -w, -h, d, 0.0f, ny, nz, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, by, bz, 1.0f, 1.0f, 1.0f, w, -h, d, 0.0f, ny,
            nz, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, by, bz, 1.0f, 1.0f, 1.0f, w, h, -d, 0.0f, ny, nz, 1.0f, 1.0f, 1.0f,
            0.0f, 0.0f, 0.0f, by, bz, 1.0f, 1.0f, 1.0f, -w, h, -d, 0.0f, ny, nz, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, by,
            bz, 1.0f, 1.0f, 1.0f,

            // 4. Left Triangular Side Face (-X) - Normal (-1, 0, 0), Tangent (0, 0, 1), Bitangent (0, 1, 0)
            -w, -h, -d, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, -w, -h, d,
            -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, -w, h, -d, -1.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f,

            // 5. Right Triangular Side Face (+X) - Normal (1, 0, 0), Tangent (0, 0, -1), Bitangent (0, 1, 0)
            w, -h, d, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, w, -h, -d,
            1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, w, h, -d, 1.0f, 0.0f,
            0.0f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f};

        uint32_t indices[] = {
            0,  1,  2,  2,  3,  0, // Bottom Face
            4,  5,  6,  6,  7,  4, // Back Face
            8,  9,  10, 10, 11, 8, // Sloped Face
            12, 13, 14,            // Left Side
            15, 16, 17             // Right Side
        };

        TRef<FVertexArray> vertexArray = FVertexArray::Create();

        TRef<FVertexBuffer> vertexBuffer = FVertexBuffer::Create(vertices, sizeof(vertices));
        vertexBuffer->SetLayout({{EShaderDataType::Float3, "aPos"},
                                 {EShaderDataType::Float3, "aNormal"},
                                 {EShaderDataType::Float2, "aTexCoord"},
                                 {EShaderDataType::Float3, "aTangent"},
                                 {EShaderDataType::Float3, "aBitangent"},
                                 {EShaderDataType::Float3, "aColor"}});
        vertexArray->AddVertexBuffer(vertexBuffer);

        TRef<FIndexBuffer> indexBuffer = FIndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t));
        vertexArray->SetIndexBuffer(indexBuffer);

        return vertexArray;
    }

    TRef<FVertexArray> FMeshPrimitives::CreatePyramid(float InWidth, float InHeight, float InDepth) {
        float w = InWidth * 0.5f;
        float h = InHeight * 0.5f;
        float d = InDepth * 0.5f;

        // Front/Back normal & bitangent (slope with depth d and height 2h)
        float zSlopeLen = std::sqrt(d * d + 4.0f * h * h);
        float fnY = d / zSlopeLen;
        float fnZ = (2.0f * h) / zSlopeLen;
        float fbY = (2.0f * h) / zSlopeLen;
        float fbZ = -d / zSlopeLen;

        // Left/Right normal & bitangent (slope with width w and height 2h)
        float xSlopeLen = std::sqrt(w * w + 4.0f * h * h);
        float rnX = (2.0f * h) / xSlopeLen;
        float rnY = w / xSlopeLen;
        float rbX = -w / xSlopeLen;
        float rbY = (2.0f * h) / xSlopeLen;

        // 16 vertices across 5 faces (1 base + 4 triangular sides):
        // Format: Pos(3), Normal(3), UV(2), Tangent(3), Bitangent(3), Color(3) = 17 floats
        float vertices[] = {
            // 1. Base Face (-Y) - Normal (0, -1, 0), Tangent (1, 0, 0), Bitangent (0, 0, 1)
            -w, -h, -d, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, w, -h, -d,
            0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, w, -h, d, 0.0f, -1.0f,
            0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, -w, -h, d, 0.0f, -1.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,

            // 2. Front Face (+Z) - Normal (0, fnY, fnZ), Tangent (1, 0, 0), Bitangent (0, fbY, fbZ)
            -w, -h, d, 0.0f, fnY, fnZ, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, fbY, fbZ, 1.0f, 1.0f, 1.0f, w, -h, d, 0.0f,
            fnY, fnZ, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, fbY, fbZ, 1.0f, 1.0f, 1.0f, 0.0f, h, 0.0f, 0.0f, fnY, fnZ,
            0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, fbY, fbZ, 1.0f, 1.0f, 1.0f,

            // 3. Right Face (+X) - Normal (rnX, rnY, 0), Tangent (0, 0, -1), Bitangent (rbX, rbY, 0)
            w, -h, d, rnX, rnY, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, rbX, rbY, 0.0f, 1.0f, 1.0f, 1.0f, w, -h, -d, rnX,
            rnY, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, rbX, rbY, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, h, 0.0f, rnX, rnY, 0.0f,
            0.5f, 1.0f, 0.0f, 0.0f, -1.0f, rbX, rbY, 0.0f, 1.0f, 1.0f, 1.0f,

            // 4. Back Face (-Z) - Normal (0, fnY, -fnZ), Tangent (-1, 0, 0), Bitangent (0, fbY, -fbZ)
            w, -h, -d, 0.0f, fnY, -fnZ, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, fbY, -fbZ, 1.0f, 1.0f, 1.0f, -w, -h, -d,
            0.0f, fnY, -fnZ, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, fbY, -fbZ, 1.0f, 1.0f, 1.0f, 0.0f, h, 0.0f, 0.0f, fnY,
            -fnZ, 0.5f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, fbY, -fbZ, 1.0f, 1.0f, 1.0f,

            // 5. Left Face (-X) - Normal (-rnX, rnY, 0), Tangent (0, 0, 1), Bitangent (-rbX, rbY, 0)
            -w, -h, -d, -rnX, rnY, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, -rbX, rbY, 0.0f, 1.0f, 1.0f, 1.0f, -w, -h, d,
            -rnX, rnY, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, -rbX, rbY, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, h, 0.0f, -rnX, rnY,
            0.0f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, -rbX, rbY, 0.0f, 1.0f, 1.0f, 1.0f};

        uint32_t indices[] = {
            0,  1,  2,  2, 3, 0, // Base
            4,  5,  6,           // Front
            7,  8,  9,           // Right
            10, 11, 12,          // Back
            13, 14, 15           // Left
        };

        TRef<FVertexArray> vertexArray = FVertexArray::Create();

        TRef<FVertexBuffer> vertexBuffer = FVertexBuffer::Create(vertices, sizeof(vertices));
        vertexBuffer->SetLayout({{EShaderDataType::Float3, "aPos"},
                                 {EShaderDataType::Float3, "aNormal"},
                                 {EShaderDataType::Float2, "aTexCoord"},
                                 {EShaderDataType::Float3, "aTangent"},
                                 {EShaderDataType::Float3, "aBitangent"},
                                 {EShaderDataType::Float3, "aColor"}});
        vertexArray->AddVertexBuffer(vertexBuffer);

        TRef<FIndexBuffer> indexBuffer = FIndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t));
        vertexArray->SetIndexBuffer(indexBuffer);

        return vertexArray;
    }

} // namespace Leon
