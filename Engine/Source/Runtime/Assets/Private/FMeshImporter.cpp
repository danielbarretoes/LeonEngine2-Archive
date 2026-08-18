#include "Assets/FMeshImporter.hpp"
#include "Assets/FSkeletalImporter.hpp"
#include "Assets/FAssetPath.hpp"
#include "Core/FLog.hpp"
#include "Renderer/FVertexLayout.hpp"
#include <ufbx.h>

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>

namespace Leon {

    namespace {

        std::string SanitizeMeshToken(std::string InName) {
            for (char& c : InName) {
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-')
                    c = '_';
            }
            InName.erase(std::remove_if(InName.begin(), InName.end(), [](unsigned char c) { return std::isspace(c); }),
                         InName.end());
            while (!InName.empty() && InName.front() == '_')
                InName.erase(InName.begin());
            while (!InName.empty() && InName.back() == '_')
                InName.pop_back();
            return InName.empty() ? "Mesh" : InName;
        }

        void CopyMaterialSlots(UStaticMesh& InMesh, const std::vector<FExtractedMaterial>& InMaterials) {
            InMesh.GetMaterialSlots().clear();
            for (const auto& extracted : InMaterials) {
                FStaticMaterialSlot slot;
                slot.SlotName = extracted.Name;
                std::string formattedMatName =
                    (extracted.Name.rfind("M_", 0) == 0) ? extracted.Name : ("M_" + extracted.Name);
                slot.DefaultMaterialPath = "Materials/" + formattedMatName + ".lmat";
                InMesh.GetMaterialSlots().push_back(slot);
            }
            if (InMesh.GetMaterialSlots().empty()) {
                FStaticMaterialSlot defSlot;
                defSlot.SlotName = "DefaultMaterial";
                defSlot.DefaultMaterialPath = "Materials/M_DefaultPBR.lmat";
                InMesh.GetMaterialSlots().push_back(defSlot);
            }
        }

        FStaticMeshVertex MakeFbxVertex(const ufbx_mesh* InMesh, uint32_t InIndexInMesh, const glm::mat4& InLocalMat,
                                        const glm::mat3& InNormMat, bool bFlipUVs) {
            FStaticMeshVertex v;
            if (InMesh->vertex_position.exists) {
                ufbx_vec3 p = ufbx_get_vertex_vec3(&InMesh->vertex_position, InIndexInMesh);
                glm::vec4 localPos(static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z), 1.0f);
                v.Position = glm::vec3(InLocalMat * localPos);
            }
            if (InMesh->vertex_normal.exists) {
                ufbx_vec3 n = ufbx_get_vertex_vec3(&InMesh->vertex_normal, InIndexInMesh);
                v.Normal = glm::normalize(
                    InNormMat * glm::vec3(static_cast<float>(n.x), static_cast<float>(n.y), static_cast<float>(n.z)));
            }
            if (InMesh->vertex_uv.exists) {
                ufbx_vec2 uv = ufbx_get_vertex_vec2(&InMesh->vertex_uv, InIndexInMesh);
                v.TexCoord = glm::vec2(static_cast<float>(uv.x),
                                       bFlipUVs ? (1.0f - static_cast<float>(uv.y)) : static_cast<float>(uv.y));
            }
            glm::vec3 tangent{1.0f, 0.0f, 0.0f};
            glm::vec3 bitangent = glm::cross(v.Normal, tangent);
            if (InMesh->vertex_tangent.exists) {
                ufbx_vec3 t = ufbx_get_vertex_vec3(&InMesh->vertex_tangent, InIndexInMesh);
                tangent = glm::normalize(
                    InNormMat * glm::vec3(static_cast<float>(t.x), static_cast<float>(t.y), static_cast<float>(t.z)));
            }
            if (InMesh->vertex_bitangent.exists) {
                ufbx_vec3 b = ufbx_get_vertex_vec3(&InMesh->vertex_bitangent, InIndexInMesh);
                bitangent = glm::normalize(
                    InNormMat * glm::vec3(static_cast<float>(b.x), static_cast<float>(b.y), static_cast<float>(b.z)));
            } else {
                bitangent = glm::normalize(glm::cross(v.Normal, tangent));
            }
            v.Tangent = PackTangent(tangent, v.Normal, bitangent);
            v.LightmapUV = glm::vec2(0.0f);
            if (InMesh->vertex_color.exists) {
                ufbx_vec4 col = ufbx_get_vertex_vec4(&InMesh->vertex_color, InIndexInMesh);
                v.Color = glm::vec3(static_cast<float>(col.x), static_cast<float>(col.y), static_cast<float>(col.z));
            } else {
                v.Color = glm::vec3(1.0f);
            }
            return v;
        }

