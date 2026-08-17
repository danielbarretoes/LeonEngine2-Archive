#include "Assets/FSkeletalImporter.hpp"
#include "Assets/FAssetPath.hpp"
#include "Assets/FAnimTypes.hpp"
#include "Core/FLog.hpp"
#include "Renderer/FVertexLayout.hpp"

#include <ufbx.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Leon {

    namespace {

        glm::mat4 UfbxMatrixToGlm(const ufbx_matrix& InM) {
            glm::mat4 r(1.0f);
            r[0] =
                glm::vec4(static_cast<float>(InM.m00), static_cast<float>(InM.m10), static_cast<float>(InM.m20), 0.0f);
            r[1] =
                glm::vec4(static_cast<float>(InM.m01), static_cast<float>(InM.m11), static_cast<float>(InM.m21), 0.0f);
            r[2] =
                glm::vec4(static_cast<float>(InM.m02), static_cast<float>(InM.m12), static_cast<float>(InM.m22), 0.0f);
            r[3] =
                glm::vec4(static_cast<float>(InM.m03), static_cast<float>(InM.m13), static_cast<float>(InM.m23), 1.0f);
            return r;
        }

        FBoneTransform UfbxTransformToBone(const ufbx_transform& InT) {
            FBoneTransform out;
            out.Translation = glm::vec3(static_cast<float>(InT.translation.x), static_cast<float>(InT.translation.y),
                                        static_cast<float>(InT.translation.z));
            out.Rotation =
                glm::normalize(glm::quat(static_cast<float>(InT.rotation.w), static_cast<float>(InT.rotation.x),
                                         static_cast<float>(InT.rotation.y), static_cast<float>(InT.rotation.z)));
            out.Scale = glm::vec3(static_cast<float>(InT.scale.x), static_cast<float>(InT.scale.y),
                                  static_cast<float>(InT.scale.z));
            return out;
        }

        std::string NodeName(const ufbx_node* InNode, size_t InFallback) {
            if (InNode && InNode->name.data && InNode->name.length > 0)
                return std::string(InNode->name.data, InNode->name.length);
            return "Bone_" + std::to_string(InFallback);
        }

        bool IsSceneRoot(const ufbx_node* InNode) {
            return !InNode || !InNode->parent;
        }

        std::vector<const ufbx_node*> CollectOrderedSkeletonNodes(ufbx_scene* InScene) {
            std::unordered_set<const ufbx_node*> marked;
            for (size_t ci = 0; ci < InScene->skin_clusters.count; ++ci) {
                const ufbx_skin_cluster* cluster = InScene->skin_clusters.data[ci];
                if (!cluster || !cluster->bone_node)
                    continue;
                const ufbx_node* node = cluster->bone_node;
                while (node && !IsSceneRoot(node)) {
                    marked.insert(node);
                    node = node->parent;
                }
            }
            if (marked.empty()) {
                for (size_t ni = 0; ni < InScene->nodes.count; ++ni) {
                    const ufbx_node* node = InScene->nodes.data[ni];
                    if (node && node->bone && !IsSceneRoot(node))
                        marked.insert(node);
                }
            }

            std::vector<const ufbx_node*> sceneOrder;
            sceneOrder.reserve(marked.size());
            for (size_t ni = 0; ni < InScene->nodes.count; ++ni) {
                const ufbx_node* node = InScene->nodes.data[ni];
                if (node && marked.count(node))
                    sceneOrder.push_back(node);
            }

            std::vector<const ufbx_node*> ordered;
            ordered.reserve(sceneOrder.size());
            std::unordered_set<const ufbx_node*> placed;
            while (placed.size() < sceneOrder.size()) {
                bool bProgress = false;
                for (const ufbx_node* node : sceneOrder) {
                    if (placed.count(node))
                        continue;
                    bool parentReady = !node->parent || !marked.count(node->parent) || placed.count(node->parent);
                    if (parentReady) {
                        ordered.push_back(node);
                        placed.insert(node);
                        bProgress = true;
                    }
                }
                if (!bProgress) {
                    for (const ufbx_node* node : sceneOrder) {
                        if (!placed.count(node)) {
                            ordered.push_back(node);
                            placed.insert(node);
                        }
                    }
                }
            }
            return ordered;
        }

        TRef<USkeleton> BuildSkeleton(ufbx_scene* InScene, const std::string& InName,
                                      std::vector<std::string>& OutErrors) {
            std::vector<const ufbx_node*> ordered = CollectOrderedSkeletonNodes(InScene);
            if (ordered.empty()) {
                OutErrors.push_back("No skeleton nodes found in FBX");
                return nullptr;
            }

            if (ordered.size() > kMaxBones)
                LE_CORE_WARN("FSkeletalImporter: Skeleton has {0} bones; GPU palette is capped at {1}", ordered.size(),
                             kMaxBones);

            auto skeleton = USkeleton::Create(InName);
            skeleton->SetUUID(FUUID::FromPath(InName));
            auto& bones = skeleton->GetBones();
            bones.resize(ordered.size());

            std::unordered_map<const ufbx_node*, int32_t> nodeToIndex;
            for (size_t i = 0; i < ordered.size(); ++i)
                nodeToIndex[ordered[i]] = static_cast<int32_t>(i);

            std::unordered_map<const ufbx_node*, glm::mat4> geometryToBone;
            glm::mat4 meshGeomToWorld(1.0f);
            bool bHaveMeshGeom = false;
            for (size_t ni = 0; ni < InScene->nodes.count; ++ni) {
                const ufbx_node* node = InScene->nodes.data[ni];
                if (!node || !node->mesh || node->mesh->skin_deformers.count == 0)
                    continue;
                meshGeomToWorld = UfbxMatrixToGlm(node->geometry_to_world);
                bHaveMeshGeom = true;
                const ufbx_skin_deformer* skin = node->mesh->skin_deformers.data[0];
                if (!skin)
                    continue;
                for (size_t ci = 0; ci < skin->clusters.count; ++ci) {
                    const ufbx_skin_cluster* cluster = skin->clusters.data[ci];
                    if (cluster && cluster->bone_node)
                        geometryToBone[cluster->bone_node] = UfbxMatrixToGlm(cluster->geometry_to_bone);
                }
                break;
            }

            glm::mat4 invMeshGeom = bHaveMeshGeom ? glm::inverse(meshGeomToWorld) : glm::mat4(1.0f);

            for (size_t i = 0; i < ordered.size(); ++i) {
                const ufbx_node* node = ordered[i];
                FSkeletonBone& bone = bones[i];
                bone.Name = NodeName(node, i);
                auto parentIt = nodeToIndex.find(node->parent);
                bone.ParentIndex = (parentIt != nodeToIndex.end()) ? parentIt->second : -1;
                bone.RestLocal = UfbxTransformToBone(node->local_transform);

                auto gtb = geometryToBone.find(node);
                if (gtb != geometryToBone.end()) {
                    bone.InverseBindPose = gtb->second * invMeshGeom;
                } else {
                    bone.InverseBindPose = glm::mat4(1.0f);
                }
            }

            std::vector<glm::mat4> restComponent(bones.size(), glm::mat4(1.0f));
            for (size_t i = 0; i < bones.size(); ++i) {
                glm::mat4 local = bones[i].RestLocal.ToMatrix();
                if (bones[i].ParentIndex >= 0)
                    restComponent[i] = restComponent[static_cast<size_t>(bones[i].ParentIndex)] * local;
                else
                    restComponent[i] = local;
            }
            for (size_t i = 0; i < bones.size(); ++i) {
                if (geometryToBone.find(ordered[i]) == geometryToBone.end())
                    bones[i].InverseBindPose = glm::inverse(restComponent[i]);
            }

            // Vertices are stored in mesh world metres (geometry_to_world). Inverse-bind from
            // clusters already lives in that space. Rebuild RestLocal from inverse(IB) so the
            // rest palette is identity — Mixamo keeps bone locals in centimetres otherwise.
            for (size_t i = 0; i < bones.size(); ++i)
                restComponent[i] = glm::inverse(bones[i].InverseBindPose);
            for (size_t i = 0; i < bones.size(); ++i) {
                if (bones[i].ParentIndex >= 0)
                    bones[i].RestLocal = FBoneTransform::FromMatrix(
                        glm::inverse(restComponent[static_cast<size_t>(bones[i].ParentIndex)]) * restComponent[i]);
                else
                    bones[i].RestLocal = FBoneTransform::FromMatrix(restComponent[i]);
            }

            skeleton->RebuildLookup();
            return skeleton;
        }

        void AssignInfluences(FSkinnedMeshVertex& OutV, const ufbx_skin_deformer* InSkin, uint32_t InVertexIndex,
                              const std::unordered_map<const ufbx_node*, int32_t>& InBoneIndex) {
            OutV.BoneIndices = glm::ivec4(0);
            OutV.BoneWeights = glm::vec4(0.0f);
            if (!InSkin || InVertexIndex >= InSkin->vertices.count)
                return;

            const ufbx_skin_vertex& sv = InSkin->vertices.data[InVertexIndex];
            struct FInf {
                int32_t Bone = 0;
                float Weight = 0.0f;
            };
            std::vector<FInf> inf;
            inf.reserve(sv.num_weights);
            for (uint32_t w = 0; w < sv.num_weights; ++w) {
                const ufbx_skin_weight& sw = InSkin->weights.data[sv.weight_begin + w];
                if (sw.cluster_index >= InSkin->clusters.count)
                    continue;
                const ufbx_skin_cluster* cluster = InSkin->clusters.data[sw.cluster_index];
                if (!cluster || !cluster->bone_node)
                    continue;
                auto it = InBoneIndex.find(cluster->bone_node);
                if (it == InBoneIndex.end())
                    continue;
                inf.push_back({it->second, static_cast<float>(sw.weight)});
            }
            std::sort(inf.begin(), inf.end(), [](const FInf& a, const FInf& b) { return a.Weight > b.Weight; });
            if (inf.size() > kMaxBoneInfluences)
                inf.resize(kMaxBoneInfluences);
            float sum = 0.0f;
            for (const auto& e : inf)
                sum += e.Weight;
            if (sum < 1e-8f) {
                OutV.BoneWeights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                return;
            }
            for (size_t i = 0; i < inf.size(); ++i) {
                OutV.BoneIndices[static_cast<int>(i)] = inf[i].Bone;
                OutV.BoneWeights[static_cast<int>(i)] = inf[i].Weight / sum;
            }
        }

        void ExtractSkinnedMesh(ufbx_scene* InScene, const FMeshImportSettings& InSettings,
                                const TRef<USkeleton>& InSkeleton,
                                const std::unordered_map<const ufbx_material*, uint32_t>& InMatToSlot,
                                FMeshImportResult& OutResult) {
            auto mesh = USkeletalMesh::Create(InSkeleton->GetName());
            mesh->SetSkeleton(InSkeleton);
            mesh->SetUUID(FUUID::FromPath(InSkeleton->GetName() + ".skm"));

            std::vector<const ufbx_node*> ordered = CollectOrderedSkeletonNodes(InScene);
            std::unordered_map<const ufbx_node*, int32_t> boneIndex;
            for (size_t i = 0; i < ordered.size(); ++i)
                boneIndex[ordered[i]] = static_cast<int32_t>(i);

            auto& verts = mesh->GetVertices();
            auto& indices = mesh->GetIndices();

            for (size_t ni = 0; ni < InScene->nodes.count; ++ni) {
                const ufbx_node* node = InScene->nodes.data[ni];
                if (!node || !node->mesh)
                    continue;
                const ufbx_mesh* uMesh = node->mesh;
                if (uMesh->num_faces == 0 || uMesh->num_triangles == 0)
                    continue;
                if (uMesh->skin_deformers.count == 0)
                    continue;

                const ufbx_skin_deformer* skin = uMesh->skin_deformers.data[0];
                glm::mat4 geomToWorld = UfbxMatrixToGlm(node->geometry_to_world);
                glm::mat3 normMat = glm::transpose(glm::inverse(glm::mat3(geomToWorld)));

                auto emitCorner = [&](uint32_t indexInMesh, FSkinnedMeshVertex& v) {
                    if (uMesh->vertex_position.exists) {
                        ufbx_vec3 p = ufbx_get_vertex_vec3(&uMesh->vertex_position, indexInMesh);
                        glm::vec4 local(static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z),
                                        1.0f);
                        v.Position = glm::vec3(geomToWorld * local);
                    }
                    if (uMesh->vertex_normal.exists) {
                        ufbx_vec3 n = ufbx_get_vertex_vec3(&uMesh->vertex_normal, indexInMesh);
                        v.Normal = glm::normalize(normMat * glm::vec3(static_cast<float>(n.x), static_cast<float>(n.y),
                                                                      static_cast<float>(n.z)));
                    }
                    if (uMesh->vertex_uv.exists) {
                        ufbx_vec2 uv = ufbx_get_vertex_vec2(&uMesh->vertex_uv, indexInMesh);
                        v.TexCoord =
                            glm::vec2(static_cast<float>(uv.x), InSettings.bFlipUVs ? (1.0f - static_cast<float>(uv.y))
                                                                                    : static_cast<float>(uv.y));
                    }
                    glm::vec3 tangent{1.0f, 0.0f, 0.0f};
                    glm::vec3 bitangent = glm::cross(v.Normal, tangent);
                    if (uMesh->vertex_tangent.exists) {
                        ufbx_vec3 t = ufbx_get_vertex_vec3(&uMesh->vertex_tangent, indexInMesh);
                        tangent = glm::normalize(normMat * glm::vec3(static_cast<float>(t.x), static_cast<float>(t.y),
                                                                     static_cast<float>(t.z)));
                    }
                    if (uMesh->vertex_bitangent.exists) {
                        ufbx_vec3 b = ufbx_get_vertex_vec3(&uMesh->vertex_bitangent, indexInMesh);
                        bitangent = glm::normalize(normMat * glm::vec3(static_cast<float>(b.x), static_cast<float>(b.y),
                                                                       static_cast<float>(b.z)));
                    } else {
                        bitangent = glm::normalize(glm::cross(v.Normal, tangent));
                    }
                    v.Tangent = PackTangent(tangent, v.Normal, bitangent);
                    v.LightmapUV = glm::vec2(0.0f);
                    if (uMesh->vertex_color.exists) {
                        ufbx_vec4 col = ufbx_get_vertex_vec4(&uMesh->vertex_color, indexInMesh);
                        v.Color =
                            glm::vec3(static_cast<float>(col.x), static_cast<float>(col.y), static_cast<float>(col.z));
                    } else {
                        v.Color = glm::vec3(1.0f);
                    }
                    uint32_t vertIx = 0;
                    if (uMesh->vertex_indices.count > indexInMesh)
                        vertIx = uMesh->vertex_indices.data[indexInMesh];
                    AssignInfluences(v, skin, vertIx, boneIndex);
                };

                auto appendFacePart = [&](const uint32_t* faceIndices, size_t numFaces, uint32_t slotIdx,
                                          const std::string& subName) {
                    FSkeletalSubmesh submesh;
                    submesh.Name = subName;
                    submesh.IndexOffset = static_cast<uint32_t>(indices.size());
                    submesh.VertexOffset = static_cast<uint32_t>(verts.size());
                    submesh.MaterialSlotIndex = slotIdx;
                    submesh.LocalTransform = glm::mat4(1.0f);
                    uint32_t startV = static_cast<uint32_t>(verts.size());

                    for (size_t fi = 0; fi < numFaces; ++fi) {
                        uint32_t faceIndex = faceIndices ? faceIndices[fi] : static_cast<uint32_t>(fi);
                        ufbx_face face = uMesh->faces.data[faceIndex];
                        uint32_t numTri = face.num_indices >= 3 ? face.num_indices - 2 : 0;
                        for (uint32_t ti = 0; ti < numTri; ++ti) {
                            uint32_t corners[3] = {0, ti + 1, ti + 2};
                            for (int k = 0; k < 3; ++k) {
                                FSkinnedMeshVertex v;
                                emitCorner(face.index_begin + corners[k], v);
                                indices.push_back(static_cast<uint32_t>(verts.size()));
                                verts.push_back(v);
                            }
                        }
                    }
                    submesh.IndexCount = static_cast<uint32_t>(indices.size() - submesh.IndexOffset);
                    submesh.VertexCount = static_cast<uint32_t>(verts.size() - startV);
                    if (submesh.IndexCount > 0)
                        mesh->GetSubmeshes().push_back(submesh);
                };

                std::string submeshName = node->name.data ? std::string(node->name.data, node->name.length)
                                                          : ("Skinned_" + std::to_string(ni));

                if (uMesh->material_parts.count > 0) {
                    for (size_t pi = 0; pi < uMesh->material_parts.count; ++pi) {
                        const ufbx_mesh_part& part = uMesh->material_parts.data[pi];
                        if (part.num_triangles == 0)
                            continue;
                        uint32_t slotIdx = 0;
                        if (pi < uMesh->materials.count && uMesh->materials.data[pi]) {
                            auto it = InMatToSlot.find(uMesh->materials.data[pi]);
                            if (it != InMatToSlot.end())
                                slotIdx = it->second;
                        }
                        std::string name = submeshName;
                        if (uMesh->material_parts.count > 1)
                            name += "_" + std::to_string(pi);
                        appendFacePart(part.face_indices.data, part.num_faces, slotIdx, name);
                    }
                } else {
                    appendFacePart(nullptr, uMesh->num_faces, 0, submeshName);
                }
            }

            if (InSettings.bGenerateTangents) {
                std::vector<FCanonicalMeshVertex> tmp(verts.size());
                for (size_t i = 0; i < verts.size(); ++i) {
                    tmp[i].Position = verts[i].Position;
                    tmp[i].Normal = verts[i].Normal;
                    tmp[i].TexCoord = verts[i].TexCoord;
                    tmp[i].Tangent = verts[i].Tangent;
                    tmp[i].Color = verts[i].Color;
                    tmp[i].LightmapUV = verts[i].LightmapUV;
                }
                GenerateLengyelTangents(tmp, indices);
                for (size_t i = 0; i < verts.size(); ++i)
                    verts[i].Tangent = tmp[i].Tangent;
            }

            if (verts.empty() || indices.empty())
                return;

            mesh->CalculateBounds();
            OutResult.SkeletalMesh = mesh;
        }

    } // namespace

    bool FSkeletalImporter::ImportFromScene(ufbx_scene* InScene, const FMeshImportSettings& InSettings,
                                            const std::string& InSourcePath, FMeshImportResult& OutResult) {
        if (InSettings.SharedSkeleton) {
            OutResult.Skeleton = InSettings.SharedSkeleton;
            return true;
        }
        (void)InSettings;
        std::string baseName = FAssetPath::GetFileNameWithoutExtension(InSourcePath);
        OutResult.Skeleton = BuildSkeleton(InScene, baseName, OutResult.Errors);
        if (!OutResult.Skeleton)
            return false;
        return true;
    }

    bool FSkeletalImporter::ImportAnimationsFromScene(ufbx_scene* InScene, const TRef<USkeleton>& InSkeleton,
                                                      const std::string& InSourcePath, FMeshImportResult& OutResult) {
        if (!InScene || !InSkeleton)
            return false;

        auto findNode = [&](const std::string& InBoneName) -> const ufbx_node* {
            for (size_t ni = 0; ni < InScene->nodes.count; ++ni) {
                const ufbx_node* node = InScene->nodes.data[ni];
                if (!node)
                    continue;
                std::string name = NodeName(node, ni);
                if (name == InBoneName || StripBoneNamespace(name) == StripBoneNamespace(InBoneName))
                    return node;
            }
            return nullptr;
        };

        auto sampleStack = [&](const ufbx_anim_stack* stack, const std::string& animName) {
            if (!stack || !stack->anim)
                return;
            double begin = stack->time_begin;
            double end = stack->time_end;
            if (end <= begin + 1e-6)
                return;

            auto seq = UAnimSequence::Create(animName);
            seq->SetUUID(FUUID::FromPath(animName));
            seq->SetDuration(static_cast<float>(end - begin));
            seq->SetSampleRate(30.0f);
            seq->SetLooping(true);

            const int frameCount = std::max(2, static_cast<int>(std::round((end - begin) * 30.0)) + 1);
            auto& tracks = seq->GetTracks();
            const auto& bones = InSkeleton->GetBones();
            tracks.resize(bones.size());

            for (size_t bi = 0; bi < bones.size(); ++bi) {
                tracks[bi].BoneName = bones[bi].Name;
                const ufbx_node* node = findNode(bones[bi].Name);
                tracks[bi].TranslationKeys.resize(static_cast<size_t>(frameCount));
                tracks[bi].RotationKeys.resize(static_cast<size_t>(frameCount));
                tracks[bi].ScaleKeys.resize(static_cast<size_t>(frameCount));
                for (int f = 0; f < frameCount; ++f) {
                    double t = begin + (end - begin) * (static_cast<double>(f) / static_cast<double>(frameCount - 1));
                    FBoneTransform bone = bones[bi].RestLocal;
                    if (node) {
                        FBoneTransform fbxBind = UfbxTransformToBone(node->local_transform);
                        FBoneTransform fbxAnim = UfbxTransformToBone(ufbx_evaluate_transform(stack->anim, node, t));
                        const float bindLen = glm::length(fbxBind.Translation);
                        const float restLen = glm::length(bones[bi].RestLocal.Translation);
                        const float unit = bindLen > 1e-4f ? restLen / bindLen : 1.0f;
                        bone.Translation = fbxAnim.Translation * unit;
                        bone.Rotation = fbxAnim.Rotation;
                        bone.Scale = fbxAnim.Scale;
                        if (glm::length(bones[bi].RestLocal.Scale) < 0.5f)
                            bone.Scale *= bones[bi].RestLocal.Scale;
                    }
                    float time = static_cast<float>(t - begin);
                    tracks[bi].TranslationKeys[static_cast<size_t>(f)] = {time, bone.Translation};
                    tracks[bi].RotationKeys[static_cast<size_t>(f)] = {time, bone.Rotation};
                    tracks[bi].ScaleKeys[static_cast<size_t>(f)] = {time, bone.Scale};
                }
            }
            seq->LinkSkeleton(InSkeleton);
            OutResult.Animations.push_back(seq);
        };

        if (InScene->anim_stacks.count == 0)
            return true;

        std::string baseName = FAssetPath::GetFileNameWithoutExtension(InSourcePath);
        for (size_t si = 0; si < InScene->anim_stacks.count; ++si) {
            const ufbx_anim_stack* stack = InScene->anim_stacks.data[si];
            std::string animName = baseName;
            if (stack && stack->name.data && stack->name.length > 0 && InScene->anim_stacks.count > 1)
                animName = std::string(stack->name.data, stack->name.length);
            sampleStack(stack, animName);
        }
        return true;
    }

    void FSkeletalImporter::ImportSkeletalGeometry(ufbx_scene* InScene, const FMeshImportSettings& InSettings,
                                                   const TRef<USkeleton>& InSkeleton, FMeshImportResult& OutResult) {
        std::unordered_map<const ufbx_material*, uint32_t> matToSlot;
        for (size_t mi = 0; mi < InScene->materials.count && mi < OutResult.ExtractedMaterials.size(); ++mi)
            matToSlot[InScene->materials.data[mi]] = static_cast<uint32_t>(mi);

        ExtractSkinnedMesh(InScene, InSettings, InSkeleton, matToSlot, OutResult);
        if (!OutResult.SkeletalMesh)
            return;
        OutResult.SkeletalMesh->GetMaterialSlots().clear();
        for (const auto& extracted : OutResult.ExtractedMaterials) {
            FSkeletalMaterialSlot slot;
            slot.SlotName = extracted.Name;
            OutResult.SkeletalMesh->GetMaterialSlots().push_back(slot);
        }
        if (OutResult.SkeletalMesh->GetMaterialSlots().empty()) {
            FSkeletalMaterialSlot slot;
            slot.SlotName = "Default";
            OutResult.SkeletalMesh->GetMaterialSlots().push_back(slot);
        }
    }

} // namespace Leon
