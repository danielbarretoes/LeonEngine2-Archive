#include "UMG/UTableView.hpp"
#include "UMG/FUIRenderer.hpp"

#include <algorithm>
#include <numeric>

namespace Leon {

    namespace {
        constexpr float kDefaultLineHeightScale = 56.0f;

        float ResolveLineHeight(float InFontScale) {
            const glm::vec2 measured = FUIRenderer::MeasureString("X", InFontScale);
            return measured.y > 0.0f ? measured.y : kDefaultLineHeightScale * InFontScale;
        }
    } // namespace

    UTableView::UTableView(const std::string& InName) : UWidget(InName) {
        Size = {640.0f, 320.0f};
    }

    void UTableView::SetColumns(const std::vector<FTableColumn>& InColumns) {
        Columns = InColumns;
    }

    void UTableView::SetRows(const std::vector<FTableRow>& InRows) {
        Rows = InRows;
    }

    void UTableView::ClearRows() {
        Rows.clear();
    }

    std::vector<float> UTableView::ComputeColumnWidths(float InContentWidth) const {
        std::vector<float> widths(Columns.size(), 0.0f);
        if (Columns.empty() || InContentWidth <= 0.0f)
            return widths;

        const float totalWeight =
            std::accumulate(Columns.begin(), Columns.end(), 0.0f, [](float InSum, const FTableColumn& InColumn) {
                return InSum + std::max(InColumn.Weight, 0.01f);
            });
        float x = 0.0f;
        for (size_t i = 0; i < Columns.size(); ++i) {
            const float weight = std::max(Columns[i].Weight, 0.01f);
            const float w = (i + 1 == Columns.size()) ? (InContentWidth - x) : (InContentWidth * weight / totalWeight);
            widths[i] = w;
            x += w;
        }
        return widths;
    }

    float UTableView::RowPaintHeight(const FTableRow& InRow) const {
        switch (InRow.Kind) {
        case ETableRowKind::SectionHeader:
            return SectionHeight > 0.0f ? SectionHeight : ResolveLineHeight(HeaderFontScale) + 4.0f;
        case ETableRowKind::Spacer:
            return ResolveLineHeight(FontScale) * 0.35f;
        case ETableRowKind::Data:
        default:
            return RowHeight > 0.0f ? RowHeight : ResolveLineHeight(FontScale) + 2.0f;
        }
    }

    void UTableView::Paint(const FGeometry& InAllottedGeometry) {
        UWidget::Paint(InAllottedGeometry);
        if (!IsVisible() || Columns.empty())
            return;

        const glm::vec2 p0 = InAllottedGeometry.AbsolutePosition;
        const glm::vec2 p1 = p0 + InAllottedGeometry.Size;
        FUIRenderer::DrawBorderQuad(p0, p1, BackgroundColor, BorderColor, 1.0f);

        const float headerH = HeaderHeight > 0.0f ? HeaderHeight : ResolveLineHeight(HeaderFontScale) + 6.0f;
        const glm::vec2 headerMin = p0;
        const glm::vec2 headerMax{p1.x, p0.y + headerH};
        FUIRenderer::DrawQuad(headerMin, headerMax, HeaderBackgroundColor);

        const float contentPad = 1.0f;
        const float contentWidth = InAllottedGeometry.Size.x - contentPad * 2.0f;
        const std::vector<float> colWidths = ComputeColumnWidths(contentWidth);
        float colX = p0.x + contentPad;
        for (size_t i = 0; i < Columns.size(); ++i) {
            const float colW = colWidths[i];
            float textX = colX + CellPaddingX;
            ETextAlignment align = Columns[i].Alignment;
            if (align == ETextAlignment::Center)
                textX = colX + colW * 0.5f;
            else if (align == ETextAlignment::Right)
                textX = colX + colW - CellPaddingX;

            const float textY = p0.y + (headerH - ResolveLineHeight(HeaderFontScale)) * 0.5f;
            FUIRenderer::DrawString(textX, textY, Columns[i].Header, HeaderTextColor, HeaderFontScale, align);
            colX += colW;
        }

        FUIRenderer::DrawQuad({p0.x, headerMax.y}, {p1.x, headerMax.y + 1.0f}, BorderColor);

        float rowY = headerMax.y;
        int dataRowIndex = 0;
        for (const FTableRow& row : Rows) {
            const float rowH = RowPaintHeight(row);
            if (rowY + rowH > p1.y)
                break;

            if (row.Kind == ETableRowKind::Spacer) {
                rowY += rowH;
                continue;
            }

            if (row.Kind == ETableRowKind::SectionHeader) {
                const glm::vec4 sectionBg = row.SectionColor.a > 0.0f
                                                ? glm::vec4(row.SectionColor.r * 0.18f, row.SectionColor.g * 0.18f,
                                                            row.SectionColor.b * 0.18f, 0.92f)
                                                : SectionBackgroundColor;
                const glm::vec4 sectionText = row.SectionColor.a > 0.0f ? row.SectionColor : SectionTextColor;
                const glm::vec2 sectionMin{p0.x + 1.0f, rowY};
                const glm::vec2 sectionMax{p1.x - 1.0f, rowY + rowH};
                FUIRenderer::DrawQuad(sectionMin, sectionMax, sectionBg);
                const float textY = rowY + (rowH - ResolveLineHeight(HeaderFontScale)) * 0.5f;
                FUIRenderer::DrawString(p0.x + CellPaddingX + 2.0f, textY, row.SectionLabel, sectionText,
                                        HeaderFontScale, ETextAlignment::Left);
                rowY += rowH;
                continue;
            }

            const glm::vec4 rowBg = row.bHighlighted
                                        ? HighlightBackgroundColor
                                        : ((dataRowIndex % 2 == 0) ? RowBackgroundColor : AlternateRowBackgroundColor);
            FUIRenderer::DrawQuad({p0.x + 1.0f, rowY}, {p1.x - 1.0f, rowY + rowH}, rowBg);

            colX = p0.x + contentPad;
            for (size_t i = 0; i < Columns.size(); ++i) {
                const float colW = colWidths[i];
                const std::string cellText = i < row.Cells.size() ? row.Cells[i] : std::string();
                float textX = colX + CellPaddingX;
                ETextAlignment align = Columns[i].Alignment;
                if (align == ETextAlignment::Center)
                    textX = colX + colW * 0.5f;
                else if (align == ETextAlignment::Right)
                    textX = colX + colW - CellPaddingX;

                const float textY = rowY + (rowH - ResolveLineHeight(FontScale)) * 0.5f;
                FUIRenderer::DrawString(textX, textY, cellText, RowTextColor, FontScale, align);
                colX += colW;
            }

            ++dataRowIndex;
            rowY += rowH;
        }
    }

} // namespace Leon
