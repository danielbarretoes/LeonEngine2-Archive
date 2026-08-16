#pragma once

#include "Core/Base.hpp"
#include <string>
#include <vector>

namespace Leon {

    class FTextureCube {
    public:
        virtual ~FTextureCube() = default;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual uint32_t GetRendererID() const = 0;

        virtual void Bind(uint32_t InSlot = 0) const = 0;
        virtual void Unbind() const = 0;

        virtual bool IsLoaded() const = 0;

        virtual void SetFaceData(uint32_t InFaceIndex, const void* InData, uint32_t InWidth, uint32_t InHeight,
                                 uint32_t InMipLevel = 0, bool InbHDR = false) = 0;
        virtual void GenerateMipmaps() = 0;

        static TRef<FTextureCube> Create(uint32_t InWidth, uint32_t InHeight, bool InbHDR = false);
        static TRef<FTextureCube> Create(const std::vector<std::string>& InFacePaths);
    };

} // namespace Leon
