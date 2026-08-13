#pragma once

#include "engine/core/Base.hpp"
#include "engine/core/Timestep.hpp"
#include "engine/core/events/Event.hpp"

#include <string>

namespace Leon {

    class FLayer {
    public:
        FLayer(const std::string& InName = "Layer") : m_DebugName(InName) {}
        virtual ~FLayer() = default;

        virtual void OnAttach() {}
        virtual void OnDetach() {}
        virtual void OnUpdate(FTimestep InTs) {}
        virtual void OnEvent(FEvent& InEvent) {}

        const std::string& GetName() const { return m_DebugName; }

    protected:
        std::string m_DebugName;
    };

    using Layer = FLayer;

} // namespace Leon