        void AppendTriangulatedFaces(UStaticMesh& InMesh, const ufbx_mesh* InUMesh, const uint32_t* InFaceIndices,
                                     size_t InFaceCount, const glm::mat4& InLocalMat, const glm::mat3& InNormMat,
                                     bool bFlipUVs, FStaticSubmesh& InOutSubmesh) {
            std::vector<FStaticMeshVertex>& verts = InMesh.GetVertices();
            std::vector<uint32_t>& indices = InMesh.GetIndices();
            const uint32_t vertexStart = static_cast<uint32_t>(verts.size());
            InOutSubmesh.IndexOffset = static_cast<uint32_t>(indices.size());
            InOutSubmesh.VertexOffset = vertexStart;
            for (size_t fi = 0; fi < InFaceCount; ++fi) {
                const uint32_t faceIndex = InFaceIndices ? InFaceIndices[fi] : static_cast<uint32_t>(fi);
                ufbx_face face = InUMesh->faces.data[faceIndex];
                const uint32_t numTri = face.num_indices >= 3 ? face.num_indices - 2 : 0;
                for (uint32_t ti = 0; ti < numTri; ++ti) {
                    const uint32_t cornerIndices[3] = {0, ti + 1, ti + 2};
                    for (int k = 0; k < 3; ++k) {
                        const uint32_t indexInMesh = face.index_begin + cornerIndices[k];
                        indices.push_back(static_cast<uint32_t>(verts.size()));
                        verts.push_back(MakeFbxVertex(InUMesh, indexInMesh, InLocalMat, InNormMat, bFlipUVs));
                    }
                }
            }
            InOutSubmesh.IndexCount = static_cast<uint32_t>(indices.size() - InOutSubmesh.IndexOffset);
            InOutSubmesh.VertexCount = static_cast<uint32_t>(verts.size() - vertexStart);
        }

