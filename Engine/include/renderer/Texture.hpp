#pragma once

#include "core/Base.hpp"
#include <string>

namespace Leon {

    /**
     * @brief Format hint for explicit-precision texture creation.
     * Used when the default RGBA8 is insufficient (e.g. BRDF LUT, HDR offscreen targets).
     */
    enum class ETextureFormat : uint8_t {
        RGBA8 = 0,   ///< Default: 8-bit RGBA unsigned
        RG16F = 1,   ///< 2-channel half-float  (BRDF LUT)
        RGBA16F = 2, ///< 4-channel half-float  (HDR framebuffer)
        RGBA32F = 3, ///< 4-channel full-float  (HDR environment maps)
    };

    class FTexture {
    public:
        virtual ~FTexture() = default;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual uint32_t GetRendererID() const = 0;

        virtual const std::string& GetPath() const = 0;

        virtual void SetData(void* InData, uint32_t InSize) = 0;

        /// Upload raw float data for float-precision textures (RG16F, RGBA16F, RGBA32F).
        /// Default implementation is a no-op; RHI backends override as needed.
        virtual void SetDataFloat(const void* /*InData*/, uint32_t /*InSize*/) {}

        virtual void Bind(uint32_t InSlot = 0) const = 0;

        virtual bool IsLoaded() const = 0;

        virtual bool operator==(const FTexture& InOther) const = 0;
    };

    class FTexture2D : public FTexture {
    public:
        static TRef<FTexture2D> Create(uint32_t InWidth, uint32_t InHeight);
        static TRef<FTexture2D> Create(const std::string& InPath);

        /// Create a float-precision texture (RG16F, RGBA16F, RGBA32F).
        /// Delegates to the active RHI driver.
        static TRef<FTexture2D> CreateWithFormat(uint32_t InWidth, uint32_t InHeight, ETextureFormat InFormat);
    };

} // namespace Leon
