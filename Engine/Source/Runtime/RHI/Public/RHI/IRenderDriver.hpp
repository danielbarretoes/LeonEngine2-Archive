#pragma once

#include "Core/Base.hpp"
#include "RHI/FBuffer.hpp"
#include "RHI/FFramebuffer.hpp"
#include "RHI/IGraphicsContext.hpp"
#include "RHI/IRenderAPI.hpp"
#include "RHI/FShader.hpp"
#include "RHI/FTexture.hpp"
#include "RHI/FVertexArray.hpp"
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
        virtual TRef<FTexture2D> CreateTexture2DWithFormat(uint32_t InWidth, uint32_t InHeight,
                                                           ETextureFormat InFormat) = 0;
        virtual TRef<class FTextureCube> CreateTextureCube(uint32_t InWidth, uint32_t InHeight,
                                                           bool bInHDR = false) = 0;
        virtual TRef<class FTextureCube> CreateTextureCube(const std::vector<std::string>& InFacePaths) = 0;
        virtual TRef<FFramebuffer> CreateFramebuffer(const FFramebufferSpecification& InSpec) = 0;
        virtual TRef<FUniformBuffer> CreateUniformBuffer(unsigned int InSize, unsigned int InBinding) = 0;
    };

    class FRenderDriverRegistry {
    public:
        static void RegisterDriver(ERenderAPI InAPI, TScope<IRenderDriver> InDriver);
        static IRenderDriver* GetDriver(ERenderAPI InAPI);
        static IRenderDriver* GetActiveDriver();
    };

} // namespace Leon
