#include <algorithm>
#include <cstdio>
#include <imgui.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorOutputLog.h>
#include <leon/editor/LucideIcons.h>
#include <leon/editor/panels/WidgetDesignerPanel.h>
#include <leon/editor/ui/UiKit.h>

namespace leon::editor {
namespace {

const char* kWidgetClassNames[] = {"User widget", "Canvas", "Vertical box", "Button",
                                   "Text block",  "Image",  "Progress bar", "Menu list"};

} // namespace

WidgetDesignerPanel::Doc* WidgetDesignerPanel::FindDoc(const std::string& path) {
    for (Doc& doc : docs_) {
        if (PathsEqualNormalized(doc.path, path)) {
            return &doc;
        }
    }
    return nullptr;
}

void WidgetDesignerPanel::Open(const std::string& path) {
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
    UserWidgetDocument document;
    if (!LoadUserWidgetDocument(norm, document)) {
        EditorLogError("Widget Designer: failed to load " + norm);
        return;
    }
    Doc doc;
    doc.path = norm;
    doc.document = std::move(document);
    doc.focusTab = true;
    docs_.push_back(std::move(doc));
    focusedPath_ = norm;
}

void WidgetDesignerPanel::CloseDoc(const std::string& path) {
    if (Doc* doc = FindDoc(path)) {
        doc->open = false;
        if (PathsEqualNormalized(focusedPath_, path)) {
            focusedPath_.clear();
        }
    }
}

void WidgetDesignerPanel::RemapAssetPath(EditorContext& /*ctx*/, const std::string& fromAbs,
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

bool WidgetDesignerPanel::SavePath(EditorContext& ctx, const std::string& path) {
    Doc* doc = FindDoc(path);
    if (doc == nullptr || !doc->open) {
        return false;
    }
    if (!doc->dirty) {
        return true;
    }
    return SaveDoc(ctx, *doc);
}

bool WidgetDesignerPanel::HasDirtyDocs() const {
    for (const Doc& doc : docs_) {
        if (doc.dirty) {
            return true;
        }
    }
    return false;
}

bool WidgetDesignerPanel::HasFocusedDirtyDoc() const {
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

bool WidgetDesignerPanel::SaveDoc(EditorContext& /*ctx*/, Doc& doc) {
    if (!SaveUserWidgetDocument(doc.path, doc.document)) {
        EditorLogError("Widget Designer: failed to save " + doc.path);
        return false;
    }
    doc.dirty = false;
    EditorLogInfo("Saved Widget " + doc.path);
    return true;
}

bool WidgetDesignerPanel::SaveFocused(EditorContext& ctx) {
    Doc* doc = FindDoc(focusedPath_);
    if (doc == nullptr || !doc->open) {
        return false;
    }
    if (!doc->dirty) {
        return true;
    }
    return SaveDoc(ctx, *doc);
}

bool WidgetDesignerPanel::SaveAll(EditorContext& ctx) {
    bool ok = true;
    for (Doc& doc : docs_) {
        if (doc.dirty) {
            ok = SaveDoc(ctx, doc) && ok;
        }
    }
    return ok;
}

void WidgetDesignerPanel::SyncDirtyPaths(EditorContext& ctx) const {
    ctx.dirtyWidgetPaths.clear();
    for (const Doc& doc : docs_) {
        if (doc.open && doc.dirty) {
            ctx.dirtyWidgetPaths.push_back(doc.path);
        }
    }
}

void WidgetDesignerPanel::DrawDoc(EditorContext& /*ctx*/, Doc& doc) {
    ImGui::TextWrapped("%s", doc.path.c_str());
    char nameBuf[128];
    (void)std::snprintf(nameBuf, sizeof(nameBuf), "%s", doc.document.name.c_str());
    if (ui::InputText("##wd_name", "Name", nameBuf, sizeof(nameBuf))) {
        doc.document.name = nameBuf;
        doc.dirty = true;
    }

    int rootClass = static_cast<int>(doc.document.root.widgetClass);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Root class");
    ImGui::SameLine();
    if (ui::SelectFromList("##root_class", &rootClass, kWidgetClassNames,
                           IM_ARRAYSIZE(kWidgetClassNames), ui::EUiSize::Sm, 180.0f)) {
        doc.document.root.widgetClass = static_cast<EWidgetClass>(rootClass);
        doc.dirty = true;
    }
    char rootId[64];
    (void)std::snprintf(rootId, sizeof(rootId), "%s", doc.document.root.id.c_str());
    if (ui::InputText("##wd_root_id", "Root ID", rootId, sizeof(rootId))) {
        doc.document.root.id = rootId;
        doc.dirty = true;
    }

    ui::SectionLabel("Hierarchy");
    if (ImGui::Selectable("(Root)", doc.selectedChild < 0)) {
        doc.selectedChild = -1;
    }
    for (std::size_t i = 0; i < doc.document.root.children.size(); ++i) {
        const WidgetNodeDesc& child = doc.document.root.children[i];
        const std::string label =
            child.id.empty()
                ? (std::string(WidgetClassToString(child.widgetClass)) + "##" + std::to_string(i))
                : child.id;
        if (ImGui::Selectable(label.c_str(), doc.selectedChild == static_cast<int>(i))) {
            doc.selectedChild = static_cast<int>(i);
        }
    }
    if (ui::Button("##add_vbox", "Add vertical box", ui::EUiVariant::Secondary, ui::EUiSize::Sm)) {
        WidgetNodeDesc node;
        node.widgetClass = EWidgetClass::VerticalBox;
        node.id = "MenuRoot";
        node.text = "Menu";
        doc.document.root.children.push_back(node);
        doc.dirty = true;
    }
    ImGui::SameLine();
    if (ui::Button("##add_image", "Add Image", ui::EUiVariant::Secondary, ui::EUiSize::Sm)) {
        WidgetNodeDesc node;
        node.widgetClass = EWidgetClass::Image;
        node.id = "Backdrop";
        node.fillScreen = true;
        doc.document.root.children.push_back(node);
        doc.dirty = true;
    }
    ImGui::SameLine();
    if (ui::Button("##add_pbar", "Add progress bar", ui::EUiVariant::Secondary, ui::EUiSize::Sm)) {
        WidgetNodeDesc node;
        node.widgetClass = EWidgetClass::ProgressBar;
        node.id = "JoinProgress";
        node.anchoredBottomCenter = true;
        doc.document.root.children.push_back(node);
        doc.dirty = true;
    }

    WidgetNodeDesc* selected = nullptr;
    if (doc.selectedChild < 0) {
        selected = &doc.document.root;
    } else if (doc.selectedChild < static_cast<int>(doc.document.root.children.size())) {
        selected = &doc.document.root.children[static_cast<std::size_t>(doc.selectedChild)];
    }
    if (selected == nullptr) {
        return;
    }

    ui::SectionLabel("Details");
    int classIndex = static_cast<int>(selected->widgetClass);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Class");
    ImGui::SameLine();
    if (ui::SelectFromList("##widget_class", &classIndex, kWidgetClassNames,
                           IM_ARRAYSIZE(kWidgetClassNames), ui::EUiSize::Sm, 180.0f)) {
        selected->widgetClass = static_cast<EWidgetClass>(classIndex);
        doc.dirty = true;
    }
    char idBuf[64];
    (void)std::snprintf(idBuf, sizeof(idBuf), "%s", selected->id.c_str());
    if (ui::InputText("##wd_id", "ID", idBuf, sizeof(idBuf))) {
        selected->id = idBuf;
        doc.dirty = true;
    }
    char textBuf[256];
    (void)std::snprintf(textBuf, sizeof(textBuf), "%s", selected->text.c_str());
    if (ui::InputText("##wd_text", "Text", textBuf, sizeof(textBuf))) {
        selected->text = textBuf;
        doc.dirty = true;
    }
    char hintBuf[256];
    (void)std::snprintf(hintBuf, sizeof(hintBuf), "%s", selected->hint.c_str());
    if (ui::InputText("##wd_hint", "Hint", hintBuf, sizeof(hintBuf))) {
        selected->hint = hintBuf;
        doc.dirty = true;
    }
    if (ui::ColorEdit3("##wd_color", "Color", &selected->color.x)) {
        doc.dirty = true;
    }
    if (ui::Checkbox("##visible", "Visible", &selected->visible, ui::EUiSize::Sm)) {
        doc.dirty = true;
    }
    if (ui::Checkbox("##fill", "Fill Screen", &selected->fillScreen, ui::EUiSize::Sm)) {
        doc.dirty = true;
    }
    if (ui::Checkbox("##anchor_bc", "Anchored Bottom Center", &selected->anchoredBottomCenter,
                     ui::EUiSize::Sm)) {
        doc.dirty = true;
    }
    if (ui::DragFloat("##wd_percent", "Percent", &selected->percent, 0.01f, 0.0f, 1.0f)) {
        doc.dirty = true;
    }
    if (ui::DragFloat4("##wd_anchors", "Anchors", &selected->anchors.minX, 0.01f)) {
        doc.dirty = true;
    }
    if (ui::DragFloat2("##wd_pos", "Pos", &selected->posX, 1.0f)) {
        doc.dirty = true;
    }
    if (ui::DragFloat2("##wd_size", "Size", &selected->sizeX, 1.0f)) {
        doc.dirty = true;
    }

    if (selected->widgetClass == EWidgetClass::VerticalBox && ImGui::TreeNode("Buttons")) {
        for (std::size_t i = 0; i < selected->children.size(); ++i) {
            WidgetNodeDesc& btn = selected->children[i];
            ImGui::PushID(static_cast<int>(i));
            char btnId[64];
            (void)std::snprintf(btnId, sizeof(btnId), "%s", btn.buttonId.c_str());
            if (ui::InputText("##btn_id", "Button ID", btnId, sizeof(btnId))) {
                btn.buttonId = btnId;
                btn.id = btnId;
                doc.dirty = true;
            }
            char label[128];
            (void)std::snprintf(label, sizeof(label), "%s", btn.text.c_str());
            if (ui::InputText("##btn_label", "Label", label, sizeof(label))) {
                btn.text = label;
                doc.dirty = true;
            }
            ImGui::PopID();
        }
        if (ui::Button("##add_btn", "Add Button", ui::EUiVariant::Secondary, ui::EUiSize::Sm)) {
            WidgetNodeDesc btn;
            btn.widgetClass = EWidgetClass::Button;
            btn.id = "button";
            btn.buttonId = "button";
            btn.text = "Button";
            selected->children.push_back(btn);
            doc.dirty = true;
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Bindings")) {
        for (std::size_t i = 0; i < selected->bindings.size(); ++i) {
            WidgetBindingDesc& b = selected->bindings[i];
            ImGui::PushID(static_cast<int>(i));
            char prop[64];
            char src[128];
            (void)std::snprintf(prop, sizeof(prop), "%s", b.widgetProperty.c_str());
            (void)std::snprintf(src, sizeof(src), "%s", b.sourcePath.c_str());
            if (ui::InputText("##bind_prop", "Property", prop, sizeof(prop))) {
                b.widgetProperty = prop;
                doc.dirty = true;
            }
            if (ui::InputText("##bind_src", "Source", src, sizeof(src))) {
                b.sourcePath = src;
                doc.dirty = true;
            }
            ImGui::PopID();
        }
        if (ui::Button("##add_bind", "Add Binding", ui::EUiVariant::Secondary, ui::EUiSize::Sm)) {
            WidgetBindingDesc b;
            b.widgetProperty = "Percent";
            b.sourcePath = "WidgetVar:Value";
            selected->bindings.push_back(b);
            doc.dirty = true;
        }
        ImGui::TreePop();
    }

    if (doc.selectedChild >= 0 && ui::Button("##rm_child", "Remove Selected Child",
                                             ui::EUiVariant::Destructive, ui::EUiSize::Sm)) {
        doc.document.root.children.erase(doc.document.root.children.begin() + doc.selectedChild);
        doc.selectedChild = -1;
        doc.dirty = true;
    }
}

void WidgetDesignerPanel::Draw(EditorContext& ctx) {
    if (!ctx.requestOpenWidgetPath.empty()) {
        Open(ctx.requestOpenWidgetPath);
        ctx.requestOpenWidgetPath.clear();
        ctx.showWidgetDesigner = true;
        ctx.requestFocusWidgetDesigner = true;
    }
    if (!ctx.showWidgetDesigner) {
        SyncDirtyPaths(ctx);
        return;
    }

    if (ctx.requestFocusWidgetDesigner) {
        ImGui::SetNextWindowFocus();
        ctx.requestFocusWidgetDesigner = false;
    }

    if (!ui::BeginPanel("Widget Designer", {.pOpen = &ctx.showWidgetDesigner})) {
        ui::EndPanel();
        SyncDirtyPaths(ctx);
        return;
    }

    if (docs_.empty()) {
        ui::Hint("Double-click a .luw Widget in the Content Browser, or create one with "
                 "right-click → New Widget Blueprint.");
        ui::EndPanel();
        SyncDirtyPaths(ctx);
        return;
    }

    if (ImGui::BeginTabBar("##WidgetDesignerTabs",
                           ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (Doc& doc : docs_) {
            if (!doc.open) {
                continue;
            }
            std::string tabLabel = doc.document.name;
            if (doc.dirty) {
                tabLabel += " *";
            }
            tabLabel += "###UwTab" + doc.path;
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
                    if (ui::Button("##uw_save", save)) {
                        (void)SaveDoc(ctx, doc);
                    }
                }
                ImGui::EndTabItem();
            }
            if (!open) {
                if (doc.dirty) {
                    open = true;
                    ctx.pendingCloseAssetTabPath = doc.path;
                    ctx.pendingCloseAssetTabKind = EAssetEditorTabKind::Widget;
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
        ctx.showWidgetDesigner = false;
        focusedPath_.clear();
    }

    SyncDirtyPaths(ctx);
}

} // namespace leon::editor
