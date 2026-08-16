#pragma once

#include "Core/Base.hpp"
#include "Core/FTimestep.hpp"
#include "Core/events/FEvent.hpp"

#include <string>

namespace Leon {

    class FLayer {
    public:
        FLayer(const std::string& InName = "Layer") : DebugName(InName) {}
        virtual ~FLayer() = default;

        virtual void OnAttach() {}
        virtual void OnDetach() {}
        virtual void OnUpdate(FTimestep InTs) {}
        virtual void OnEvent(FEvent& InEvent) {}

        const std::string& GetName() const { return DebugName; }

    protected:
        std::string DebugName;
    };

} // namespace Leon
