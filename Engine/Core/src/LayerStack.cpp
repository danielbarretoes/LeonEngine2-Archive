#include "engine/core/LayerStack.hpp"
#include <algorithm>

namespace Leon {

    FLayerStack::~FLayerStack() {
        for (FLayer* layer : m_Layers) {
            layer->OnDetach();
            delete layer;
        }
    }

    void FLayerStack::PushLayer(FLayer* InLayer) {
        m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, InLayer);
        m_LayerInsertIndex++;
        InLayer->OnAttach();
    }

    void FLayerStack::PushOverlay(FLayer* InOverlay) {
        m_Layers.emplace_back(InOverlay);
        InOverlay->OnAttach();
    }

    void FLayerStack::PopLayer(FLayer* InLayer) {
        auto it = std::find(m_Layers.begin(), m_Layers.begin() + m_LayerInsertIndex, InLayer);
        if (it != m_Layers.begin() + m_LayerInsertIndex) {
            InLayer->OnDetach();
            m_Layers.erase(it);
            m_LayerInsertIndex--;
        }
    }

    void FLayerStack::PopOverlay(FLayer* InOverlay) {
        auto it = std::find(m_Layers.begin() + m_LayerInsertIndex, m_Layers.end(), InOverlay);
        if (it != m_Layers.end()) {
            InOverlay->OnDetach();
            m_Layers.erase(it);
        }
    }

} // namespace Leon
