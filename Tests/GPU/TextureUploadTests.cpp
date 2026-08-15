#include <doctest/doctest.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <cmath>

TEST_SUITE("GPU - Texture Upload, Half-Float & Seamless Cubemap Integration") {

    TEST_CASE("Headless OpenGL Context - Float Texture Upload & Readback Invariants") {
        if (!glfwInit()) {
            MESSAGE("GLFW init failed — skipping GPU integration test.");
            return;
        }

        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWwindow* window = glfwCreateWindow(64, 64, "HeadlessRendererTests", nullptr, nullptr);
        if (!window) {
            MESSAGE("Could not create headless OpenGL 4.5 context — skipping GPU integration test.");
            glfwTerminate();
            return;
        }

        glfwMakeContextCurrent(window);
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            MESSAGE("GLAD failed to load OpenGL 4.5 entry points.");
            glfwDestroyWindow(window);
            glfwTerminate();
            return;
        }

        // 1. Test Seamless Cubemap OpenGL Flag Enablement
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        GLboolean seamlessEnabled = glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        CHECK(seamlessEnabled == GL_TRUE);

        // 2. Test GL_RGBA16F Cubemap Creation, Upload and Readback
        const int cubeSize = 16;
        GLuint cubeTex = 0;
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cubeTex);
        glTextureStorage2D(cubeTex, 1, GL_RGBA16F, cubeSize, cubeSize);

        std::vector<float> uploadFaceData(cubeSize * cubeSize * 4, 3.1415f);
        for (int face = 0; face < 6; ++face) {
            glTextureSubImage3D(cubeTex, 0, 0, 0, face, cubeSize, cubeSize, 1, GL_RGBA, GL_FLOAT, uploadFaceData.data());
        }

        std::vector<float> readbackData(cubeSize * cubeSize * 4 * 6, 0.0f);
        glGetTextureImage(cubeTex, 0, GL_RGBA, GL_FLOAT, static_cast<GLsizei>(readbackData.size() * sizeof(float)), readbackData.data());

        for (size_t i = 0; i < readbackData.size(); ++i) {
            CHECK(!std::isnan(readbackData[i]));
            CHECK(!std::isinf(readbackData[i]));
            // Half-float precision has ~11 bits of mantissa (eps ~ 0.005)
            CHECK(readbackData[i] == doctest::Approx(3.1415f).epsilon(0.005f));
        }

        glDeleteTextures(1, &cubeTex);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}
