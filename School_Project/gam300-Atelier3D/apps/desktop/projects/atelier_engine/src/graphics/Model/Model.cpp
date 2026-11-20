/* !
@file               Model.cpp
@author             w.kyele@digipen.edu [100%]
@date               26/10/2026
@brief
    Implements the Model upload (meshes, textures, materials) and the hierarchy
    walk that draws a whole model. Nothing here knows which file format the
    ModelData came from.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/graphics/Model/Model.hpp"

// Engine Assets Headers
#include "engine/assets/Animation.hpp"

// Engine Graphics Headers
#include "engine/graphics/Renderer/MeshRenderer.hpp"

// Standard Library Headers
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace Engine::Graphics
{
    namespace
    {
        /*!***********************************************************************
        @brief          Folds each submesh's baseVertex into its own index run, so
                        the uploaded buffer needs nothing but a (firstIndex, count)
                        draw per submesh. The indices are copied first because the
                        source ModelData is const.
        @param          mesh          Mesh whose indices are prepared in place.
        *************************************************************************/
        void ApplySubMeshBaseVertex(Assets::MeshData& mesh)
        {
            for (const Assets::SubMesh& sub : mesh.subMeshes)
            {
                if (sub.baseVertex == 0)
                {
                    continue;
                }
                const std::size_t begin = std::min<std::size_t>(sub.baseIndex, mesh.indices.size());
                const std::size_t end = std::min<std::size_t>(begin + sub.indexCount, mesh.indices.size());
                for (std::size_t i = begin; i < end; ++i)
                {
                    const auto shifted = static_cast<std::int64_t>(mesh.indices[i]) + sub.baseVertex;
                    mesh.indices[i] = static_cast<std::uint32_t>(std::max<std::int64_t>(shifted, 0));
                }
            }
        }
    }

    /*!***********************************************************************
    @brief          Uploads a CPU-side ModelData: builds a Graphics::Mesh per mesh (folding each submesh's
                    baseVertex into its own index run), builds the Material instances and asks the resolver for
                    the GPU texture of every texture slot.
    @param          data            CPU-side model to upload.
    @param          defaultShader   Shader given to every material (the renderer's lit standard
                                    shader); materials draw nothing without one. Held by reference, so
                                    it must outlive the model.
    @param          resolveTexture  Turns a material's texture path into a GPU texture (loaded and
                                    shared by the caller).
    @return         Uploaded Model, ready to Draw.
    *************************************************************************/
    Model Model::FromModelData(const Assets::ModelData& data, const Shader& defaultShader,
                               const TextureResolver& resolveTexture)
    {
        Model model;
        model.name_ = data.name;

        model.defaultMaterial_ = std::make_shared<Material>(data.name.empty() ? "default" : data.name + "_default");
        model.defaultMaterial_->SetShader(defaultShader);

        // The resolver already shares one GPU texture per image file, so asking twice
        // for the same path is cheap; we only guard against asking with an empty slot.
        const auto textureFor = [&resolveTexture](const std::string& path) -> std::shared_ptr<Texture>
        { return path.empty() ? nullptr : resolveTexture(path); };

        model.materials_.reserve(data.materials.size());
        for (const Assets::MaterialData& src : data.materials)
        {
            auto material = std::make_shared<Material>(src.name);
            material->SetShader(defaultShader);
            material->SetAlbedoTint(src.baseColorFactor);
            material->SetMetallic(src.metallic);
            material->SetRoughness(src.roughness);
            material->SetEmissiveTint(src.emissiveFactor);
            material->SetEmissiveStrength(src.emissiveStrength);
            material->SetAlbedoTexture(textureFor(src.baseColorTexture));
            material->SetNormalTexture(textureFor(src.normalTexture));
            material->SetMetallicRoughnessTexture(textureFor(src.metallicRoughnessTexture));
            model.materials_.push_back(std::move(material));
        }

        model.meshes_.reserve(data.meshes.size());
        for (const Assets::MeshData& src : data.meshes)
        {
            Assets::MeshData upload = src;
            ApplySubMeshBaseVertex(upload);

            MeshEntry entry;
            entry.mesh = std::make_shared<Mesh>(Mesh::FromAssetData(upload));
            if (src.skin >= 0 && static_cast<std::size_t>(src.skin) < data.skins.size())
            {
                entry.skin = src.skin;
            }

            if (upload.subMeshes.empty())
            {
                DrawRange range;
                range.firstIndex = 0;
                range.indexCount = static_cast<std::uint32_t>(upload.indices.size());
                entry.ranges.push_back(range);
            }
            else
            {
                for (const Assets::SubMesh& sub : upload.subMeshes)
                {
                    const std::size_t first = std::min<std::size_t>(sub.baseIndex, upload.indices.size());
                    const std::size_t count = std::min<std::size_t>(sub.indexCount, upload.indices.size() - first);

                    DrawRange range;
                    range.firstIndex = static_cast<std::uint32_t>(first);
                    range.indexCount = static_cast<std::uint32_t>(count);
                    range.materialIndex = sub.materialIndex;
                    entry.ranges.push_back(range);
                }
            }
            model.meshes_.push_back(std::move(entry));
        }

        model.nodes_.reserve(data.nodes.size());
        for (const Assets::ModelNode& src : data.nodes)
        {
            Node node;
            node.name = src.name;
            node.localTransform = src.localTransform;
            node.meshIndices = src.meshIndices;
            node.children = src.children;
            model.nodes_.push_back(std::move(node));
            model.restLocals_.push_back(src.localTransform);
        }
        model.skins_ = data.skins;
        model.animations_ = data.animations;

        model.roots_ = data.roots;
        if (model.roots_.empty() && !model.nodes_.empty())
        {
            std::vector<bool> isChild(model.nodes_.size(), false);
            for (const Node& node : model.nodes_)
            {
                for (const std::uint32_t child : node.children)
                {
                    if (child < isChild.size())
                    {
                        isChild[child] = true;
                    }
                }
            }
            for (std::uint32_t i = 0; i < model.nodes_.size(); ++i)
            {
                if (!isChild[i])
                {
                    model.roots_.push_back(i);
                }
            }
        }

        return model;
    }

    /*!***********************************************************************
    @brief          Draws the whole model: every root node (or every mesh when the file had no hierarchy) with
                    model composed on top of each node's own transform.
    @param          model         Object's model matrix (position/rotation/scale).
    @param          camera        Camera the frame is seen through.
    *************************************************************************/
    void Model::Draw(const glm::mat4& model, const Camera& camera) const
    {
        Draw(model, camera, -1, 0.0f);
    }

    /*!***********************************************************************
    @brief          Draws the whole model posed at a moment of one of its animations (or at rest for clip -1).
                    A mesh without a skeleton is drawn where its node is; a skinned mesh is drawn from the model's
                    root, bent by its bones (their nodes' transforms from the root times their inverse bind
                    matrices), as glTF and assimp define it.
    @param          model         Object's model matrix (position/rotation/scale).
    @param          camera        Camera the frame is seen through.
    @param          clip          Index into Animations(), or -1.
    @param          time          Seconds into the clip.
    *************************************************************************/
    void Model::Draw(const glm::mat4& model, const Camera& camera, int clip, float time, const Material* material) const
    {
        const bool animated = clip >= 0 && static_cast<std::size_t>(clip) < animations_.size();
        Draw(model, camera, animated ? &animations_[static_cast<std::size_t>(clip)] : nullptr, time, material);
    }

    /*!***********************************************************************
    @brief          Draws the whole model posed by a clip of the caller's (made for this model's nodes), or at rest
                    for nullptr.
    *************************************************************************/
    void Model::Draw(const glm::mat4& model, const Camera& camera, const Assets::AnimationClip* clip, float time,
                     const Material* material) const
    {
        if (nodes_.empty())
        {
            for (std::uint32_t i = 0; i < meshes_.size(); ++i)
            {
                DrawMesh(i, model, camera, nullptr, material);
            }
            return;
        }

        const std::vector<glm::mat4> worlds =
            Assets::NodeWorlds(nodes_, roots_, clip ? Assets::SampleClip(*clip, time, restLocals_) : restLocals_);

        // Each skeleton's bones are worked out once, however many meshes it bends
        std::vector<std::vector<glm::mat4>> bones(skins_.size());
        std::vector<bool> bonesReady(skins_.size(), false);

        // Every node a root reaches, each once
        std::vector<bool> drawn(nodes_.size(), false);
        std::vector<std::uint32_t> pending(roots_.rbegin(), roots_.rend());
        while (!pending.empty())
        {
            const std::uint32_t index = pending.back();
            pending.pop_back();
            if (index >= nodes_.size() || drawn[index])
            {
                continue;
            }
            drawn[index] = true;
            const Node& node = nodes_[index];
            for (const std::uint32_t meshIndex : node.meshIndices)
            {
                const std::int32_t skin = meshIndex < meshes_.size() ? meshes_[meshIndex].skin : -1;
                if (skin < 0)
                {
                    DrawMesh(meshIndex, model * worlds[index], camera, nullptr, material);
                    continue;
                }
                const auto s = static_cast<std::size_t>(skin);
                if (!bonesReady[s])
                {
                    bones[s] = Assets::SkinMatrices(skins_[s], worlds);
                    bonesReady[s] = true;
                }
                DrawMesh(meshIndex, model, camera, &bones[s], material);
            }
            for (auto child = node.children.rbegin(); child != node.children.rend(); ++child)
            {
                pending.push_back(*child);
            }
        }
    }

    std::optional<glm::mat4> Model::NodeWorld(std::string_view name, const Assets::AnimationClip* clip,
                                              float time) const
    {
        const auto node =
            std::find_if(nodes_.begin(), nodes_.end(), [name](const Node& each) { return each.name == name; });
        if (node == nodes_.end())
        {
            return std::nullopt;
        }
        const std::vector<glm::mat4> worlds =
            Assets::NodeWorlds(nodes_, roots_, clip ? Assets::SampleClip(*clip, time, restLocals_) : restLocals_);
        return worlds[static_cast<std::size_t>(node - nodes_.begin())];
    }

    /*!***********************************************************************
    @brief          Draws every run of one mesh with its own material.
    @param          meshIndex     Which entry of Meshes() to draw.
    @param          model         World transform for this mesh.
    @param          camera        Camera the frame is seen through.
    *************************************************************************/
    void Model::DrawMesh(std::uint32_t meshIndex, const glm::mat4& model, const Camera& camera,
                         const std::vector<glm::mat4>* bones, const Material* override) const
    {
        if (meshIndex >= meshes_.size())
        {
            return;
        }

        const MeshEntry& entry = meshes_[meshIndex];
        if (!entry.mesh)
        {
            return;
        }

        for (const DrawRange& range : entry.ranges)
        {
            const Material* material = override; // an object's Material, in place of the model's own
            if (!material && range.materialIndex < materials_.size() && materials_[range.materialIndex])
            {
                material = materials_[range.materialIndex].get();
            }
            else if (!material && defaultMaterial_)
            {
                material = defaultMaterial_.get();
            }
            if (!material)
            {
                continue;
            }
            // The shader is shared with every other draw, so the skinning is switched off again straight after
            const Shader* shader = material->GetShader();
            const bool skinned = bones && !bones->empty() && shader;
            if (skinned)
            {
                shader->SetInt("u_Skinned", 1);
                shader->SetMat4Array("u_Bones", bones->data(), bones->size());
            }
            Renderer::MeshRenderer::DrawRange(*entry.mesh, *material, model, camera, range.firstIndex,
                                              range.indexCount);
            if (skinned)
            {
                shader->SetInt("u_Skinned", 0);
            }
        }
    }
}
