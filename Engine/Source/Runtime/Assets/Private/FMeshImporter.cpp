#include "Assets/FMeshImporter.hpp"
#include "Assets/FAssetPath.hpp"
#include "Core/FLog.hpp"
#include <ufbx.h>

#include <algorithm>
#include <unordered_map>

namespace Leon {

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
        auto staticMesh = UStaticMesh::Create(baseMeshName);
        staticMesh->SetUUID(FUUID::FromPath(baseMeshName));

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

            FStaticMaterialSlot slot;
            slot.SlotName = mat.Name;
            std::string formattedMatName = (mat.Name.rfind("M_", 0) == 0) ? mat.Name : ("M_" + mat.Name);
            slot.DefaultMaterialPath = "Materials/" + formattedMatName + ".lmat";
            uint32_t slotIdx = static_cast<uint32_t>(staticMesh->GetMaterialSlots().size());
            staticMesh->GetMaterialSlots().push_back(slot);
            matToSlotIndex[uMat] = slotIdx;
        }

        if (staticMesh->GetMaterialSlots().empty()) {
            FStaticMaterialSlot defSlot;
            defSlot.SlotName = "DefaultMaterial";
            defSlot.DefaultMaterialPath = "Materials/M_DefaultPBR.lmat";
            staticMesh->GetMaterialSlots().push_back(defSlot);
        }

        // 2. Extract Geometry (Iterate through all nodes / meshes in the FBX)
        std::vector<FStaticMeshVertex>& allVertices = staticMesh->GetVertices();
        std::vector<uint32_t>& allIndices = staticMesh->GetIndices();

        for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
            const ufbx_node* node = scene->nodes.data[ni];
            if (!node || !node->mesh)
                continue;

            const ufbx_mesh* uMesh = node->mesh;
            if (uMesh->num_faces == 0 || uMesh->num_triangles == 0)
                continue;

            std::string submeshName =
                node->name.data ? std::string(node->name.data, node->name.length) : ("Submesh_" + std::to_string(ni));

            // Node local transform matrix
            glm::mat4 localMat(1.0f);
            for (int c = 0; c < 4; ++c) {
                for (int r = 0; r < 3; ++r) {
                    localMat[c][r] = static_cast<float>(node->node_to_world.cols[c].v[r]);
                }
            }
            glm::mat3 normMat = glm::transpose(glm::inverse(glm::mat3(localMat)));

            // If mesh has multiple material parts, split by material part
            if (uMesh->material_parts.count > 0) {
                for (size_t pi = 0; pi < uMesh->material_parts.count; ++pi) {
                    const ufbx_mesh_part& part = uMesh->material_parts.data[pi];
                    if (part.num_triangles == 0)
                        continue;

                    uint32_t slotIdx = 0;
                    if (pi < uMesh->materials.count && uMesh->materials.data[pi]) {
                        const ufbx_material* partMat = uMesh->materials.data[pi];
                        if (matToSlotIndex.find(partMat) != matToSlotIndex.end()) {
                            slotIdx = matToSlotIndex[partMat];
                        }
                    }

                    FStaticSubmesh submesh;
                    submesh.Name = submeshName + (uMesh->material_parts.count > 1 ? ("_" + std::to_string(pi)) : "");
                    submesh.IndexOffset = static_cast<uint32_t>(allIndices.size());
                    submesh.VertexOffset = static_cast<uint32_t>(allVertices.size());
                    submesh.MaterialSlotIndex = slotIdx;
                    submesh.LocalTransform = glm::mat4(1.0f);

                    uint32_t submeshVertexStart = static_cast<uint32_t>(allVertices.size());

                    for (size_t fi = 0; fi < part.num_faces; ++fi) {
                        uint32_t faceIndex = part.face_indices.data[fi];
                        ufbx_face face = uMesh->faces.data[faceIndex];
                        uint32_t numTri = face.num_indices - 2;

                        for (uint32_t ti = 0; ti < numTri; ++ti) {
                            uint32_t cornerIndices[3] = {0, ti + 1, ti + 2};

                            for (int k = 0; k < 3; ++k) {
                                uint32_t indexInFace = cornerIndices[k];
                                uint32_t indexInMesh = face.index_begin + indexInFace;

                                FStaticMeshVertex v;

                                // Position
                                if (uMesh->vertex_position.exists) {
                                    ufbx_vec3 p = ufbx_get_vertex_vec3(&uMesh->vertex_position, indexInMesh);
                                    glm::vec4 localPos(static_cast<float>(p.x), static_cast<float>(p.y),
                                                       static_cast<float>(p.z), 1.0f);
                                    glm::vec4 worldPos = localMat * localPos;
                                    v.Position = glm::vec3(worldPos);
                                }

                                // Normal
                                if (uMesh->vertex_normal.exists) {
                                    ufbx_vec3 n = ufbx_get_vertex_vec3(&uMesh->vertex_normal, indexInMesh);
                                    v.Normal = glm::normalize(normMat * glm::vec3(static_cast<float>(n.x),
                                                                                  static_cast<float>(n.y),
                                                                                  static_cast<float>(n.z)));
                                }

                                // UV
                                if (uMesh->vertex_uv.exists) {
                                    ufbx_vec2 uv = ufbx_get_vertex_vec2(&uMesh->vertex_uv, indexInMesh);
                                    v.TexCoord = glm::vec2(static_cast<float>(uv.x),
                                                           InSettings.bFlipUVs ? (1.0f - static_cast<float>(uv.y))
                                                                               : static_cast<float>(uv.y));
                                }

                                glm::vec3 tangent{1.0f, 0.0f, 0.0f};
                                glm::vec3 bitangent = glm::cross(v.Normal, tangent);
                                if (uMesh->vertex_tangent.exists) {
                                    ufbx_vec3 t = ufbx_get_vertex_vec3(&uMesh->vertex_tangent, indexInMesh);
                                    tangent = glm::normalize(normMat * glm::vec3(static_cast<float>(t.x),
                                                                                   static_cast<float>(t.y),
                                                                                   static_cast<float>(t.z)));
                                }
                                if (uMesh->vertex_bitangent.exists) {
                                    ufbx_vec3 b = ufbx_get_vertex_vec3(&uMesh->vertex_bitangent, indexInMesh);
                                    bitangent = glm::normalize(normMat * glm::vec3(static_cast<float>(b.x),
                                                                                     static_cast<float>(b.y),
                                                                                     static_cast<float>(b.z)));
                                } else {
                                    bitangent = glm::normalize(glm::cross(v.Normal, tangent));
                                }
                                v.Tangent = PackTangent(tangent, v.Normal, bitangent);
                                v.LightmapUV = v.TexCoord;

                                // Color
                                if (uMesh->vertex_color.exists) {
                                    ufbx_vec4 col = ufbx_get_vertex_vec4(&uMesh->vertex_color, indexInMesh);
                                    v.Color = glm::vec3(static_cast<float>(col.x), static_cast<float>(col.y),
                                                        static_cast<float>(col.z));
                                } else {
                                    v.Color = glm::vec3(1.0f);
                                }

                                allIndices.push_back(static_cast<uint32_t>(allVertices.size()));
                                allVertices.push_back(v);
                            }
                        }
                    }

                    submesh.IndexCount = static_cast<uint32_t>(allIndices.size() - submesh.IndexOffset);
                    submesh.VertexCount = static_cast<uint32_t>(allVertices.size() - submeshVertexStart);
                    if (submesh.IndexCount > 0) {
                        staticMesh->GetSubmeshes().push_back(submesh);
                    }
                }
            } else {
                // Single submesh for this node
                FStaticSubmesh submesh;
                submesh.Name = submeshName;
                submesh.IndexOffset = static_cast<uint32_t>(allIndices.size());
                submesh.VertexOffset = static_cast<uint32_t>(allVertices.size());
                submesh.MaterialSlotIndex = 0;
                submesh.LocalTransform = glm::mat4(1.0f);

                uint32_t submeshVertexStart = static_cast<uint32_t>(allVertices.size());

                for (size_t fi = 0; fi < uMesh->num_faces; ++fi) {
                    ufbx_face face = uMesh->faces.data[fi];
                    uint32_t numTri = face.num_indices - 2;

                    for (uint32_t ti = 0; ti < numTri; ++ti) {
                        uint32_t cornerIndices[3] = {0, ti + 1, ti + 2};

                        for (int k = 0; k < 3; ++k) {
                            uint32_t indexInFace = cornerIndices[k];
                            uint32_t indexInMesh = face.index_begin + indexInFace;

                            FStaticMeshVertex v;

                            if (uMesh->vertex_position.exists) {
                                ufbx_vec3 p = ufbx_get_vertex_vec3(&uMesh->vertex_position, indexInMesh);
                                glm::vec4 localPos(static_cast<float>(p.x), static_cast<float>(p.y),
                                                   static_cast<float>(p.z), 1.0f);
                                glm::vec4 worldPos = localMat * localPos;
                                v.Position = glm::vec3(worldPos);
                            }

                            if (uMesh->vertex_normal.exists) {
                                ufbx_vec3 n = ufbx_get_vertex_vec3(&uMesh->vertex_normal, indexInMesh);
                                v.Normal =
                                    glm::normalize(normMat * glm::vec3(static_cast<float>(n.x), static_cast<float>(n.y),
                                                                       static_cast<float>(n.z)));
                            }

                            if (uMesh->vertex_uv.exists) {
                                ufbx_vec2 uv = ufbx_get_vertex_vec2(&uMesh->vertex_uv, indexInMesh);
                                v.TexCoord = glm::vec2(static_cast<float>(uv.x), InSettings.bFlipUVs
                                                                                     ? (1.0f - static_cast<float>(uv.y))
                                                                                     : static_cast<float>(uv.y));
                            }

                            glm::vec3 tangent{1.0f, 0.0f, 0.0f};
                            glm::vec3 bitangent = glm::cross(v.Normal, tangent);
                            if (uMesh->vertex_tangent.exists) {
                                ufbx_vec3 t = ufbx_get_vertex_vec3(&uMesh->vertex_tangent, indexInMesh);
                                tangent =
                                    glm::normalize(normMat * glm::vec3(static_cast<float>(t.x), static_cast<float>(t.y),
                                                                       static_cast<float>(t.z)));
                            }
                            if (uMesh->vertex_bitangent.exists) {
                                ufbx_vec3 b = ufbx_get_vertex_vec3(&uMesh->vertex_bitangent, indexInMesh);
                                bitangent =
                                    glm::normalize(normMat * glm::vec3(static_cast<float>(b.x), static_cast<float>(b.y),
                                                                       static_cast<float>(b.z)));
                            } else {
                                bitangent = glm::normalize(glm::cross(v.Normal, tangent));
                            }
                            v.Tangent = PackTangent(tangent, v.Normal, bitangent);
                            v.LightmapUV = v.TexCoord;

                            if (uMesh->vertex_color.exists) {
                                ufbx_vec4 col = ufbx_get_vertex_vec4(&uMesh->vertex_color, indexInMesh);
                                v.Color = glm::vec3(static_cast<float>(col.x), static_cast<float>(col.y),
                                                    static_cast<float>(col.z));
                            } else {
                                v.Color = glm::vec3(1.0f);
                            }

                            allIndices.push_back(static_cast<uint32_t>(allVertices.size()));
                            allVertices.push_back(v);
                        }
                    }
                }

                submesh.IndexCount = static_cast<uint32_t>(allIndices.size() - submesh.IndexOffset);
                submesh.VertexCount = static_cast<uint32_t>(allVertices.size() - submeshVertexStart);
                if (submesh.IndexCount > 0) {
                    staticMesh->GetSubmeshes().push_back(submesh);
                }
            }
        }

        staticMesh->CalculateBounds();
        ufbx_free_scene(scene);

        OutResult.StaticMesh = staticMesh;
        LE_CORE_INFO("FMeshImporter: Successfully imported \"{0}\" ({1} vertices, {2} indices, {3} submeshes)",
                     InSourcePath, allVertices.size(), allIndices.size(), staticMesh->GetSubmeshes().size());
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
