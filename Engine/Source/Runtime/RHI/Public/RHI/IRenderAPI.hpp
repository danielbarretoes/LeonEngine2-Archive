#pragma once

#include "Core/Base.hpp"
#include <string>

namespace Leon {

    class FVertexArray;

    enum class ERenderAPI { None = 0, OpenGL = 1 };

    enum class EDepthFunc { Less = 0, LessEqual = 1, Equal = 2, Always = 3 };
    enum class ECullMode { Back = 0, Front = 1, FrontAndBack = 2, None = 3 };
    enum class EBlendFactor {
        Zero = 0,
        One = 1,
        SrcColor = 2,
        OneMinusSrcColor = 3,
        SrcAlpha = 4,
        OneMinusSrcAlpha = 5,
        DstAlpha = 6,
        OneMinusDstAlpha = 7,
        DstColor = 8,
        OneMinusDstColor = 9
    };

    struct FGPUInfo {
        std::string Vendor;
        std::string Renderer;
        std::string Version;
        std::string ShadingLanguageVersion;
    };

    struct FGPUVRAMStats {
        size_t TotalVRAMBytes = 0;
        size_t UsedVRAMBytes = 0;
    };

    class IRenderAPI {
    public:
        virtual ~IRenderAPI() = default;

        virtual void Init() = 0;
        virtual void SetViewport(unsigned int InX, unsigned int InY, unsigned int InWidth, unsigned int InHeight) = 0;
        virtual void SetClearColor(float InR, float InG, float InB, float InA) = 0;
        virtual void Clear() = 0;

        virtual void SetDepthTesting(bool InEnabled) = 0;
        virtual void SetDepthMask(bool InEnabled) = 0;
        virtual void SetDepthFunc(EDepthFunc InFunc) = 0;
        virtual void SetCulling(bool InEnabled, ECullMode InMode = ECullMode::Back) = 0;
        virtual void SetWireframe(bool InEnabled) = 0;
        virtual void SetBlendState(bool InEnabled) = 0;
        virtual void SetBlendFunc(EBlendFactor InSrc, EBlendFactor InDst) = 0;
        virtual void SetClipDistance(bool InEnabled) = 0;
        virtual void SetPolygonOffset(bool InEnabled, float InFactor = 0.0f, float InUnits = 0.0f) = 0;

        virtual uint32_t GetFramebufferBinding() = 0;
        virtual void BindFramebuffer(uint32_t InRendererID) = 0;

        virtual void DrawArrays(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) = 0;
        virtual void DrawIndexed(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount = 0) = 0;
        virtual void DrawIndexedOffset(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                                       unsigned int InIndexOffset) = 0;
        virtual void DrawIndexedInstanced(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                                          unsigned int InInstanceCount) = 0;
        virtual void DrawIndexedOffsetInstanced(const TRef<FVertexArray>& InVertexArray, unsigned int InIndexCount,
                                                unsigned int InIndexOffset, unsigned int InInstanceCount) = 0;
        virtual void DrawLines(const TRef<FVertexArray>& InVertexArray, unsigned int InVertexCount) = 0;
        virtual void SetLineWidth(float InWidth) = 0;

        virtual FGPUInfo GetGPUInfo() = 0;
        virtual FGPUVRAMStats GetGPUVRAMStats() = 0;

        virtual void BeginGPUTimeQuery(uint32_t InSlot) { (void)InSlot; }
        virtual void EndGPUTimeQuery(uint32_t InSlot) { (void)InSlot; }
        virtual void ResolveGPUTimeQueries() {}
        virtual float GetGPUTimeMs(uint32_t InSlot) const {
            (void)InSlot;
            return 0.0f;
        }

        /** Clear backend program-bind caches so recycled native IDs are not skipped after travel. */
        virtual void InvalidateShaderBindingCache() {}

        static ERenderAPI GetAPI() { return CurrentAPI; }
        static void SetAPI(ERenderAPI InAPI) { CurrentAPI = InAPI; }

        static TScope<IRenderAPI> Create();

    private:
        static ERenderAPI CurrentAPI;
    };

} // namespace Leon
