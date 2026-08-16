#pragma once

#include "UMG/UWidget.hpp"
#include <memory>
#include <vector>

namespace Leon {

    /**
     * @brief Unreal Engine aligned container widget base class.
     */
    class UPanelWidget : public UWidget {
    public:
        UPanelWidget(const std::string& InName = "PanelWidget");
        ~UPanelWidget() override = default;

        virtual void AddChild(const TRef<UWidget>& InChild);
        virtual bool RemoveChild(const TRef<UWidget>& InChild);
        virtual void ClearChildren();

        const std::vector<TRef<UWidget>>& GetAllChildren() const { return Children; }
        size_t GetChildrenCount() const { return Children.size(); }

        void Paint(const FGeometry& InAllottedGeometry) override;
        void Tick(float InDeltaTime) override;

        bool OnMouseMove(const glm::vec2& InMousePos) override;
        bool OnMouseButtonDown(int InButton, const glm::vec2& InMousePos) override;
        bool OnMouseButtonUp(int InButton, const glm::vec2& InMousePos) override;

    protected:
        std::vector<TRef<UWidget>> Children;
    };

} // namespace Leon
