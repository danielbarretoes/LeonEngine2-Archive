#include <algorithm>
#include <cstdio>
#include <imgui.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorOutputLog.h>
#include <leon/editor/LucideIcons.h>
#include <leon/editor/panels/BlueprintEditorPanel.h>
#include <leon/editor/ui/UiKit.h>

namespace leon::editor {
namespace {

const char* kEventNames[] = {"Begin play", "On trigger overlap", "Custom"};
const char* kActionNames[] = {"Print",         "Play sound (2D)", "Set bool",     "Set float",
                              "Set vector",    "Set string",      "Call function", "Set actor hidden"};
const char* kComponentNames[] = {"Static mesh",     "Cube",            "Sphere",         "Plane",
                                 "Blocking volume", "Point light",     "Spot light",     "Directional light",
                                 "Trigger volume"};

} // namespace

BlueprintEditorPanel::Doc* BlueprintEditorPanel::FindDoc(const std::string& path) {
    for (Doc& doc : docs_) {
        if (PathsEqualNormalized(doc.path, path)) {
            return &doc;
        }
    }
    return nullptr;
}

void BlueprintEditorPanel::Open(const std::string& path) {
    const std::string norm = NormalizeAssetPathAbs(path);
    if (norm.empty()) {
        return;
    }
    if (Doc* existing = FindDoc(norm)) {
        existing->open = true;
        existing->focusTab = true;
        focusedPath_ = norm;
        return;
    }
    BlueprintDocument document;
    if (!LoadBlueprintDocument(norm, document)) {
        EditorLogError("Blueprint Editor: failed to load " + norm);
        return;
    }
    Doc doc;
    doc.path = norm;
    doc.document = std::move(document);
    doc.focusTab = true;
    docs_.push_back(std::move(doc));
    focusedPath_ = norm;
}

void BlueprintEditorPanel::CloseDoc(const std::string& path) {
    if (Doc* doc = FindDoc(path)) {
        doc->open = false;
        if (PathsEqualNormalized(focusedPath_, path)) {
            focusedPath_.clear();
        }
    }
}

void BlueprintEditorPanel::RemapAssetPath(EditorContext& /*ctx*/, const std::string& fromAbs,
                                          const std::string& toAbs) {
    if (fromAbs.empty()) {
        return;
    }
    namespace fs = std::filesystem;
    const fs::path fromNorm = fs::path(fromAbs).lexically_normal();
    for (Doc& doc : docs_) {
        if (!PathsEqualNormalized(doc.path, fromAbs) &&
            fs::path(doc.path).lexically_normal() != fromNorm) {
            continue;
        }
        if (toAbs.empty()) {
            doc.open = false;
            if (PathsEqualNormalized(focusedPath_, doc.path)) {
                focusedPath_.clear();
            }
        } else {
            doc.path = NormalizeAssetPathAbs(toAbs);
            if (PathsEqualNormalized(focusedPath_, fromAbs)) {
                focusedPath_ = doc.path;
            }
        }
    }
}

bool BlueprintEditorPanel::SavePath(EditorContext& ctx, const std::string& path) {
    Doc* doc = FindDoc(path);
    if (doc == nullptr || !doc->open) {
        return false;
    }
    if (!doc->dirty) {
        return true;
    }
    return SaveDoc(ctx, *doc);
}

bool BlueprintEditorPanel::HasDirtyDocs() const {
    for (const Doc& doc : docs_) {
        if (doc.dirty) {
            return true;
        }
    }
    return false;
}

bool BlueprintEditorPanel::HasFocusedDirtyDoc() const {
    if (focusedPath_.empty()) {
        return false;
    }
    for (const Doc& d : docs_) {
        if (PathsEqualNormalized(d.path, focusedPath_)) {
            return d.open && d.dirty;
        }
    }
    return false;
}

bool BlueprintEditorPanel::SaveDoc(EditorContext& /*ctx*/, Doc& doc) {
    doc.document.version = kLeonBlueprintDocumentVersion;
    if (!SaveBlueprintDocument(doc.path, doc.document)) {
        EditorLogError("Blueprint Editor: failed to save " + doc.path);
        return false;
    }
    doc.dirty = false;
    EditorLogInfo("Saved Blueprint " + doc.path);
    return true;
}

bool BlueprintEditorPanel::SaveFocused(EditorContext& ctx) {
    Doc* doc = FindDoc(focusedPath_);
    if (doc == nullptr || !doc->open) {
        return false;
    }
    if (!doc->dirty) {
        return true;
    }
    return SaveDoc(ctx, *doc);
}

bool BlueprintEditorPanel::SaveAll(EditorContext& ctx) {
    bool ok = true;
    for (Doc& doc : docs_) {
        if (doc.dirty) {
            ok = SaveDoc(ctx, doc) && ok;
        }
    }
    return ok;
}

void BlueprintEditorPanel::SyncDirtyPaths(EditorContext& ctx) const {
    ctx.dirtyBlueprintPaths.clear();
    for (const Doc& doc : docs_) {
        if (doc.open && doc.dirty) {
            ctx.dirtyBlueprintPaths.push_back(doc.path);
        }
    }
}

void BlueprintEditorPanel::DrawDoc(EditorContext& /*ctx*/, Doc& doc) {
    ImGui::TextWrapped("%s", doc.path.c_str());
    char nameBuf[128];
    (void)std::snprintf(nameBuf, sizeof(nameBuf), "%s", doc.document.name.c_str());
    if (ui::InputText("##bp_name", "Name", nameBuf, sizeof(nameBuf))) {
        doc.document.name = nameBuf;
        doc.dirty = true;
    }

    if (ImGui::BeginTabBar("##BpTabs")) {
        if (ImGui::BeginTabItem("Components")) {
            for (std::size_t i = 0; i < doc.document.components.size(); ++i) {
                BlueprintComponentDesc& c = doc.document.components[i];
                ImGui::PushID(static_cast<int>(i));
                int classIndex = static_cast<int>(c.componentClass);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Class");
                ImGui::SameLine();
                if (ui::SelectFromList("##comp_class", &classIndex, kComponentNames,
                                       IM_ARRAYSIZE(kComponentNames), ui::EUiSize::Sm, 180.0f)) {
                    c.componentClass = static_cast<EBlueprintComponentClass>(classIndex);
                    doc.dirty = true;
                }
                if (ui::DragFloat3("##rel_loc", "Relative Location", &c.relativeLocation.x,
                                   0.05f)) {
                    doc.dirty = true;
                }
                if (ui::DragFloat3("##rel_scale", "Relative Scale", &c.relativeScale.x, 0.05f)) {
                    doc.dirty = true;
                }
                if (c.componentClass == EBlueprintComponentClass::TriggerVolume) {
                    if (ui::DragFloat("##interact_r", "Interact Radius", &c.interactRadius, 0.05f,
                                      0.1f, 50.0f)) {
                        doc.dirty = true;
                    }
                    char payload[128];
                    (void)std::snprintf(payload, sizeof(payload), "%s", c.payload.c_str());
                    if (ui::InputText("##payload", "Payload", payload, sizeof(payload))) {
                        c.payload = payload;
                        doc.dirty = true;
                    }
                }
                if (ui::Button("##rm_comp", "Remove Component", ui::EUiVariant::Destructive,
                               ui::EUiSize::Sm)) {
                    doc.document.components.erase(doc.document.components.begin() +
                                                  static_cast<std::ptrdiff_t>(i));
                    doc.dirty = true;
                    ImGui::PopID();
                    break;
                }
                ui::Separator();
                ImGui::PopID();
            }
            if (ui::Button("##add_cube", "Add Cube Component", ui::EUiVariant::Secondary,
                           ui::EUiSize::Sm)) {
                BlueprintComponentDesc cube;
                cube.componentClass = EBlueprintComponentClass::Cube;
                doc.document.components.push_back(cube);
                doc.dirty = true;
            }
            ImGui::SameLine();
            if (ui::Button("##add_trigger", "Add trigger volume", ui::EUiVariant::Secondary,
                           ui::EUiSize::Sm)) {
                BlueprintComponentDesc trigger;
                trigger.componentClass = EBlueprintComponentClass::TriggerVolume;
                trigger.interactRadius = 2.0f;
                doc.document.components.push_back(trigger);
                doc.dirty = true;
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Variables")) {
            for (std::size_t i = 0; i < doc.document.variables.size(); ++i) {
                BlueprintVariableDesc& v = doc.document.variables[i];
                ImGui::PushID(static_cast<int>(i));
                char name[64];
                (void)std::snprintf(name, sizeof(name), "%s", v.name.c_str());
                if (ui::InputText("##var_name", "Name", name, sizeof(name))) {
                    v.name = name;
                    doc.dirty = true;
                }
                if (ui::Checkbox("##def_bool", "Default Bool", &v.defaultBool, ui::EUiSize::Sm)) {
                    doc.dirty = true;
                }
                if (ui::Checkbox("##inst_edit", "Instance editable", &v.instanceEditable,
                                 ui::EUiSize::Sm)) {
                    doc.dirty = true;
                }
                if (ui::Button("##rm_var", "Remove", ui::EUiVariant::Destructive,
                               ui::EUiSize::Sm)) {
                    doc.document.variables.erase(doc.document.variables.begin() +
                                                 static_cast<std::ptrdiff_t>(i));
                    doc.dirty = true;
                    ImGui::PopID();
                    break;
                }
                ui::Separator();
                ImGui::PopID();
            }
            if (ui::Button("##add_bool_var", "Add Bool Variable", ui::EUiVariant::Secondary,
                           ui::EUiSize::Sm)) {
                BlueprintVariableDesc v;
                v.name = "NewVariable";
                v.type = EPropertyType::Bool;
                doc.document.variables.push_back(v);
                doc.dirty = true;
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Events")) {
            for (std::size_t ei = 0; ei < doc.document.events.size(); ++ei) {
                BlueprintEventDesc& event = doc.document.events[ei];
                ImGui::PushID(static_cast<int>(ei));
                int eventType = static_cast<int>(event.type);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Event");
                ImGui::SameLine();
                if (ui::SelectFromList("##event_type", &eventType, kEventNames,
                                       IM_ARRAYSIZE(kEventNames), ui::EUiSize::Sm, 180.0f)) {
                    event.type = static_cast<EBlueprintEventType>(eventType);
                    doc.dirty = true;
                }
                for (std::size_t ai = 0; ai < event.actions.size(); ++ai) {
                    BlueprintActionDesc& action = event.actions[ai];
                    ImGui::PushID(static_cast<int>(ai));
                    int actionType = static_cast<int>(action.type);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("Action");
                    ImGui::SameLine();
                    if (ui::SelectFromList("##action_type", &actionType, kActionNames,
                                           IM_ARRAYSIZE(kActionNames), ui::EUiSize::Sm, 180.0f)) {
                        action.type = static_cast<EBlueprintActionType>(actionType);
                        doc.dirty = true;
                    }
                    char strArg[256];
                    (void)std::snprintf(strArg, sizeof(strArg), "%s", action.stringArg.c_str());
                    if (ui::InputText("##str_arg", "String value", strArg, sizeof(strArg))) {
                        action.stringArg = strArg;
                        doc.dirty = true;
                    }
                    char varName[64];
                    (void)std::snprintf(varName, sizeof(varName), "%s",
                                        action.variableName.c_str());
                    if (ui::InputText("##var_ref", "Variable", varName, sizeof(varName))) {
                        action.variableName = varName;
                        doc.dirty = true;
                    }
                    if (ui::Checkbox("##bool_arg", "Bool value", &action.boolArg, ui::EUiSize::Sm)) {
                        doc.dirty = true;
                    }
                    if (ui::DragFloat("##float_arg", "Float value", &action.floatArg, 0.05f)) {
                        doc.dirty = true;
                    }
                    if (ui::Button("##rm_action", "Remove Action", ui::EUiVariant::Destructive,
                                   ui::EUiSize::Sm)) {
                        event.actions.erase(event.actions.begin() +
                                            static_cast<std::ptrdiff_t>(ai));
                        doc.dirty = true;
                        ImGui::PopID();
                        break;
                    }
                    ui::Separator();
                    ImGui::PopID();
                }
                if (ui::Button("##add_print", "Add Print Action", ui::EUiVariant::Secondary,
                               ui::EUiSize::Sm)) {
                    BlueprintActionDesc a;
                    a.type = EBlueprintActionType::Print;
                    a.stringArg = "Hello";
                    event.actions.push_back(a);
                    doc.dirty = true;
                }
                if (ui::Button("##rm_event", "Remove Event", ui::EUiVariant::Destructive,
                               ui::EUiSize::Sm)) {
                    doc.document.events.erase(doc.document.events.begin() +
                                              static_cast<std::ptrdiff_t>(ei));
                    doc.dirty = true;
                    ImGui::PopID();
                    break;
                }
                ui::Separator();
                ImGui::PopID();
            }
            if (ui::Button("##add_beginplay", "Add begin play event", ui::EUiVariant::Secondary,
                           ui::EUiSize::Sm)) {
                BlueprintEventDesc e;
                e.type = EBlueprintEventType::BeginPlay;
                doc.document.events.push_back(e);
                doc.dirty = true;
            }
            ImGui::SameLine();
            if (ui::Button("##add_overlap", "Add trigger overlap event", ui::EUiVariant::Secondary,
                           ui::EUiSize::Sm)) {
                BlueprintEventDesc e;
                e.type = EBlueprintEventType::OnTriggerOverlap;
                doc.document.events.push_back(e);
                doc.dirty = true;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void BlueprintEditorPanel::Draw(EditorContext& ctx) {
    if (!ctx.requestOpenBlueprintPath.empty()) {
        Open(ctx.requestOpenBlueprintPath);
        ctx.requestOpenBlueprintPath.clear();
        ctx.showBlueprintEditor = true;
        ctx.requestFocusBlueprintEditor = true;
    }
    if (!ctx.showBlueprintEditor) {
        SyncDirtyPaths(ctx);
        return;
    }

    if (ctx.requestFocusBlueprintEditor) {
        ImGui::SetNextWindowFocus();
        ctx.requestFocusBlueprintEditor = false;
    }

    if (!ui::BeginPanel("Blueprint Editor", {.pOpen = &ctx.showBlueprintEditor})) {
        ui::EndPanel();
        SyncDirtyPaths(ctx);
        return;
    }

    if (docs_.empty()) {
        ui::Hint("Double-click a .lbp Blueprint in the Content Browser, or create one with "
                 "right-click → New Blueprint.");
        ui::EndPanel();
        SyncDirtyPaths(ctx);
        return;
    }

    if (ImGui::BeginTabBar("##BlueprintEditorTabs",
                           ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (Doc& doc : docs_) {
            if (!doc.open) {
                continue;
            }
            std::string tabLabel = doc.document.name;
            if (doc.dirty) {
                tabLabel += " *";
            }
            tabLabel += "###BpTab" + doc.path;
            ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
            if (doc.focusTab) {
                flags |= ImGuiTabItemFlags_SetSelected;
                doc.focusTab = false;
            }
            bool open = true;
            if (ImGui::BeginTabItem(tabLabel.c_str(), &open, flags)) {
                focusedPath_ = doc.path;
                DrawDoc(ctx, doc);
                {
                    ui::UiButtonDesc save;
                    save.label = "Save";
                    save.icon = ui::UiIcon(ELucideIcon::Save);
                    save.variant = ui::EUiVariant::Primary;
                    save.size = ui::EUiSize::Sm;
                    if (ui::Button("##bp_save", save)) {
                        (void)SaveDoc(ctx, doc);
                    }
                }
                ImGui::EndTabItem();
            }
            if (!open) {
                if (doc.dirty) {
                    open = true;
                    ctx.pendingCloseAssetTabPath = doc.path;
                    ctx.pendingCloseAssetTabKind = EAssetEditorTabKind::Blueprint;
                    ImGui::OpenPopup("Close Asset Tab");
                } else {
                    doc.open = false;
                }
            }
        }
        ImGui::EndTabBar();
    }

    ui::EndPanel();

    docs_.erase(std::remove_if(docs_.begin(), docs_.end(), [](const Doc& d) { return !d.open; }),
                docs_.end());
    if (docs_.empty()) {
        ctx.showBlueprintEditor = false;
        focusedPath_.clear();
    }

    SyncDirtyPaths(ctx);
}

} // namespace leon::editor
