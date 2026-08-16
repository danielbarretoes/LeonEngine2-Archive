#pragma once

#include "Core/Base.hpp"
#include "Core/FLayer.hpp"

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

        std::vector<FLayer*>::iterator begin() { return Layers.begin(); }
        std::vector<FLayer*>::iterator end() { return Layers.end(); }
        std::vector<FLayer*>::reverse_iterator rbegin() { return Layers.rbegin(); }
        std::vector<FLayer*>::reverse_iterator rend() { return Layers.rend(); }

        std::vector<FLayer*>::const_iterator begin() const { return Layers.begin(); }
        std::vector<FLayer*>::const_iterator end() const { return Layers.end(); }
        std::vector<FLayer*>::const_reverse_iterator rbegin() const { return Layers.rbegin(); }
        std::vector<FLayer*>::const_reverse_iterator rend() const { return Layers.rend(); }

    private:
        std::vector<FLayer*> Layers;
        unsigned int LayerInsertIndex = 0;
    };

} // namespace Leon
