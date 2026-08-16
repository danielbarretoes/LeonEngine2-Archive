#include "Core/FLayerStack.hpp"
#include <algorithm>

namespace Leon {

    FLayerStack::~FLayerStack() {
        for (FLayer* layer : Layers) {
            layer->OnDetach();
            delete layer;
        }
    }

    void FLayerStack::PushLayer(FLayer* InLayer) {
        Layers.emplace(Layers.begin() + LayerInsertIndex, InLayer);
        LayerInsertIndex++;
        InLayer->OnAttach();
    }

    void FLayerStack::PushOverlay(FLayer* InOverlay) {
        Layers.emplace_back(InOverlay);
        InOverlay->OnAttach();
    }

    void FLayerStack::PopLayer(FLayer* InLayer) {
        auto it = std::find(Layers.begin(), Layers.begin() + LayerInsertIndex, InLayer);
        if (it != Layers.begin() + LayerInsertIndex) {
            InLayer->OnDetach();
            Layers.erase(it);
            LayerInsertIndex--;
        }
    }

    void FLayerStack::PopOverlay(FLayer* InOverlay) {
        auto it = std::find(Layers.begin() + LayerInsertIndex, Layers.end(), InOverlay);
        if (it != Layers.end()) {
            InOverlay->OnDetach();
            Layers.erase(it);
        }
    }

} // namespace Leon
