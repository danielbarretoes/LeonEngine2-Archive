#pragma once

#include "engine/core/Base.hpp"
#include "engine/renderer/Buffer.hpp"
#include "engine/renderer/Framebuffer.hpp"
#include "engine/renderer/GraphicsContext.hpp"
#include "engine/renderer/RenderAPI.hpp"
#include "engine/renderer/Shader.hpp"
#include "engine/renderer/Texture.hpp"
#include "engine/renderer/VertexArray.hpp"
#include <string>

namespace Leon {

    class IRenderDriver {
    public:
        virtual ~IRenderDriver() = default;

        virtual TScope<IGraphicsContext> CreateGraphicsContext(void* InWindowHandle) = 0;
        virtual TScope<IRenderAPI> CreateRenderAPI() = 0;
        virtual TRef<FVertexBuffer> CreateVertexBuffer(unsigned int InSize) = 0;
        virtual TRef<FVertexBuffer> CreateVertexBuffer(const float* InVertices, unsigned int InSize) = 0;
        virtual TRef<FIndexBuffer> CreateIndexBuffer(const uint32_t* InIndices, unsigned int InCount) = 0;
        virtual TRef<FVertexArray> CreateVertexArray() = 0;
        virtual TRef<FShader> CreateShader(const std::string& InFilePath) = 0;
        virtual TRef<FShader> CreateShader(const std::string& InName, const std::string& InVertexSrc,
                                           const std::string& InFragmentSrc) = 0;
        virtual TRef<FTexture2D> CreateTexture2D(uint32_t InWidth, uint32_t InHeight) = 0;
        virtual TRef<FTexture2D> CreateTexture2D(const std::string& InPath) = 0;
        virtual TRef<FFramebuffer> CreateFramebuffer(const FFramebufferSpecification& InSpec) = 0;
    };

    using RenderDriver = IRenderDriver;

    class FRenderDriverRegistry {
    public:
        static void RegisterDriver(ERenderAPI InAPI, TScope<IRenderDriver> InDriver);
        static IRenderDriver* GetDriver(ERenderAPI InAPI);
        static IRenderDriver* GetActiveDriver();
    };

    using RenderDriverRegistry = FRenderDriverRegistry;

} // namespace Leon
