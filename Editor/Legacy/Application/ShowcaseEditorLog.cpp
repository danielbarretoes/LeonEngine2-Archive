#include <leon/core/Ascii.h>
#include <leon/editor/EditorContext.h>
#include <leon/editor/EditorOutputLog.h>
#include <leon/editor/ShowcaseEditorLog.h>
#include <leon/level/Level.h>
#include <leon/level/Light.h>

namespace leon::editor {
namespace {

void LogInfo(std::string_view message) {
    EditorLogInfo(std::string(message));
}

void LogStation(std::string_view zone, std::string_view feature, std::string_view hint) {
    EditorLogInfo(std::string("[Showcase] ") + std::string(zone) + " - " + std::string(feature) +
                  " | " + std::string(hint));
}

void CountLights(const Level& level, int& pointTotal, int& spotTotal, int& dirShadow) {
    pointTotal = spotTotal = dirShadow = 0;
    for (const DirectionalLight& light : level.DirectionalLights()) {
        if (light.castShadows) {
            ++dirShadow;
        }
    }
    pointTotal = static_cast<int>(level.PointLights().size());
    spotTotal = static_cast<int>(level.SpotLights().size());
}

} // namespace

bool IsShowcaseProject(std::string_view projectName) {
    return AsciiToLower(std::string(projectName)) == "showcase";
}

void LogShowcaseEditorManifest(const EditorContext& ctx, const Level& level) {
    LogInfo("========== Leon Fundamentals Showcase (Editor) ==========");
    EditorLogInfo("[Showcase] Project '" + ctx.projectDisplayName +
                  "' template=" + ctx.projectTemplateId);
    EditorLogInfo("[Showcase] Level '" + level.Name() + "' env=" + level.EnvironmentPath() +
                  " exposure=" + std::to_string(level.EnvironmentExposure()));

    int pointTotal = 0;
    int spotTotal = 0;
    int dirShadow = 0;
    CountLights(level, pointTotal, spotTotal, dirShadow);

    EditorLogInfo("[Showcase] Lights: dirShadow=" + std::to_string(dirShadow) +
                  " point=" + std::to_string(pointTotal) + " spot=" + std::to_string(spotTotal));
    EditorLogInfo("[Showcase] Drawables: meshes=" + std::to_string(level.StaticMeshes().size()) +
                  " labels=" + std::to_string(level.TextRenderActors().size()) +
                  " playerStarts=" + std::to_string(level.PlayerStarts().size()));

    LogInfo("[Showcase] --- Zones ---");
    LogStation("1", "Geometry", "Cube, sphere, plane, cylinder");
    LogStation("2", "Hierarchy", "Parent/child door, lamp, robot");
    LogStation("3-11", "Rendering", "Lighting, textures, normals, reflections, transparency");

    LogInfo("[Showcase] --- Editor ---");
    LogStation("Stamp", "Regenerate", "leon-make-fundamentals-showcase Content/Levels");

    LogInfo("========================================================");
}

} // namespace leon::editor
