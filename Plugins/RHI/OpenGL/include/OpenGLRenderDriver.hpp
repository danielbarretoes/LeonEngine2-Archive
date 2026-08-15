#pragma once

#include "renderer/RenderDriver.hpp"

namespace Leon {

    class FOpenGLRenderDriver : public IRenderDriver {
    public:
        TScope<IGraphicsContext> CreateGraphicsContext(void* InWindowHandle) override;
        TScope<IRenderAPI> CreateRenderAPI() override;
        TRef<FVertexBuffer> CreateVertexBuffer(unsigned int InSize) override;
        TRef<FVertexBuffer> CreateVertexBuffer(const float* InVertices, unsigned int InSize) override;
        TRef<FIndexBuffer> CreateIndexBuffer(const uint32_t* InIndices, unsigned int InCount) override;
        TRef<FVertexArray> CreateVertexArray() override;
        TRef<FShader> CreateShader(const std::string& InFilePath) override;
        TRef<FShader> CreateShader(const std::string& InName, const std::string& InVertexSrc,
                                   const std::string& InFragmentSrc) override;
        TRef<FTexture2D> CreateTexture2D(uint32_t InWidth, uint32_t InHeight) override;
        TRef<FTexture2D> CreateTexture2D(const std::string& InPath) override;
        TRef<FTexture2D> CreateTexture2DWithFormat(uint32_t InWidth, uint32_t InHeight,
                                                   ETextureFormat InFormat) override;
        TRef<class FTextureCube> CreateTextureCube(uint32_t InWidth, uint32_t InHeight, bool InbHDR = false) override;
        TRef<class FTextureCube> CreateTextureCube(const std::vector<std::string>& InFacePaths) override;
        TRef<FFramebuffer> CreateFramebuffer(const FFramebufferSpecification& InSpec) override;
        TRef<FUniformBuffer> CreateUniformBuffer(unsigned int InSize, unsigned int InBinding) override;

        static void Register();
    };

    using OpenGLRenderDriver = FOpenGLRenderDriver;

} // namespace Leon
