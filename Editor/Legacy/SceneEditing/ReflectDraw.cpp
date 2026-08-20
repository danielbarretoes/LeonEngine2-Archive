#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <leon/editor/EditorDisplayNames.h>
#include <leon/editor/EditorContext.h>
#include <leon/editor/EditorHistory.h>
#include <leon/editor/ReflectDraw.h>
#include <leon/editor/ui/UiKit.h>
#include <leon/reflect/PropertyHandle.h>
#include <unordered_map>
#include <vector>

namespace leon::editor {
namespace {

void CaptureIfNeeded(EditorContext& ctx, ReflectDrawCallbacks& callbacks, bool& captured) {
    if (captured) {
        return;
    }
    if (callbacks.onBeginEdit) {
        callbacks.onBeginEdit();
    } else if (ctx.history != nullptr) {
        ctx.history->Capture(ctx);
    }
    captured = true;
}

void NotifyChanged(EditorContext& ctx, ReflectDrawCallbacks& callbacks) {
    if (callbacks.onChanged) {
        callbacks.onChanged();
    } else {
        ctx.MarkDirty();
    }
}

[[nodiscard]] bool DrawPropertyRow(void* obj, const PropertyInfo& prop, EditorContext& ctx,
                                   ReflectDrawCallbacks& callbacks) {
    if (!HasFlag(prop.flags, EPropertyFlags::EditAnywhere) &&
        !HasFlag(prop.flags, EPropertyFlags::VisibleAnywhere)) {
        return false;
    }
    const bool readOnly = !HasFlag(prop.flags, EPropertyFlags::EditAnywhere);
    bool changed = false;
    bool captured = false;

    if (readOnly) {
        ImGui::BeginDisabled();
    }

    const char* fieldId = prop.name != nullptr ? prop.name : "##prop";
    const char* fieldLabel = PropertyDisplayLabel(prop.name);

    switch (prop.type) {
    case EPropertyType::Bool: {
        bool value = false;
        if (!PropertyHandle::GetBool(obj, prop, value)) {
            break;
        }
        if (ui::Checkbox(fieldId, fieldLabel, &value, ui::EUiSize::Sm)) {
            CaptureIfNeeded(ctx, callbacks, captured);
            (void)PropertyHandle::SetBool(obj, prop, value);
            changed = true;
        }
        break;
    }
    case EPropertyType::Int: {
        int value = 0;
        if (!PropertyHandle::GetInt(obj, prop, value)) {
            break;
        }
        if (ui::DragInt(fieldId, fieldLabel, &value, 1.0f)) {
            if (ImGui::IsItemActivated()) {
                CaptureIfNeeded(ctx, callbacks, captured);
            }
            (void)PropertyHandle::SetInt(obj, prop, value);
            changed = true;
        }
        break;
    }
    case EPropertyType::Float: {
        float value = 0.0f;
        if (!PropertyHandle::GetFloat(obj, prop, value)) {
            break;
        }
        if (ui::DragFloat(fieldId, fieldLabel, &value, 0.05f)) {
            if (ImGui::IsItemActivated()) {
                CaptureIfNeeded(ctx, callbacks, captured);
            }
            (void)PropertyHandle::SetFloat(obj, prop, value);
            changed = true;
        }
        break;
    }
    case EPropertyType::Vec3: {
        glm::vec3 value{};
        if (!PropertyHandle::GetVec3(obj, prop, value)) {
            break;
        }
        const bool isColor =
            prop.name != nullptr &&
            (std::strstr(prop.name, "Color") != nullptr || std::strstr(prop.name, "color") != nullptr);
        bool edited = false;
        if (isColor) {
            edited = ui::ColorEdit3(fieldId, fieldLabel, &value.x);
        } else {
            edited = ui::DragFloat3(fieldId, fieldLabel, &value.x, 0.05f);
        }
        if (edited) {
            if (ImGui::IsItemActivated()) {
                CaptureIfNeeded(ctx, callbacks, captured);
            }
            (void)PropertyHandle::SetVec3(obj, prop, value);
            changed = true;
        }
        break;
    }
    case EPropertyType::String:
    case EPropertyType::AssetPath: {
        if (prop.type == EPropertyType::AssetPath && callbacks.drawAssetPath &&
            callbacks.drawAssetPath(prop, obj)) {
            changed = true;
            break;
        }
        std::string value;
        if (!PropertyHandle::GetString(obj, prop, value)) {
            break;
        }
        char buf[256];
        (void)std::snprintf(buf, sizeof(buf), "%s", value.c_str());
        if (ui::InputText(fieldId, fieldLabel, buf, sizeof(buf))) {
            CaptureIfNeeded(ctx, callbacks, captured);
            (void)PropertyHandle::SetString(obj, prop, buf);
            changed = true;
        }
        if (ImGui::IsItemActivated()) {
            CaptureIfNeeded(ctx, callbacks, captured);
        }
        break;
    }
    case EPropertyType::Enum: {
        int value = 0;
        if (!PropertyHandle::GetEnum(obj, prop, value)) {
            break;
        }
        if (prop.enumNames != nullptr && prop.enumCount > 0) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(fieldLabel);
            ImGui::SameLine();
            if (ui::SelectFromList(fieldId, &value, prop.enumNames, prop.enumCount,
                                   ui::EUiSize::Sm, 160.0f)) {
                CaptureIfNeeded(ctx, callbacks, captured);
                (void)PropertyHandle::SetEnum(obj, prop, value);
                changed = true;
            }
        } else if (ui::DragInt(fieldId, fieldLabel, &value, 1.0f)) {
            CaptureIfNeeded(ctx, callbacks, captured);
            (void)PropertyHandle::SetEnum(obj, prop, value);
            changed = true;
        }
        break;
    }
    }

    if (readOnly) {
        ImGui::EndDisabled();
    }
    if (changed) {
        NotifyChanged(ctx, callbacks);
    }
    return changed;
}

} // namespace

bool DrawReflectedObject(void* obj, const TypeInfo& typeInfo, EditorContext& ctx,
                         ReflectDrawCallbacks callbacks) {
    if (obj == nullptr || typeInfo.properties.empty()) {
        return false;
    }

    std::vector<std::string> categoryOrder;
    std::unordered_map<std::string, std::vector<const PropertyInfo*>> byCategory;
    for (const PropertyInfo& prop : typeInfo.properties) {
        if (prop.name == nullptr) {
            continue;
        }
        if (callbacks.skipProperties.count(prop.name) > 0) {
            continue;
        }
        const char* cat = prop.category != nullptr ? prop.category : "Default";
        if (byCategory.find(cat) == byCategory.end()) {
            categoryOrder.emplace_back(cat);
        }
        byCategory[cat].push_back(&prop);
    }

    bool anyChanged = false;
    for (const std::string& category : categoryOrder) {
        if (!ui::CollapsingSection(CategoryDisplayLabel(category.c_str()))) {
            continue;
        }
        for (const PropertyInfo* prop : byCategory[category]) {
            anyChanged |= DrawPropertyRow(obj, *prop, ctx, callbacks);
        }
    }
    return anyChanged;
}

} // namespace leon::editor
