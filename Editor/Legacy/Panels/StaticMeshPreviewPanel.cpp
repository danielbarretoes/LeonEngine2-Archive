#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <imgui.h>
#include <leon/core/Paths.h>
#include <leon/core/Window.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorHistory.h>
#include <leon/editor/EditorOutputLog.h>
#include <leon/editor/panels/StaticMeshPreviewPanel.h>
#include <leon/editor/PreviewEnvironment.h>
#include <leon/editor/StaticMeshAssetPath.h>
#include <leon/editor/ui/UiKit.h>
#include <leon/render/Renderer.h>
#include <leon/render/ResourceCache.h>

namespace leon::editor {
namespace fs = std::filesystem;

namespace {

constexpr float kDetailsPanelWidth = 300.0f;

[[nodiscard]] int VertexCountOf(const StaticMesh& mesh) {
    if (mesh.HasCpuData()) {
        return static_cast<int>(mesh.CpuData().vertices.size());
    }
    return 0;
}

} // namespace

StaticMeshPreviewPanel::Doc* StaticMeshPreviewPanel::FindDoc(const std::string& path) {
    for (Doc& doc : docs_) {
        if (PathsEqualNormalized(doc.path, path)) {
            return &doc;
        }
    }
    return nullptr;
}

void StaticMeshPreviewPanel::SyncPreviewLighting(Doc& doc) {
    if (doc.previewLevel.DirectionalLights().empty()) {
        ApplyDefaultPreviewSun(doc.previewLevel);
    }
    doc.previewLevel.DirectionalLights().front().intensity = doc.sunIntensity;
    doc.previewLevel.SetEnvironmentExposure(doc.envExposure);
}

void StaticMeshPreviewPanel::ResetCamera(Doc& doc) {
    if (doc.mesh == nullptr) {
        return;
    }
    const glm::vec3 mn = doc.mesh->LocalMin();
    const glm::vec3 mx = doc.mesh->LocalMax();
    const glm::vec3 center = (mn + mx) * 0.5f;
    const float radius = std::max(0.08f, glm::length(mx - mn) * 0.5f);
    doc.camera.SetMode(ECameraMode::Orbit);
    doc.camera.SetTarget(center);
    doc.camera.SetYawPitch(35.0f, 18.0f);
    doc.camera.SetDistance(radius * 2.8f);
}

bool StaticMeshPreviewPanel::LoadDoc(EditorContext& ctx, Doc& doc, const std::string& path) {
    if (ctx.resources == nullptr) {
        return false;
    }
    const std::string cooked = ResolveCookedStaticMeshPath(path);
    if (cooked.empty()) {
        doc.status = "No cooked .lmesh for " + fs::path(path).filename().string();
        return false;
    }
    auto mesh = ctx.resources->LoadStaticMesh(cooked);
    if (mesh == nullptr || !mesh->Valid()) {
        doc.status = "Failed to load " + fs::path(cooked).filename().string();
        return false;
    }

    doc.path = path;
    doc.mesh = std::move(mesh);
    doc.previewLevel.Clear();
    ApplyDefaultPreviewLighting(doc.previewLevel, *ctx.resources);
    SyncPreviewLighting(doc);

    StaticMeshComponent comp;
    comp.mesh = doc.mesh;
    comp.editorClass = "StaticMesh";
    comp.meshPath = MakePackRelativeAssetPath(ctx, cooked);
    if (comp.meshPath.empty()) {
        comp.meshPath = cooked;
    }
    comp.materialOverride = false;
    comp.material = ctx.resources->DefaultMaterial();
    doc.previewLevel.AddStaticMesh(std::move(comp));

    ResetCamera(doc);
    doc.status = fs::path(cooked).filename().string();
    return true;
}

void StaticMeshPreviewPanel::Open(const std::string& path) {
    const std::string norm = NormalizeAssetPathAbs(path);
    if (norm.empty()) {
        return;
    }
    if (Doc* existing = FindDoc(norm)) {
        existing->open = true;
        existing->focusTab = true;
        activePath_ = norm;
        return;
    }
    Doc doc;
    doc.path = norm;
    doc.focusTab = true;
    docs_.push_back(std::move(doc));
    activePath_ = norm;
}

void StaticMeshPreviewPanel::CloseDoc(const std::string& path) {
    if (Doc* doc = FindDoc(path)) {
        doc->open = false;
        if (PathsEqualNormalized(activePath_, path)) {
            activePath_.clear();
        }
    }
}

void StaticMeshPreviewPanel::RemapAssetPath(EditorContext& /*ctx*/, const std::string& fromAbs,
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
            if (PathsEqualNormalized(activePath_, doc.path)) {
                activePath_.clear();
            }
        } else {
            doc.path = NormalizeAssetPathAbs(toAbs);
            if (PathsEqualNormalized(activePath_, fromAbs)) {
                activePath_ = doc.path;
            }
        }
    }
}

