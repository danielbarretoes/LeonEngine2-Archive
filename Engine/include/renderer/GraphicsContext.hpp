#pragma once

#include "core/Base.hpp"

namespace Leon {

    class IGraphicsContext {
    public:
        virtual ~IGraphicsContext() = default;

        virtual void Init() = 0;
        virtual void SwapBuffers() = 0;

        static TScope<IGraphicsContext> Create(void* InWindowHandle);
    };

    using GraphicsContext = IGraphicsContext;

} // namespace Leon