        bool FillStaticMeshFromFbxNode(UStaticMesh& InMesh, const ufbx_node* InNode,
                                       const FMeshImportSettings& InSettings,
                                       const std::unordered_map<const ufbx_material*, uint32_t>& InMatToSlot) {
            if (!InNode || !InNode->mesh)
                return false;
            const ufbx_mesh* uMesh = InNode->mesh;
            if (uMesh->num_faces == 0 || uMesh->num_triangles == 0)
                return false;

            const std::string submeshName =
                InNode->name.data ? std::string(InNode->name.data, InNode->name.length) : InMesh.GetName();

            glm::mat4 localMat(1.0f);
            for (int c = 0; c < 4; ++c) {
                for (int r = 0; r < 3; ++r)
                    localMat[c][r] = static_cast<float>(InNode->node_to_world.cols[c].v[r]);
            }
            const glm::mat3 normMat = glm::transpose(glm::inverse(glm::mat3(localMat)));

            bool bAdded = false;
            if (uMesh->material_parts.count > 0) {
                for (size_t pi = 0; pi < uMesh->material_parts.count; ++pi) {
                    const ufbx_mesh_part& part = uMesh->material_parts.data[pi];
                    if (part.num_triangles == 0)
                        continue;
                    uint32_t slotIdx = 0;
                    if (pi < uMesh->materials.count && uMesh->materials.data[pi]) {
                        const ufbx_material* partMat = uMesh->materials.data[pi];
                        auto it = InMatToSlot.find(partMat);
                        if (it != InMatToSlot.end())
                            slotIdx = it->second;
                    }
                    FStaticSubmesh submesh;
                    submesh.Name = submeshName + (uMesh->material_parts.count > 1 ? ("_" + std::to_string(pi)) : "");
                    submesh.MaterialSlotIndex = slotIdx;
                    submesh.LocalTransform = glm::mat4(1.0f);
                    AppendTriangulatedFaces(InMesh, uMesh, part.face_indices.data, part.num_faces, localMat, normMat,
                                            InSettings.bFlipUVs, submesh);
                    if (submesh.IndexCount > 0) {
                        InMesh.GetSubmeshes().push_back(submesh);
                        bAdded = true;
                    }
                }
            } else {
                FStaticSubmesh submesh;
                submesh.Name = submeshName;
                submesh.MaterialSlotIndex = 0;
                submesh.LocalTransform = glm::mat4(1.0f);
                AppendTriangulatedFaces(InMesh, uMesh, nullptr, uMesh->num_faces, localMat, normMat,
                                        InSettings.bFlipUVs, submesh);
                if (submesh.IndexCount > 0) {
                    InMesh.GetSubmeshes().push_back(submesh);
                    bAdded = true;
                }
            }
            return bAdded;
        }

    } // namespace

    bool FMeshImporter::ImportFBX(const std::string& InSourcePath, const FMeshImportSettings& InSettings,
                                  FMeshImportResult& OutResult) {
        ufbx_load_opts opts = {0};
        opts.target_axes = ufbx_axes_right_handed_y_up;
        opts.target_unit_meters = 1.0f;
        opts.generate_missing_normals = InSettings.bGenerateNormalsIfMissing;
        opts.evaluate_skinning = false;

        ufbx_error error;
        ufbx_scene* scene = ufbx_load_file(InSourcePath.c_str(), &opts, &error);
        if (!scene) {
            char errBuf[512];
            ufbx_format_error(errBuf, sizeof(errBuf), &error);
            OutResult.Errors.push_back(std::string("ufbx error loading ") + InSourcePath + ": " + errBuf);
            LE_CORE_ERROR("FMeshImporter: Failed to load FBX \"{0}\": {1}", InSourcePath, errBuf);
            return false;
        }

        std::string baseMeshName = FAssetPath::GetFileNameWithoutExtension(InSourcePath);

        // 1. Extract Materials
        std::unordered_map<const ufbx_material*, uint32_t> matToSlotIndex;

        for (size_t mi = 0; mi < scene->materials.count; ++mi) {
            const ufbx_material* uMat = scene->materials.data[mi];
            if (!uMat)
                continue;

            FExtractedMaterial mat;
            mat.Name =
                uMat->name.data ? std::string(uMat->name.data, uMat->name.length) : ("Material_" + std::to_string(mi));

            // Extract PBR properties
            if (uMat->pbr.base_color.has_value) {
                mat.BaseColor = glm::vec3(static_cast<float>(uMat->pbr.base_color.value_vec4.x),
                                          static_cast<float>(uMat->pbr.base_color.value_vec4.y),
                                          static_cast<float>(uMat->pbr.base_color.value_vec4.z));
            } else if (uMat->fbx.diffuse_color.has_value) {
                mat.BaseColor = glm::vec3(static_cast<float>(uMat->fbx.diffuse_color.value_vec4.x),
                                          static_cast<float>(uMat->fbx.diffuse_color.value_vec4.y),
                                          static_cast<float>(uMat->fbx.diffuse_color.value_vec4.z));
            }

            if (uMat->pbr.roughness.has_value) {
                mat.Roughness = static_cast<float>(uMat->pbr.roughness.value_real);
            } else if (uMat->fbx.specular_exponent.has_value) {
                float shininess = static_cast<float>(uMat->fbx.specular_exponent.value_real);
                mat.Roughness = glm::clamp(std::sqrt(2.0f / (shininess + 2.0f)), 0.0f, 1.0f);
            }

            if (uMat->pbr.metalness.has_value) {
                mat.Metallic = static_cast<float>(uMat->pbr.metalness.value_real);
            } else if (uMat->fbx.reflection_factor.has_value) {
                mat.Metallic = static_cast<float>(uMat->fbx.reflection_factor.value_real);
            } else if (uMat->fbx.specular_factor.has_value && uMat->fbx.specular_factor.value_real > 0.8) {
                mat.Metallic = static_cast<float>(uMat->fbx.specular_factor.value_real);
            }

            // Custom props
            ufbx_prop* pMetallic = ufbx_find_prop(&uMat->props, "Metallic");
            if (pMetallic && pMetallic->type == UFBX_PROP_NUMBER) {
                mat.Metallic = static_cast<float>(pMetallic->value_real);
            }
            ufbx_prop* pRoughness = ufbx_find_prop(&uMat->props, "Roughness");
            if (pRoughness && pRoughness->type == UFBX_PROP_NUMBER) {
                mat.Roughness = static_cast<float>(pRoughness->value_real);
            }

            if (uMat->pbr.emission_color.has_value && uMat->pbr.emission_factor.has_value &&
                uMat->pbr.emission_factor.value_real > 0.0) {
                mat.EmissiveColor = glm::vec3(static_cast<float>(uMat->pbr.emission_color.value_vec4.x),
                                              static_cast<float>(uMat->pbr.emission_color.value_vec4.y),
                                              static_cast<float>(uMat->pbr.emission_color.value_vec4.z));
                mat.EmissiveIntensity = static_cast<float>(uMat->pbr.emission_factor.value_real);
            } else if (uMat->fbx.emission_color.has_value && uMat->fbx.emission_factor.has_value &&
                       uMat->fbx.emission_factor.value_real > 0.0) {
                mat.EmissiveColor = glm::vec3(static_cast<float>(uMat->fbx.emission_color.value_vec4.x),
                                              static_cast<float>(uMat->fbx.emission_color.value_vec4.y),
                                              static_cast<float>(uMat->fbx.emission_color.value_vec4.z));
                mat.EmissiveIntensity = static_cast<float>(uMat->fbx.emission_factor.value_real);
            }

            // Semantic heuristic fallbacks
            std::string matNameLower = mat.Name;
            std::transform(matNameLower.begin(), matNameLower.end(), matNameLower.begin(), ::tolower);
            if (matNameLower.find("chrome") != std::string::npos) {
                if (mat.Metallic == 0.0f)
                    mat.Metallic = 0.96f;
                if (mat.Roughness >= 0.5f)
                    mat.Roughness = 0.06f;
            } else if (matNameLower.find("glass") != std::string::npos ||
                       matNameLower.find("window") != std::string::npos) {
                if (mat.Roughness >= 0.5f)
                    mat.Roughness = 0.08f;
                if (mat.Metallic == 0.0f)
                    mat.Metallic = 0.2f;
            } else if (matNameLower.find("tire") != std::string::npos ||
                       matNameLower.find("rubber") != std::string::npos) {
                if (mat.Roughness < 0.7f)
                    mat.Roughness = 0.82f;
            } else if (matNameLower.find("headlight") != std::string::npos ||
                       matNameLower.find("taillight") != std::string::npos ||
                       matNameLower.find("emissive") != std::string::npos ||
                       matNameLower.find("neon") != std::string::npos) {
                if (mat.EmissiveIntensity <= 0.0f) {
                    mat.EmissiveIntensity = (matNameLower.find("headlight") != std::string::npos) ? 6.0f : 5.0f;
                    if (mat.EmissiveColor == glm::vec3(0.0f)) {
                        mat.EmissiveColor = mat.BaseColor;
                    }
                }
            }
            if (matNameLower.find("leaf") != std::string::npos || matNameLower.find("leaves") != std::string::npos ||
                matNameLower.find("frond") != std::string::npos || matNameLower.find("foliage") != std::string::npos ||
                matNameLower.find("plant") != std::string::npos) {
                mat.bDoubleSided = true;
            }

            // Extract textures from material maps
            if (uMat->pbr.base_color.texture && uMat->pbr.base_color.texture->filename.data) {
                mat.DiffuseTextureName = FAssetPath::GetFileName(std::string(
                    uMat->pbr.base_color.texture->filename.data, uMat->pbr.base_color.texture->filename.length));
                OutResult.ReferencedTextureNames.push_back(mat.DiffuseTextureName);
            }
            if (uMat->pbr.normal_map.texture && uMat->pbr.normal_map.texture->filename.data) {
                mat.NormalTextureName = FAssetPath::GetFileName(std::string(
                    uMat->pbr.normal_map.texture->filename.data, uMat->pbr.normal_map.texture->filename.length));
                OutResult.ReferencedTextureNames.push_back(mat.NormalTextureName);
            }
            if (uMat->pbr.roughness.texture && uMat->pbr.roughness.texture->filename.data) {
                mat.RoughnessTextureName = FAssetPath::GetFileName(std::string(
                    uMat->pbr.roughness.texture->filename.data, uMat->pbr.roughness.texture->filename.length));
                OutResult.ReferencedTextureNames.push_back(mat.RoughnessTextureName);
            }
            if (uMat->pbr.metalness.texture && uMat->pbr.metalness.texture->filename.data) {
                mat.MetallicTextureName = FAssetPath::GetFileName(std::string(
                    uMat->pbr.metalness.texture->filename.data, uMat->pbr.metalness.texture->filename.length));
                OutResult.ReferencedTextureNames.push_back(mat.MetallicTextureName);
            }
            if (uMat->pbr.emission_color.texture && uMat->pbr.emission_color.texture->filename.data) {
                mat.EmissiveTextureName =
                    FAssetPath::GetFileName(std::string(uMat->pbr.emission_color.texture->filename.data,
                                                        uMat->pbr.emission_color.texture->filename.length));
                OutResult.ReferencedTextureNames.push_back(mat.EmissiveTextureName);
            }
            if (uMat->pbr.ambient_occlusion.texture && uMat->pbr.ambient_occlusion.texture->filename.data) {
                mat.AOTextureName =
                    FAssetPath::GetFileName(std::string(uMat->pbr.ambient_occlusion.texture->filename.data,
                                                        uMat->pbr.ambient_occlusion.texture->filename.length));
                OutResult.ReferencedTextureNames.push_back(mat.AOTextureName);
            }

            OutResult.ExtractedMaterials.push_back(mat);
            matToSlotIndex[uMat] = static_cast<uint32_t>(OutResult.ExtractedMaterials.size() - 1);
        }

        // Mixamo animation FBX files often have bones + takes but no skin deformer.
        const bool bSkeletal = scene->skin_deformers.count > 0 || scene->bones.count > 0;
        if (bSkeletal) {
            if (!FSkeletalImporter::ImportFromScene(scene, InSettings, InSourcePath, OutResult) ||
                !OutResult.Skeleton) {
                ufbx_free_scene(scene);
                return false;
            }
            FSkeletalImporter::ImportSkeletalGeometry(scene, InSettings, OutResult.Skeleton, OutResult);
            FSkeletalImporter::ImportAnimationsFromScene(scene, OutResult.Skeleton, InSourcePath, OutResult);
            if (OutResult.SkeletalMesh) {
                auto& slots = OutResult.SkeletalMesh->GetMaterialSlots();
                for (size_t i = 0; i < slots.size() && i < OutResult.ExtractedMaterials.size(); ++i) {
                    slots[i].SlotName = OutResult.ExtractedMaterials[i].Name;
                    std::string formatted =
                        (slots[i].SlotName.rfind("M_", 0) == 0) ? slots[i].SlotName : ("M_" + slots[i].SlotName);
                    slots[i].DefaultMaterialPath = "Materials/" + formatted + ".lmat";
                }
            }
            ufbx_free_scene(scene);
            LE_CORE_INFO("FMeshImporter: Imported skeletal FBX \"{0}\" ({1} bones, {2} verts, {3} anims)", InSourcePath,
                         OutResult.Skeleton->GetNumBones(),
                         OutResult.SkeletalMesh ? OutResult.SkeletalMesh->GetVertices().size() : 0,
                         OutResult.Animations.size());
            return OutResult.SkeletalMesh != nullptr || !OutResult.Animations.empty() || OutResult.Skeleton != nullptr;
        }

        std::vector<const ufbx_node*> meshNodes;
        meshNodes.reserve(scene->nodes.count);
        for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
            const ufbx_node* node = scene->nodes.data[ni];
            if (node && node->mesh && node->mesh->num_faces > 0 && node->mesh->num_triangles > 0)
                meshNodes.push_back(node);
        }

        const bool bSplit = InSettings.bSplitStaticMeshes && meshNodes.size() > 1;
        auto finishMesh = [&](UStaticMesh& mesh) {
            if (InSettings.bGenerateTangents)
                GenerateLengyelTangents(mesh.GetVertices(), mesh.GetIndices());
            mesh.CalculateBounds();
        };

        if (bSplit) {
            std::unordered_set<std::string> usedNames;
            for (size_t i = 0; i < meshNodes.size(); ++i) {
                const ufbx_node* node = meshNodes[i];
                std::string nodeName =
                    node->name.data ? std::string(node->name.data, node->name.length) : ("Mesh_" + std::to_string(i));
                nodeName = SanitizeMeshToken(nodeName);
                std::string unique = nodeName;
                int suffix = 2;
                while (!usedNames.insert(unique).second) {
                    unique = nodeName + "_" + std::to_string(suffix++);
                }
                const std::string meshName = baseMeshName + "_" + unique;
                auto piece = UStaticMesh::Create(meshName);
                piece->SetUUID(FUUID::FromPath(meshName));
                CopyMaterialSlots(*piece, OutResult.ExtractedMaterials);
                if (!FillStaticMeshFromFbxNode(*piece, node, InSettings, matToSlotIndex))
                    continue;
                finishMesh(*piece);
                OutResult.SeparateMeshes.push_back(piece);
            }
            ufbx_free_scene(scene);
            if (OutResult.SeparateMeshes.empty()) {
                OutResult.Errors.push_back("FBX contained mesh nodes but none produced geometry: " + InSourcePath);
                return false;
            }
            OutResult.StaticMesh = OutResult.SeparateMeshes.front();
            LE_CORE_INFO("FMeshImporter: Split \"{0}\" into {1} static meshes", InSourcePath,
                         OutResult.SeparateMeshes.size());
            return true;
        }

        auto staticMesh = UStaticMesh::Create(baseMeshName);
        staticMesh->SetUUID(FUUID::FromPath(baseMeshName));
        CopyMaterialSlots(*staticMesh, OutResult.ExtractedMaterials);
        for (const ufbx_node* node : meshNodes)
            FillStaticMeshFromFbxNode(*staticMesh, node, InSettings, matToSlotIndex);
        finishMesh(*staticMesh);
        ufbx_free_scene(scene);

        OutResult.StaticMesh = staticMesh;
        LE_CORE_INFO("FMeshImporter: Successfully imported \"{0}\" ({1} vertices, {2} indices, {3} submeshes)",
                     InSourcePath, staticMesh->GetVertices().size(), staticMesh->GetIndices().size(),
                     staticMesh->GetSubmeshes().size());
        return true;
    }

    std::string FMeshImporter::ResolveTextureReference(const std::string& InMaterialName, const std::string& InSlotType,
                                                       const std::vector<std::string>& InAvailableTextureFiles) {
        std::string matLower = InMaterialName;
        std::transform(matLower.begin(), matLower.end(), matLower.begin(), ::tolower);

        std::string slotLower = InSlotType;
        std::transform(slotLower.begin(), slotLower.end(), slotLower.begin(), ::tolower);

        // Try exact substring match first
        for (const auto& file : InAvailableTextureFiles) {
            std::string fileLower = file;
            std::transform(fileLower.begin(), fileLower.end(), fileLower.begin(), ::tolower);

            // If file contains material identifier and slot suffix
            if (fileLower.find(matLower) != std::string::npos && fileLower.find(slotLower) != std::string::npos) {
                return file;
            }
        }

        // Try fallback fuzzy match on tokens (e.g. "material_slot_tokens")
        return "";
    }

} // namespace Leon