void StaticMeshPreviewPanel::PlaceInLevel(EditorContext& ctx, Doc& doc) {
    if (ctx.level == nullptr || ctx.resources == nullptr || doc.mesh == nullptr) {
        return;
    }
    if (ctx.history != nullptr) {
        ctx.history->Capture(ctx, "Place StaticMesh");
    }
    StaticMeshComponent comp;
    comp.mesh = doc.mesh;
    comp.editorClass = "StaticMesh";
    const std::string cookedAbs = ResolveCookedStaticMeshPath(doc.path);
    const std::string persistAbs = cookedAbs.empty() ? doc.path : cookedAbs;
    comp.meshPath = MakePackRelativeAssetPath(ctx, persistAbs);
    if (comp.meshPath.empty()) {
        comp.meshPath = persistAbs;
    }
    comp.transform.position = ctx.camera != nullptr ? ctx.camera->Target() : glm::vec3{0, 0.5f, 0};
    const std::string placedPath = comp.meshPath;
    ctx.level->AddStaticMesh(std::move(comp));
    ctx.Select(EEditorSelectionKind::StaticMesh, ctx.level->StaticMeshes().size() - 1);
    ctx.MarkDirty();
    EditorLogInfo("Static Mesh Editor: placed " + placedPath + " in Level");
}

void StaticMeshPreviewPanel::HandleOrbit(EditorContext& ctx, Doc& doc) {
    if (ctx.window == nullptr || !ImGui::IsWindowHovered()) {
        doc.orbitDragging = false;
        return;
    }
    Window& window = *ctx.window;
    const bool rmb = window.IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT);
    double mx = 0.0;
    double my = 0.0;
    window.GetCursorPos(mx, my);
    if (rmb) {
        if (doc.orbitDragging) {
            doc.camera.Orbit(static_cast<float>(mx - doc.lastMouseX) * 0.25f,
                             -static_cast<float>(my - doc.lastMouseY) * 0.25f);
        }
        doc.orbitDragging = true;
        doc.lastMouseX = mx;
        doc.lastMouseY = my;
    } else {
        doc.orbitDragging = false;
        doc.lastMouseX = mx;
        doc.lastMouseY = my;
    }
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f && ImGui::IsWindowHovered()) {
        doc.camera.Zoom(-wheel * 0.35f);
    }
}

void StaticMeshPreviewPanel::RenderViewport(EditorContext& ctx, Doc& doc, int vw, int vh) {
    if (ctx.renderer == nullptr || doc.mesh == nullptr) {
        return;
    }
    if (!doc.target || !doc.target->EnsureSize(vw, vh)) {
        return;
    }
    const int fbW = doc.target->width();
    const int fbH = doc.target->height();
    doc.camera.SetPerspective(45.0f, static_cast<float>(fbW) / static_cast<float>(fbH), 0.05f,
                              500.0f);

    SyncPreviewLighting(doc);
    ctx.renderer->SetDrawFramebuffer(doc.target->fbo());
    ctx.renderer->BeginFrame(fbW, fbH);
    ctx.renderer->DrawScene(doc.previewLevel, doc.camera);
    ctx.renderer->SetDrawFramebuffer(0);
    doc.target->End();

    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(doc.target->colorTexture())),
                 ImVec2(static_cast<float>(vw), static_cast<float>(vh)), ImVec2(0, 1),
                 ImVec2(1, 0));
}

void StaticMeshPreviewPanel::DrawDetails(EditorContext& ctx, Doc& doc) {
    ImGui::TextUnformatted("Details");
    ImGui::Separator();

    if (ui::CollapsingSection("Asset")) {
        ImGui::TextWrapped("%s", doc.path.c_str());
        if (!doc.status.empty()) {
            ImGui::TextDisabled("%s", doc.status.c_str());
        }
        if (ui::Button("##sme_reveal", "Reveal in Content Browser", ui::EUiVariant::Ghost,
                       ui::EUiSize::Sm)) {
            ctx.requestRevealContentPath = doc.path;
        }
        if (ui::Button("##sme_place", "Place in Level", ui::EUiVariant::Primary, ui::EUiSize::Sm)) {
            PlaceInLevel(ctx, doc);
        }
    }

    if (doc.mesh != nullptr && ui::CollapsingSection("Stats")) {
        const glm::vec3 mn = doc.mesh->LocalMin();
        const glm::vec3 mx = doc.mesh->LocalMax();
        const glm::vec3 extent = mx - mn;
        const int verts = VertexCountOf(*doc.mesh);
        ImGui::Text("Triangles: %d", doc.mesh->TriangleCount());
        if (verts > 0) {
            ImGui::Text("Vertices: %d", verts);
        }
        ImGui::Text("Sections: %d", static_cast<int>(doc.mesh->Submeshes().size()));
        ImGui::Text("Materials: %d", static_cast<int>(doc.mesh->Materials().size()));
        ImGui::Text("Bounds: %.2f × %.2f × %.2f", extent.x, extent.y, extent.z);
    }

    if (doc.mesh != nullptr && ui::CollapsingSection("Materials")) {
        const auto& mats = doc.mesh->Materials();
        if (mats.empty()) {
            ImGui::TextDisabled("No embedded materials (engine default)");
        } else {
            for (std::size_t i = 0; i < mats.size(); ++i) {
                ImGui::BulletText("[%zu] slot", i);
            }
        }
    }

    if (ui::CollapsingSection("Preview")) {
        ImGui::TextDisabled("RMB orbit · Wheel zoom");
        if (ImGui::SliderFloat("Sun Intensity", &doc.sunIntensity, 0.0f, 8.0f, "%.2f")) {
            SyncPreviewLighting(doc);
        }
        if (ImGui::SliderFloat("Sky Exposure", &doc.envExposure, 0.1f, 4.0f, "%.2f")) {
            SyncPreviewLighting(doc);
        }
        if (ui::Button("##sme_reset_cam", "Reset Camera", ui::EUiVariant::Secondary,
                       ui::EUiSize::Sm)) {
            ResetCamera(doc);
        }
        ImGui::SameLine();
        if (ui::Button("##sme_reset_lit", "Reset Lighting", ui::EUiVariant::Ghost,
                       ui::EUiSize::Sm)) {
            doc.sunIntensity = 3.25f;
            doc.envExposure = 1.15f;
            SyncPreviewLighting(doc);
        }
    }
}

