#pragma once

#include "core/Base.hpp"
#include <string>

namespace Leon {

    class FTexture {
    public:
        virtual ~FTexture() = default;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual uint32_t GetRendererID() const = 0;

        virtual const std::string& GetPath() const = 0;

        virtual void SetData(void* InData, uint32_t InSize) = 0;
        virtual void Bind(uint32_t InSlot = 0) const = 0;

        virtual bool IsLoaded() const = 0;

        virtual bool operator==(const FTexture& InOther) const = 0;
    };

    class FTexture2D : public FTexture {
    public:
        static TRef<FTexture2D> Create(uint32_t InWidth, uint32_t InHeight);
        static TRef<FTexture2D> Create(const std::string& InPath);
    };

} // namespace Leon
