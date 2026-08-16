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
#include "Renderer/FWorldRenderer.hpp"
#include "RHI/FShader.hpp"
#include "RHI/FRenderCommand.hpp"

#include "FOpenGLRenderDriver.hpp"

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
        glm::vec4 Tangent;
        glm::vec3 Color{1.0f};
        glm::vec2 LightmapUV{0.0f};
    };

    class FHeadlessGLContext {
    public:
        static FHeadlessGLContext& Get() {
            static FHeadlessGLContext instance;
            return instance;
        }

        bool IsValid() const { return bInitialized; }

        GLuint GetFBO() const { return FBO; }
        GLuint GetColorTexture() const { return ColorTexture; }
        GLuint GetVAO() const { return VAO; }
        GLuint GetCameraUBO() const { return CameraUBO; }
        GLuint GetLightingUBO() const { return LightingUBO; }

        void BindFramebuffer(int width = 1, int height = 1) {
            glBindFramebuffer(GL_FRAMEBUFFER, FBO);
            glViewport(0, 0, width, height);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glDisable(GL_BLEND);
            if (GLAD_GL_ARB_framebuffer_sRGB || GLAD_GL_EXT_framebuffer_sRGB) {
                glDisable(GL_FRAMEBUFFER_SRGB);
            }
            glBindBufferBase(GL_UNIFORM_BUFFER, 0, CameraUBO);
            glBindBufferBase(GL_UNIFORM_BUFFER, 1, LightingUBO);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        void UpdateCameraUBO(const FCameraBufferData& data) {
            glBindBuffer(GL_UNIFORM_BUFFER, CameraUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FCameraBufferData), &data);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }

        void UpdateLightingUBO(const FLightingBufferData& data) {
            glBindBuffer(GL_UNIFORM_BUFFER, LightingUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FLightingBufferData), &data);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }

        void DrawQuad() {
            glBindVertexArray(VAO);
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
                    glBindTexture(GL_TEXTURE_2D, DefaultFlatNormalTex);
                } else if (unit == 3) {
                    glBindTexture(GL_TEXTURE_2D, DefaultWhiteTex); // AO default 1.0
                } else {
                    glBindTexture(GL_TEXTURE_2D, DefaultWhiteTex);
                }
            }
            // Unit 6: BRDF LUT
            glActiveTexture(GL_TEXTURE6);
            glBindTexture(GL_TEXTURE_2D, DefaultWhiteTex);

            // Unit 7 & 8: Irradiance & Prefilter cubemaps
            glActiveTexture(GL_TEXTURE7);
            glBindTexture(GL_TEXTURE_CUBE_MAP, DefaultCubeTex);
            glActiveTexture(GL_TEXTURE8);
            glBindTexture(GL_TEXTURE_CUBE_MAP, DefaultCubeTex);

            // Unit 9: Emissive
            glActiveTexture(GL_TEXTURE9);
            glBindTexture(GL_TEXTURE_2D, DefaultBlackTex);

            // Unit 10: Cascade Shadow Map (Texture2DArrayShadow)
            glActiveTexture(GL_TEXTURE10);
            glBindTexture(GL_TEXTURE_2D_ARRAY, DefaultShadowArrayTex);

            // Unit 11: Spot Shadow Map (Texture2DShadow)
            glActiveTexture(GL_TEXTURE11);
            glBindTexture(GL_TEXTURE_2D, DefaultShadowTex);
        }

        void ResetShaderUniforms(const TRef<FShader>& shader) {
            if (!shader) return;
            shader->Bind();
            shader->SetInt("u_AlphaMode", 0);
            shader->SetFloat("u_AlphaCutoff", 0.5f);
            shader->SetInt("u_UseAlbedoMap", 0);
            shader->SetInt("u_UseNormalMap", 0);
            shader->SetInt("u_UseMetallicMap", 0);
            shader->SetInt("u_UseAOMap", 0);
            shader->SetInt("u_UseRoughnessMap", 0);
            shader->SetInt("u_UseEmissiveMap", 0);
            shader->SetInt("u_UsePlanarReflection", 0);
            shader->SetInt("u_UseIBL", 0);
            shader->SetInt("u_UseShadows", 0);
            shader->SetInt("u_UseSpotShadows", 0);
            shader->SetInt("u_DebugMode", 0);
            shader->SetFloat3("u_AlbedoColor", 1.0f, 1.0f, 1.0f);
            shader->SetFloat("u_Metallic", 0.0f);
            shader->SetFloat("u_Roughness", 0.5f);
            shader->SetFloat("u_AO", 1.0f);
            shader->SetFloat("u_NormalScale", 1.0f);
            shader->SetFloat("u_OcclusionStrength", 1.0f);
            shader->SetFloat3("u_EmissiveColor", 0.0f, 0.0f, 0.0f);
            shader->SetFloat("u_EmissiveIntensity", 0.0f);
            shader->SetFloat2("u_UVTiling", 1.0f, 1.0f);
            shader->SetFloat2("u_UVOffset", 0.0f, 0.0f);
        }

        GLuint GetDefaultShadowArrayTex() const { return DefaultShadowArrayTex; }
        GLuint GetDefaultShadowTex() const { return DefaultShadowTex; }

        GLuint Create1x1Texture(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
            GLuint tex = 0;
            uint8_t data[4] = { r, g, b, a };
            glCreateTextures(GL_TEXTURE_2D, 1, &tex);
            glTextureStorage2D(tex, 1, GL_RGBA8, 1, 1);
            glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTextureSubImage2D(tex, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
            return tex;
        }

        GLuint Create1x1SRGBTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
            GLuint tex = 0;
            uint8_t data[4] = { r, g, b, a };
            glCreateTextures(GL_TEXTURE_2D, 1, &tex);
            glTextureStorage2D(tex, 1, GL_SRGB8_ALPHA8, 1, 1);
            glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTextureSubImage2D(tex, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
            return tex;
        }

        GLuint Create1x1FloatTexture(float r, float g, float b, float a = 1.0f) {
            GLuint tex = 0;
            float data[4] = { r, g, b, a };
            glCreateTextures(GL_TEXTURE_2D, 1, &tex);
            glTextureStorage2D(tex, 1, GL_RGBA32F, 1, 1);
            glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTextureSubImage2D(tex, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, data);
            return tex;
        }

        GLuint Create2x2Texture(const uint32_t pixels[4]) {
            GLuint tex = 0;
            glCreateTextures(GL_TEXTURE_2D, 1, &tex);
            glTextureStorage2D(tex, 1, GL_RGBA8, 2, 2);
            glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTextureSubImage2D(tex, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            return tex;
        }

        void DestroyTexture(GLuint texId) {
            if (texId != 0) {
                glDeleteTextures(1, &texId);
            }
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

            NativeWindow = glfwCreateWindow(64, 64, "LeonHeadlessContext", nullptr, nullptr);
            if (!NativeWindow) {
                std::cerr << "[TEST ERROR] Could not create headless GLFW window!\n";
                glfwTerminate();
                return;
            }

            glfwMakeContextCurrent(NativeWindow);

            if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
                std::cerr << "[TEST ERROR] GLAD loader failed!\n";
                glfwDestroyWindow(NativeWindow);
                glfwTerminate();
                return;
            }

            // Register and activate OpenGL RenderDriver for FShader/FRenderCommand
            IRenderAPI::SetAPI(ERenderAPI::OpenGL);
            FRenderDriverRegistry::RegisterDriver(ERenderAPI::OpenGL, MakeScope<FOpenGLRenderDriver>());
            FRenderCommand::Init();

            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

            InitFBO();
            InitGeometry();
            InitUBOs();
            InitDefaultTextures();

            bInitialized = true;
        }

        ~FHeadlessGLContext() {
            if (bInitialized) {
                glDeleteFramebuffers(1, &FBO);
                glDeleteTextures(1, &ColorTexture);
                glDeleteRenderbuffers(1, &DepthRBO);

                glDeleteVertexArrays(1, &VAO);
                glDeleteBuffers(1, &VBO);
                glDeleteBuffers(1, &EBO);

                glDeleteBuffers(1, &CameraUBO);
                glDeleteBuffers(1, &LightingUBO);

                glDeleteTextures(1, &DefaultWhiteTex);
                glDeleteTextures(1, &DefaultBlackTex);
                glDeleteTextures(1, &DefaultFlatNormalTex);
                glDeleteTextures(1, &DefaultCubeTex);
                glDeleteTextures(1, &DefaultShadowTex);
                glDeleteTextures(1, &DefaultShadowArrayTex);

                glfwDestroyWindow(NativeWindow);
                glfwTerminate();
            }
        }

        void InitFBO() {
            glCreateFramebuffers(1, &FBO);
            glBindFramebuffer(GL_FRAMEBUFFER, FBO);

            glCreateTextures(GL_TEXTURE_2D, 1, &ColorTexture);
            glBindTexture(GL_TEXTURE_2D, ColorTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ColorTexture, 0);

            glCreateRenderbuffers(1, &DepthRBO);
            glBindRenderbuffer(GL_RENDERBUFFER, DepthRBO);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, DepthRBO);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        void InitGeometry() {
            // Fullscreen / Unit Quad centered facing +Z with normal +Z
            std::vector<FTestVertex> vertices = {
                { glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },
                { glm::vec3( 1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },
                { glm::vec3( 1.0f,  1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) },
                { glm::vec3(-1.0f,  1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) }
            };

            std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

            glCreateVertexArrays(1, &VAO);
            glBindVertexArray(VAO);

            glCreateBuffers(1, &VBO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(FTestVertex), vertices.data(), GL_STATIC_DRAW);

            glCreateBuffers(1, &EBO);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
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

            // layout(location = 3) in vec4 aTangent
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(FTestVertex), (void*)offsetof(FTestVertex, Tangent));

            // layout(location = 4) in vec3 aColor
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(FTestVertex), (void*)offsetof(FTestVertex, Color));

            // layout(location = 5) in vec2 aLightmapUV
            glEnableVertexAttribArray(5);
            glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(FTestVertex), (void*)offsetof(FTestVertex, LightmapUV));

            glBindVertexArray(0);
        }

        void InitUBOs() {
            // Camera UBO: Binding 0
            glCreateBuffers(1, &CameraUBO);
            glBindBuffer(GL_UNIFORM_BUFFER, CameraUBO);
            glBufferData(GL_UNIFORM_BUFFER, sizeof(FCameraBufferData), nullptr, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_UNIFORM_BUFFER, 0, CameraUBO);

            // Lighting UBO: Binding 1
            glCreateBuffers(1, &LightingUBO);
            glBindBuffer(GL_UNIFORM_BUFFER, LightingUBO);
            glBufferData(GL_UNIFORM_BUFFER, sizeof(FLightingBufferData), nullptr, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_UNIFORM_BUFFER, 1, LightingUBO);

            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }

        void InitDefaultTextures() {
            // White 1x1
            uint32_t white = 0xFFFFFFFF;
            glCreateTextures(GL_TEXTURE_2D, 1, &DefaultWhiteTex);
            glTextureStorage2D(DefaultWhiteTex, 1, GL_RGBA8, 1, 1);
            glTextureSubImage2D(DefaultWhiteTex, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &white);

            // Black 1x1
            uint32_t black = 0xFF000000;
            glCreateTextures(GL_TEXTURE_2D, 1, &DefaultBlackTex);
            glTextureStorage2D(DefaultBlackTex, 1, GL_RGBA8, 1, 1);
            glTextureSubImage2D(DefaultBlackTex, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &black);

            // Flat normal 1x1 (128, 128, 255, 255)
            uint32_t normal = 0xFFFF8080;
            glCreateTextures(GL_TEXTURE_2D, 1, &DefaultFlatNormalTex);
            glTextureStorage2D(DefaultFlatNormalTex, 1, GL_RGBA8, 1, 1);
            glTextureSubImage2D(DefaultFlatNormalTex, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &normal);

            // Default Cubemap 1x1
            glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &DefaultCubeTex);
            glTextureStorage2D(DefaultCubeTex, 1, GL_RGBA8, 1, 1);
            for (int f = 0; f < 6; ++f) {
                glTextureSubImage3D(DefaultCubeTex, 0, 0, 0, f, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &black);
            }

            // Default Shadow 2D (Compare mode)
            glCreateTextures(GL_TEXTURE_2D, 1, &DefaultShadowTex);
            glTextureStorage2D(DefaultShadowTex, 1, GL_DEPTH_COMPONENT24, 1, 1);
            glTextureParameteri(DefaultShadowTex, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTextureParameteri(DefaultShadowTex, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
            float clearDepth = 1.0f;
            glClearTexImage(DefaultShadowTex, 0, GL_DEPTH_COMPONENT, GL_FLOAT, &clearDepth);

            // Default Shadow 2D Array
            glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &DefaultShadowArrayTex);
            glTextureStorage3D(DefaultShadowArrayTex, 1, GL_DEPTH_COMPONENT24, 1, 1, 4);
            glTextureParameteri(DefaultShadowArrayTex, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTextureParameteri(DefaultShadowArrayTex, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
            glClearTexImage(DefaultShadowArrayTex, 0, GL_DEPTH_COMPONENT, GL_FLOAT, &clearDepth);
        }

        GLFWwindow* NativeWindow = nullptr;
        bool bInitialized = false;

        GLuint FBO = 0;
        GLuint ColorTexture = 0;
        GLuint DepthRBO = 0;

        GLuint VAO = 0;
        GLuint VBO = 0;
        GLuint EBO = 0;

        GLuint CameraUBO = 0;
        GLuint LightingUBO = 0;

        GLuint DefaultWhiteTex = 0;
        GLuint DefaultBlackTex = 0;
        GLuint DefaultFlatNormalTex = 0;
        GLuint DefaultCubeTex = 0;
        GLuint DefaultShadowTex = 0;
        GLuint DefaultShadowArrayTex = 0;
    };

} // namespace Leon::TestGPU
