#pragma once

#include <functional>
#include <leon/reflect/TypeInfo.h>
#include <string>
#include <unordered_set>

namespace leon::editor {

struct EditorContext;

/// Callbacks while editing a reflected object in Details.
struct ReflectDrawCallbacks {
    /// Capture undo before the first edit of an interaction.
    std::function<void()> onBeginEdit;
    /// Mark level/asset dirty after a value change.
    std::function<void()> onChanged;
    /// Optional: custom drawer for AssetPath (return true if handled).
    std::function<bool(const PropertyInfo& prop, void* obj)> drawAssetPath;
    /// Property names to skip (e.g. materialPath when using Material picker).
    std::unordered_set<std::string> skipProperties;
};

/// Draw all editable properties grouped by category. Transform stays separate.
/// Returns true if any property changed this frame.
[[nodiscard]] bool DrawReflectedObject(void* obj, const TypeInfo& typeInfo, EditorContext& ctx,
                                       ReflectDrawCallbacks callbacks = {});

template <typename T>
[[nodiscard]] bool DrawReflectedObject(T& obj, EditorContext& ctx,
                                       ReflectDrawCallbacks callbacks = {}) {
    return DrawReflectedObject(static_cast<void*>(&obj), GetTypeInfo<T>(), ctx,
                               std::move(callbacks));
}

} // namespace leon::editor
