#pragma once

#include "engine/core/Base.hpp"
#include "engine/core/Layer.hpp"

#include <vector>

namespace Leon {

    class FLayerStack {
    public:
        FLayerStack() = default;
        ~FLayerStack();

        void PushLayer(FLayer* InLayer);
        void PushOverlay(FLayer* InOverlay);
        void PopLayer(FLayer* InLayer);
        void PopOverlay(FLayer* InOverlay);

        std::vector<FLayer*>::iterator begin() { return m_Layers.begin(); }
        std::vector<FLayer*>::iterator end() { return m_Layers.end(); }
        std::vector<FLayer*>::reverse_iterator rbegin() { return m_Layers.rbegin(); }
        std::vector<FLayer*>::reverse_iterator rend() { return m_Layers.rend(); }

        std::vector<FLayer*>::const_iterator begin() const { return m_Layers.begin(); }
        std::vector<FLayer*>::const_iterator end() const { return m_Layers.end(); }
        std::vector<FLayer*>::const_reverse_iterator rbegin() const { return m_Layers.rbegin(); }
        std::vector<FLayer*>::const_reverse_iterator rend() const { return m_Layers.rend(); }

    private:
        std::vector<FLayer*> m_Layers;
        unsigned int m_LayerInsertIndex = 0;
    };

    using LayerStack = FLayerStack;

} // namespace Leon
