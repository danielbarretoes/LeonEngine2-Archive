#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "renderer/SceneRenderer.hpp"
#include "renderer/Shader.hpp"

#include "OpenGLRenderDriver.hpp"

namespace Leon::TestGPU {

    inline void SetMat4(const TRef<FShader>& shader, const std::string& name, const glm::mat4& mat) {
        shader->SetMat4(name, glm::value_ptr(mat));
    }
    inline void SetMat3(const TRef<FShader>& shader, const std::string& name, const glm::mat3& mat) {
        shader->SetMat3(name, glm::value_ptr(mat));
    }
    inline void SetFloat3(const TRef<FShader>& shader, const std::string& name, const glm::vec3& v) {
        shader->SetFloat3(name, v.x, v.y, v.z);
    }
    inline void SetFloat2(const TRef<FShader>& shader, const std::string& name, const glm::vec2& v) {
        shader->SetFloat2(name, v.x, v.y);
    }

    struct FTestVertex {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoord;
        glm::vec3 Tangent;
        glm::vec3 Bitangent;
        glm::vec3 Color{1.0f};
    };

    class FHeadlessGLContext {
    public:
        static FHeadlessGLContext& Get() {
            static FHeadlessGLContext instance;
            return instance;
        }

        bool IsValid() const { return m_bInitialized; }

        GLuint GetFBO() const { return m_FBO; }
        GLuint GetColorTexture() const { return m_ColorTexture; }
        GLuint GetVAO() const { return m_VAO; }
        GLuint GetCameraUBO() const { return m_CameraUBO; }
        GLuint GetLightingUBO() const { return m_LightingUBO; }

        void BindFramebuffer(int width = 1, int height = 1) {
            glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
            glViewport(0, 0, width, height);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        void UpdateCameraUBO(const FCameraBufferData& data) {
            glBindBuffer(GL_UNIFORM_BUFFER, m_CameraUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FCameraBufferData), &data);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }

        void UpdateLightingUBO(const FLightingBufferData& data) {
            glBindBuffer(GL_UNIFORM_BUFFER, m_LightingUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FLightingBufferData), &data);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }

        void DrawQuad() {
            glBindVertexArray(m_VAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        }

        glm::vec4 ReadPixel(int x = 0, int y = 0) {
            glm::vec4 pixel(0.0f);
            glReadPixels(x, y, 1, 1, GL_RGBA, GL_FLOAT, &pixel);
            return pixel;
        }

        // Helper to bind default 1x1 fallback textures to all 12 texture units
        void BindDefaultTextures() {
            for (int unit = 0; unit < 6; ++unit) {
                glActiveTexture(GL_TEXTURE0 + unit);
                if (unit == 1) {
                    glBindTexture(GL_TEXTURE_2D, m_DefaultFlatNormalTex);
                } else if (unit == 3) {
                    glBindTexture(GL_TEXTURE_2D, m_DefaultWhiteTex); // AO default 1.0
                } else {
                    glBindTexture(GL_TEXTURE_2D, m_DefaultWhiteTex);
                }
            }
            // Unit 6: BRDF LUT
            glActiveTexture(GL_TEXTURE6);
            glBindTexture(GL_TEXTURE_2D, m_DefaultWhiteTex);

            // Unit 7 & 8: Irradiance & Prefilter cubemaps
            glActiveTexture(GL_TEXTURE7);
            glBindTexture(GL_TEXTURE_CUBE_MAP, m_DefaultCubeTex);
            glActiveTexture(GL_TEXTURE8);
            glBindTexture(GL_TEXTURE_CUBE_MAP, m_DefaultCubeTex);

            // Unit 9: Emissive
            glActiveTexture(GL_TEXTURE9);
            glBindTexture(GL_TEXTURE_2D, m_DefaultBlackTex);

            // Unit 10: Cascade Shadow Map (Texture2DArrayShadow)
            glActiveTexture(GL_TEXTURE10);
            glBindTexture(GL_TEXTURE_2D_ARRAY, m_DefaultShadowArrayTex);

            // Unit 11: Spot Shadow Map (Texture2DShadow)
            glActiveTexture(GL_TEXTURE11);
            glBindTexture(GL_TEXTURE_2D, m_DefaultShadowTex);
        }

    private:
        FHeadlessGLContext() {
            if (!glfwInit()) {
                std::cerr << "[TEST ERROR] GLFW init failed!\n";
                return;
            }

            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

            m_Window = glfwCreateWindow(64, 64, "LeonHeadlessContext", nullptr, nullptr);
            if (!m_Window) {
                std::cerr << "[TEST ERROR] Could not create headless GLFW window!\n";
                glfwTerminate();
                return;
            }

            glfwMakeContextCurrent(m_Window);

            if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
                std::cerr << "[TEST ERROR] GLAD loader failed!\n";
                glfwDestroyWindow(m_Window);
                glfwTerminate();
                return;
            }

            // Register and activate OpenGL RenderDriver for FShader/FRenderCommand
            IRenderAPI::SetAPI(ERenderAPI::OpenGL);
            FRenderDriverRegistry::RegisterDriver(ERenderAPI::OpenGL, MakeScope<FOpenGLRenderDriver>());

            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

            InitFBO();
            InitGeometry();
            InitUBOs();
            InitDefaultTextures();

            m_bInitialized = true;
        }

