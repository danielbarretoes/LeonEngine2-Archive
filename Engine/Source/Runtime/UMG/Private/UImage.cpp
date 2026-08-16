#include "UMG/UImage.hpp"
#include "Assets/UAssetManager.hpp"
#include "UMG/FUIRenderer.hpp"

namespace Leon {

    UImage::UImage(const std::string& InName) : UWidget(InName) {}

    void UImage::SetBrushFromPath(const std::string& InVirtualOrFilePath) {
        TexturePath = InVirtualOrFilePath;
        Texture = UAssetManager::GetTexture2D(InVirtualOrFilePath);
    }

    void UImage::Paint(const FGeometry& InAllottedGeometry) {
        UWidget::Paint(InAllottedGeometry);
        if (!IsVisible())
            return;

        glm::vec2 p0 = InAllottedGeometry.AbsolutePosition;
        glm::vec2 p1 = p0 + InAllottedGeometry.Size;

        if (Texture) {
            FUIRenderer::DrawTexturedQuad(p0, p1, Texture, Tint);
        } else {
            FUIRenderer::DrawQuad(p0, p1, Tint);
        }
    }

} // namespace Leon