void StaticMeshPreviewPanel::DrawActiveDoc(EditorContext& ctx, Doc& doc) {
    if (doc.mesh == nullptr && ctx.resources != nullptr) {
        (void)LoadDoc(ctx, doc, doc.path);
    }

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float detailsW = std::min(kDetailsPanelWidth, std::max(200.0f, avail.x * 0.32f));
    const float viewportW = std::max(64.0f, avail.x - detailsW - 8.0f);

    ImGui::BeginChild("##StaticMeshViewport", ImVec2(viewportW, avail.y), true);
    HandleOrbit(ctx, doc);
    const ImVec2 vpAvail = ImGui::GetContentRegionAvail();
    const int vw = std::max(64, static_cast<int>(vpAvail.x));
    const int vh = std::max(64, static_cast<int>(vpAvail.y));
    RenderViewport(ctx, doc, vw, vh);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##StaticMeshDetails", ImVec2(detailsW, avail.y), true);
    DrawDetails(ctx, doc);
    ImGui::EndChild();
}

void StaticMeshPreviewPanel::Draw(EditorContext& ctx) {
    if (!ctx.requestOpenMeshPreviewPath.empty()) {
        Open(ctx.requestOpenMeshPreviewPath);
        ctx.requestOpenMeshPreviewPath.clear();
        ctx.showStaticMeshEditor = true;
        ctx.requestFocusStaticMeshEditor = true;
    }

    if (!ctx.showStaticMeshEditor) {
        return;
    }

    if (ctx.requestFocusStaticMeshEditor) {
        ImGui::SetNextWindowFocus();
        ctx.requestFocusStaticMeshEditor = false;
    }

    ImGui::SetNextWindowSize(ImVec2(960.0f, 640.0f), ImGuiCond_FirstUseEver);
    if (!ui::BeginPanel("Static Mesh Editor", {.pOpen = &ctx.showStaticMeshEditor})) {
        ui::EndPanel();
        return;
    }

    if (docs_.empty()) {
        ui::Hint("Viewport + Details for cooked .lmesh assets. "
                 "Double-click a mesh in the Content Browser to open.");
        ui::EndPanel();
        return;
    }

    if (ImGui::BeginTabBar("##StaticMeshEditorTabs",
                           ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (Doc& doc : docs_) {
            if (!doc.open) {
                continue;
            }
            if (doc.mesh == nullptr && ctx.resources != nullptr) {
                (void)LoadDoc(ctx, doc, doc.path);
            }
            const std::string stem = fs::path(doc.path).stem().string();
            const std::string tabId = stem + "###SmeTab" + doc.path;
            ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
            if (doc.focusTab) {
                flags |= ImGuiTabItemFlags_SetSelected;
                doc.focusTab = false;
            }
            bool open = true;
            if (ImGui::BeginTabItem(tabId.c_str(), &open, flags)) {
                activePath_ = doc.path;
                DrawActiveDoc(ctx, doc);
                ImGui::EndTabItem();
            }
            if (!open) {
                doc.open = false;
            }
        }
        ImGui::EndTabBar();
    }

    ui::EndPanel();

    docs_.erase(std::remove_if(docs_.begin(), docs_.end(), [](const Doc& d) { return !d.open; }),
                docs_.end());
    if (docs_.empty()) {
        ctx.showStaticMeshEditor = false;
        activePath_.clear();
    } else if (FindDoc(activePath_) == nullptr) {
        activePath_ = docs_.front().path;
    }
}

} // namespace leon::editor
