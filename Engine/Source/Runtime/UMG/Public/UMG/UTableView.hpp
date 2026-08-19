#pragma once

#include "Engine/Components.hpp"
#include "UMG/UWidget.hpp"

#include <string>
#include <vector>

namespace Leon {

    struct FTableColumn {
        std::string Header;
        float Weight = 1.0f;
        ETextAlignment Alignment = ETextAlignment::Left;
    };

    enum class ETableRowKind : uint8_t { Data = 0, SectionHeader = 1, Spacer = 2 };

    struct FTableRow {
        ETableRowKind Kind = ETableRowKind::Data;
        std::vector<std::string> Cells;
        std::string SectionLabel;
        bool bHighlighted = false;
    };

    /**
     * @brief Columnar table widget for scoreboards, lists, and data panels.
     */
    class UTableView : public UWidget {
    public:
        UTableView(const std::string& InName = "TableView");

        void SetColumns(const std::vector<FTableColumn>& InColumns);
        const std::vector<FTableColumn>& GetColumns() const { return Columns; }

        void SetRows(const std::vector<FTableRow>& InRows);
        void ClearRows();
        const std::vector<FTableRow>& GetRows() const { return Rows; }

        void SetFontScale(float InScale) { FontScale = InScale; }
        float GetFontScale() const { return FontScale; }
        void SetHeaderFontScale(float InScale) { HeaderFontScale = InScale; }
        float GetHeaderFontScale() const { return HeaderFontScale; }

        void SetRowHeight(float InHeight) { RowHeight = InHeight; }
        void SetHeaderHeight(float InHeight) { HeaderHeight = InHeight; }
        void SetSectionHeight(float InHeight) { SectionHeight = InHeight; }
        void SetCellPaddingX(float InPadding) { CellPaddingX = InPadding; }

        void Paint(const FGeometry& InAllottedGeometry) override;

    private:
        std::vector<float> ComputeColumnWidths(float InContentWidth) const;
        float RowPaintHeight(const FTableRow& InRow) const;

        std::vector<FTableColumn> Columns;
        std::vector<FTableRow> Rows;

        float FontScale = 1.0f;
        float HeaderFontScale = 1.0f;
        float RowHeight = 0.0f;
        float HeaderHeight = 0.0f;
        float SectionHeight = 0.0f;
        float CellPaddingX = 8.0f;

        glm::vec4 BackgroundColor{0.04f, 0.05f, 0.09f, 0.55f};
        glm::vec4 HeaderBackgroundColor{0.08f, 0.10f, 0.16f, 0.95f};
        glm::vec4 RowBackgroundColor{0.05f, 0.06f, 0.10f, 0.70f};
        glm::vec4 AlternateRowBackgroundColor{0.06f, 0.07f, 0.11f, 0.70f};
        glm::vec4 HighlightBackgroundColor{0.12f, 0.18f, 0.28f, 0.90f};
        glm::vec4 SectionBackgroundColor{0.10f, 0.14f, 0.22f, 0.95f};
        glm::vec4 BorderColor{0.20f, 0.28f, 0.38f, 0.85f};
        glm::vec4 HeaderTextColor{0.65f, 0.74f, 0.90f, 0.95f};
        glm::vec4 RowTextColor{0.94f, 0.95f, 0.98f, 1.0f};
        glm::vec4 SectionTextColor{0.82f, 0.88f, 0.98f, 1.0f};
    };

} // namespace Leon
