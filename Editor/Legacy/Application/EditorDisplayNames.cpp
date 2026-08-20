#include <cctype>
#include <cstdio>
#include <cstring>
#include <utility>
#include <span>
#include <leon/editor/EditorDisplayNames.h>

namespace leon::editor {
namespace {

[[nodiscard]] const char* LookupMappedLabel(const char* id,
                                            std::span<const std::pair<const char*, const char*>> table) {
    if (id == nullptr || id[0] == '\0') {
        return "";
    }
    for (const auto& [key, label] : table) {
        if (std::strcmp(id, key) == 0) {
            return label;
        }
    }
    return nullptr;
}

[[nodiscard]] const char* LookupClassLabel(const char* classId) {
    if (classId == nullptr || classId[0] == '\0') {
        return "Actor";
    }
    struct Entry {
        const char* id;
        const char* label;
    };
    static constexpr Entry kEntries[] = {
        {"StaticMesh", "Static mesh"},
        {"BlockingVolume", "Blocking volume"},
        {"PointLight", "Point light"},
        {"SpotLight", "Spot light"},
        {"DirectionalLight", "Directional light"},
        {"TriggerVolume", "Trigger volume"},
        {"PainCausingVolume", "Pain volume"},
        {"PlayerStart", "Player start"},
        {"AISpawnPoint", "AI spawn point"},
        {"TextRenderActor", "Text render"},
        {"Cube", "Cube"},
        {"Sphere", "Sphere"},
        {"Plane", "Plane"},
    };
    for (const Entry& entry : kEntries) {
        if (std::strcmp(classId, entry.id) == 0) {
            return entry.label;
        }
    }
    return classId;
}

[[nodiscard]] const char* LookupPropertyLabel(const char* propertyId) {
    static constexpr std::pair<const char*, const char*> kEntries[] = {
        {"tag", "Label"},
        {"materialPath", "Material"},
        {"mobility", "Mobility"},
        {"lightmapResolution", "Lightmap resolution"},
        {"collisionEnabled", "Collision enabled"},
        {"simulatePhysics", "Simulate physics"},
        {"enableGravity", "Enable gravity"},
        {"hidden", "Hidden"},
        {"castShadows", "Cast shadows"},
        {"receiveShadows", "Receive shadows"},
        {"spinYaw", "Spin (yaw)"},
        {"lightColor", "Color"},
        {"intensity", "Intensity"},
        {"range", "Range"},
        {"innerConeAngle", "Inner cone angle"},
        {"outerConeAngle", "Outer cone angle"},
        {"sourceAngle", "Source angle"},
        {"interactCost", "Interact cost"},
        {"interactRadius", "Interact radius"},
        {"payload", "Payload"},
        {"bConsumeOnUse", "Consume on use"},
        {"damagePerSecond", "Damage per second"},
        {"damageInterval", "Damage interval"},
        {"Text", "Text"},
        {"TextRenderColor", "Color"},
        {"WorldSize", "World size"},
        {"HorizontalAlignment", "Horizontal alignment"},
        {"bHiddenInGame", "Hidden in game"},
    };
    if (const char* mapped = LookupMappedLabel(propertyId, kEntries)) {
        return mapped;
    }
    return propertyId;
}

[[nodiscard]] const char* LookupCategoryLabel(const char* categoryId) {
    static constexpr std::pair<const char*, const char*> kEntries[] = {
        {"Default", "General"},
        {"Static Mesh", "Static mesh"},
        {"Lightmass", "Baked lighting"},
        {"TextRender", "Text render"},
        {"StaticMeshComponent", "Static mesh"},
        {"PointLight", "Point light"},
        {"SpotLight", "Spot light"},
        {"DirectionalLight", "Directional light"},
        {"TriggerVolume", "Trigger volume"},
        {"PainCausingVolume", "Pain volume"},
        {"AISpawnPoint", "AI spawn point"},
        {"TextRenderActor", "Text render"},
    };
    if (const char* mapped = LookupMappedLabel(categoryId, kEntries)) {
        return mapped;
    }
    return categoryId;
}

void HumanizeIdentifierFallback(const char* id, char* out, std::size_t outSize) {
    if (out == nullptr || outSize == 0) {
        return;
    }
    out[0] = '\0';
    if (id == nullptr || id[0] == '\0') {
        return;
    }

    const char* src = id;
    if (src[0] == 'b' && src[1] >= 'A' && src[1] <= 'Z') {
        ++src;
    }

    std::size_t outLen = 0;
    bool wordStart = true;
    for (; *src != '\0' && outLen + 1 < outSize; ++src) {
        const char ch = *src;
        if (ch >= 'A' && ch <= 'Z') {
            if (!wordStart) {
                out[outLen++] = ' ';
            }
            out[outLen++] = ch;
            wordStart = false;
            continue;
        }
        if (ch >= 'a' && ch <= 'z') {
            out[outLen++] = wordStart ? ch : static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            wordStart = false;
            continue;
        }
        if (ch >= '0' && ch <= '9') {
            if (!wordStart) {
                out[outLen++] = ' ';
            }
            out[outLen++] = ch;
            wordStart = false;
        }
    }
    out[outLen] = '\0';
    if (outLen == 0) {
        (void)std::snprintf(out, outSize, "%s", id);
    }
}

[[nodiscard]] char* ThreadLocalDisplayBuffer() {
    thread_local char buffer[96];
    return buffer;
}

[[nodiscard]] const char* StableDisplayLabel(const char* id,
                                              const char* (*lookup)(const char*)) {
    if (id == nullptr) {
        return "";
    }
    const char* mapped = lookup(id);
    if (mapped != id) {
        return mapped;
    }
    char* buffer = ThreadLocalDisplayBuffer();
    HumanizeIdentifierFallback(id, buffer, 96);
    return buffer;
}

} // namespace

const char* PlaceActorsKindLabel(EEditorPlaceActorsKind kind) {
    switch (kind) {
    case EEditorPlaceActorsKind::Cube:
        return "Cube";
    case EEditorPlaceActorsKind::Sphere:
        return "Sphere";
    case EEditorPlaceActorsKind::Plane:
        return "Plane";
    case EEditorPlaceActorsKind::Cylinder:
        return "Cylinder";
    case EEditorPlaceActorsKind::BlockingVolume:
        return "Blocking volume";
    case EEditorPlaceActorsKind::PointLight:
        return "Point light";
    case EEditorPlaceActorsKind::SpotLight:
        return "Spot light";
    case EEditorPlaceActorsKind::DirectionalLight:
        return "Directional light";
    case EEditorPlaceActorsKind::TriggerVolume:
        return "Trigger volume";
    case EEditorPlaceActorsKind::PainCausingVolume:
        return "Pain volume";
    case EEditorPlaceActorsKind::PlayerStart:
        return "Player start";
    case EEditorPlaceActorsKind::AISpawnPoint:
        return "AI spawn point";
    case EEditorPlaceActorsKind::TextRenderActor:
        return "Text render";
    case EEditorPlaceActorsKind::BlueprintClass:
        return "Blueprint class";
    case EEditorPlaceActorsKind::None:
    default:
        return "None";
    }
}

const char* HumanizeClassName(const char* classId) {
    return LookupClassLabel(classId);
}

void FormatIndexedActorLabel(const char* typeName, std::size_t index, char* buf, std::size_t bufSize) {
    if (buf == nullptr || bufSize == 0) {
        return;
    }
    (void)std::snprintf(buf, bufSize, "%s %zu", LookupClassLabel(typeName), index);
}

const char* TemplateDisplayName(const char* templateId) {
    if (templateId == nullptr || templateId[0] == '\0') {
        return "Template";
    }
    if (std::strcmp(templateId, "Blank") == 0) {
        return "Blank";
    }
    if (std::strcmp(templateId, "ThirdPerson") == 0) {
        return "Third person";
    }
    return templateId;
}

const char* MaterialExpressionTypeLabel(EMaterialExpressionType type) {
    switch (type) {
    case EMaterialExpressionType::Constant3Vector:
        return "Constant (RGB)";
    case EMaterialExpressionType::TextureSample:
        return "Texture sample";
    case EMaterialExpressionType::Multiply:
        return "Multiply";
    case EMaterialExpressionType::Lerp:
        return "Lerp";
    case EMaterialExpressionType::TextureCoordinate:
        return "Texture coordinate";
    case EMaterialExpressionType::ScalarParameter:
        return "Scalar parameter";
    case EMaterialExpressionType::VectorParameter:
        return "Vector parameter";
    }
    return "Expression";
}

const char* MaterialPinLabel(const char* pinId) {
    if (pinId == nullptr) {
        return "";
    }
    if (std::strcmp(pinId, "BaseColor") == 0) {
        return "Base color";
    }
    if (std::strcmp(pinId, "OpacityMask") == 0) {
        return "Opacity mask";
    }
    if (std::strcmp(pinId, "ORM") == 0) {
        return "ORM pack";
    }
    return pinId;
}

const char* HumanizePropertyName(const char* propertyId) {
    return LookupPropertyLabel(propertyId);
}

const char* HumanizeCategoryName(const char* categoryId) {
    return LookupCategoryLabel(categoryId);
}

const char* HumanizeReflectTypeName(const char* typeName) {
    if (typeName == nullptr || typeName[0] == '\0') {
        return "Object";
    }
    static constexpr std::pair<const char*, const char*> kEntries[] = {
        {"StaticMeshComponent", "Static mesh"},
        {"PointLight", "Point light"},
        {"SpotLight", "Spot light"},
        {"DirectionalLight", "Directional light"},
        {"TriggerVolume", "Trigger volume"},
        {"PainCausingVolume", "Pain volume"},
        {"AISpawnPoint", "AI spawn point"},
        {"TextRenderActor", "Text render"},
    };
    if (const char* mapped = LookupMappedLabel(typeName, kEntries)) {
        return mapped;
    }
    return HumanizeClassName(typeName);
}

const char* PropertyDisplayLabel(const char* propertyId) {
    return StableDisplayLabel(propertyId, LookupPropertyLabel);
}

const char* CategoryDisplayLabel(const char* categoryId) {
    return StableDisplayLabel(categoryId, LookupCategoryLabel);
}

} // namespace leon::editor