        ~FHeadlessGLContext() {
            if (m_bInitialized) {
                glDeleteFramebuffers(1, &m_FBO);
                glDeleteTextures(1, &m_ColorTexture);
                glDeleteRenderbuffers(1, &m_DepthRBO);

                glDeleteVertexArrays(1, &m_VAO);
                glDeleteBuffers(1, &m_VBO);
                glDeleteBuffers(1, &m_EBO);

                glDeleteBuffers(1, &m_CameraUBO);
                glDeleteBuffers(1, &m_LightingUBO);

                glDeleteTextures(1, &m_DefaultWhiteTex);
                glDeleteTextures(1, &m_DefaultBlackTex);
                glDeleteTextures(1, &m_DefaultFlatNormalTex);
                glDeleteTextures(1, &m_DefaultCubeTex);
                glDeleteTextures(1, &m_DefaultShadowTex);
                glDeleteTextures(1, &m_DefaultShadowArrayTex);

                glfwDestroyWindow(m_Window);
                glfwTerminate();
            }
        }

        void InitFBO() {
            glCreateFramebuffers(1, &m_FBO);
            glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

            glCreateTextures(GL_TEXTURE_2D, 1, &m_ColorTexture);
            glBindTexture(GL_TEXTURE_2D, m_ColorTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorTexture, 0);

            glCreateRenderbuffers(1, &m_DepthRBO);
            glBindRenderbuffer(GL_RENDERBUFFER, m_DepthRBO);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_DepthRBO);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        void InitGeometry() {
            // Fullscreen / Unit Quad centered facing +Z with normal +Z
            std::vector<FTestVertex> vertices = {
                { glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f) },
                { glm::vec3( 1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f) },
                { glm::vec3( 1.0f,  1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f) },
                { glm::vec3(-1.0f,  1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f) }
            };

            std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

            glCreateVertexArrays(1, &m_VAO);
            glBindVertexArray(m_VAO);

