#pragma once

#include <cstddef>
#include <leon/editor/EditorContext.h>
#include <leon/render/LeonMaterialGraph.h>

namespace leon::editor {

/// User-facing label for Place Actors / Modes classes.
[[nodiscard]] const char* PlaceActorsKindLabel(EEditorPlaceActorsKind kind);

/// Humanize known editor class identifiers (StaticMesh → Static mesh). Returns `classId` if unknown.
[[nodiscard]] const char* HumanizeClassName(const char* classId);

/// e.g. PlayerStart + 0 → "Player start 0"
void FormatIndexedActorLabel(const char* typeName, std::size_t index, char* buf, std::size_t bufSize);

/// New Project template id → display name (ThirdPerson → Third person).
[[nodiscard]] const char* TemplateDisplayName(const char* templateId);

[[nodiscard]] const char* MaterialExpressionTypeLabel(EMaterialExpressionType type);

/// Material output pin id (BaseColor → Base color).
[[nodiscard]] const char* MaterialPinLabel(const char* pinId);

/// Reflected Details property id → label (castShadows → Cast shadows).
[[nodiscard]] const char* HumanizePropertyName(const char* propertyId);

/// Reflected Details category → section title (Lightmass → Baked lighting).
[[nodiscard]] const char* HumanizeCategoryName(const char* categoryId);

/// TypeInfo registry name → panel title (StaticMeshComponent → Static mesh).
[[nodiscard]] const char* HumanizeReflectTypeName(const char* typeName);

/// Stable display string for a property (uses thread-local buffer for fallback splits).
[[nodiscard]] const char* PropertyDisplayLabel(const char* propertyId);

/// Stable display string for a reflected category.
[[nodiscard]] const char* CategoryDisplayLabel(const char* categoryId);

} // namespace leon::editor