            glCreateBuffers(1, &m_VBO);
            glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(FTestVertex), vertices.data(), GL_STATIC_DRAW);

            glCreateBuffers(1, &m_EBO);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

            // layout(location = 0) in vec3 aPos
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FTestVertex), (void*)offsetof(FTestVertex, Position));

            // layout(location = 1) in vec3 aNormal
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(FTestVertex), (void*)offsetof(FTestVertex, Normal));

            // layout(location = 2) in vec2 aTexCoord
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(FTestVertex), (void*)offsetof(FTestVertex, TexCoord));

            // layout(location = 3) in vec3 aTangent
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(FTestVertex), (void*)offsetof(FTestVertex, Tangent));

            // layout(location = 4) in vec3 aBitangent
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(FTestVertex), (void*)offsetof(FTestVertex, Bitangent));

            // layout(location = 5) in vec3 aColor
            glEnableVertexAttribArray(5);
            glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(FTestVertex), (void*)offsetof(FTestVertex, Color));

            glBindVertexArray(0);
        }

        void InitUBOs() {
            // Camera UBO: Binding 0
            glCreateBuffers(1, &m_CameraUBO);
            glBindBuffer(GL_UNIFORM_BUFFER, m_CameraUBO);
            glBufferData(GL_UNIFORM_BUFFER, sizeof(FCameraBufferData), nullptr, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_CameraUBO);

            // Lighting UBO: Binding 1
            glCreateBuffers(1, &m_LightingUBO);
            glBindBuffer(GL_UNIFORM_BUFFER, m_LightingUBO);
            glBufferData(GL_UNIFORM_BUFFER, sizeof(FLightingBufferData), nullptr, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_LightingUBO);

            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }

        void InitDefaultTextures() {
            // White 1x1
            uint32_t white = 0xFFFFFFFF;
            glCreateTextures(GL_TEXTURE_2D, 1, &m_DefaultWhiteTex);
            glTextureStorage2D(m_DefaultWhiteTex, 1, GL_RGBA8, 1, 1);
            glTextureSubImage2D(m_DefaultWhiteTex, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &white);

            // Black 1x1
            uint32_t black = 0xFF000000;
            glCreateTextures(GL_TEXTURE_2D, 1, &m_DefaultBlackTex);
            glTextureStorage2D(m_DefaultBlackTex, 1, GL_RGBA8, 1, 1);
            glTextureSubImage2D(m_DefaultBlackTex, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &black);

            // Flat normal 1x1 (128, 128, 255, 255)
            uint32_t normal = 0xFFFF8080;
            glCreateTextures(GL_TEXTURE_2D, 1, &m_DefaultFlatNormalTex);
            glTextureStorage2D(m_DefaultFlatNormalTex, 1, GL_RGBA8, 1, 1);
            glTextureSubImage2D(m_DefaultFlatNormalTex, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &normal);

            // Default Cubemap 1x1
            glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_DefaultCubeTex);
            glTextureStorage2D(m_DefaultCubeTex, 1, GL_RGBA8, 1, 1);
            for (int f = 0; f < 6; ++f) {
                glTextureSubImage3D(m_DefaultCubeTex, 0, 0, 0, f, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &black);
            }

            // Default Shadow 2D (Compare mode)
            glCreateTextures(GL_TEXTURE_2D, 1, &m_DefaultShadowTex);
            glTextureStorage2D(m_DefaultShadowTex, 1, GL_DEPTH_COMPONENT24, 1, 1);
            glTextureParameteri(m_DefaultShadowTex, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTextureParameteri(m_DefaultShadowTex, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

            // Default Shadow 2D Array
            glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_DefaultShadowArrayTex);
            glTextureStorage3D(m_DefaultShadowArrayTex, 1, GL_DEPTH_COMPONENT24, 1, 1, 4);
            glTextureParameteri(m_DefaultShadowArrayTex, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTextureParameteri(m_DefaultShadowArrayTex, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        }

        GLFWwindow* m_Window = nullptr;
        bool m_bInitialized = false;

        GLuint m_FBO = 0;
        GLuint m_ColorTexture = 0;
        GLuint m_DepthRBO = 0;

        GLuint m_VAO = 0;
        GLuint m_VBO = 0;
        GLuint m_EBO = 0;

        GLuint m_CameraUBO = 0;
        GLuint m_LightingUBO = 0;

        GLuint m_DefaultWhiteTex = 0;
        GLuint m_DefaultBlackTex = 0;
        GLuint m_DefaultFlatNormalTex = 0;
        GLuint m_DefaultCubeTex = 0;
        GLuint m_DefaultShadowTex = 0;
        GLuint m_DefaultShadowArrayTex = 0;
    };

} // namespace Leon::TestGPU
