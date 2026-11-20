/* !
@file               Model.hpp
@author             w.kyele@digipen.edu [100%]
@date               26/10/2026
@brief
    Declares Model, the GPU-ready half of a loaded file: the uploaded meshes with
    their per-material draw runs, the resolved materials, and the node hierarchy.
    Model::FromModelData performs the upload, Model::Draw walks the hierarchy and
    issues the draws.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Assets Headers
#include "engine/assets/data/ModelData.hpp"

// Engine Graphics Headers
#include "engine/graphics/Camera/Camera.hpp"
#include "engine/graphics/Material/Material.hpp"
#include "engine/graphics/Mesh/Mesh.hpp"

// Third-Party Headers
#include <glm/glm.hpp>

// Standard Library Headers
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          Turns a material's texture path - as written in the model
                    file - into an already uploaded GPU texture. Null when the
                    slot has no texture, so the material keeps the white default.
    *************************************************************************/
    using TextureResolver = std::function<std::shared_ptr<Texture>(const std::string& path)>;

    /*!***********************************************************************
    @brief          An uploaded model: one shared mesh per MeshData, the material
                    instances those meshes reference, and the node hierarchy.

                    A Model is a plain value (move it, hold it by value); its GPU
                    sub-objects are shared and live as long as it does, so it must
                    be destroyed before the GL context goes away.
    *************************************************************************/
    class Model
    {
    public:
        /*!***********************************************************************
        @brief          One material run inside an uploaded mesh: where it starts
                        in the index buffer, how long it is, and which material slot
                        of Materials() it draws with.
        *************************************************************************/
        struct DrawRange
        {
            std::uint32_t firstIndex = 0;
            std::uint32_t indexCount = 0;
            std::uint32_t materialIndex = 0;
        };

        /*!***********************************************************************
        @brief          An uploaded mesh plus the runs it is drawn in. A mesh with
                        no submesh information gets a single run covering all of its
                        indices.
        *************************************************************************/
        struct MeshEntry
        {
            std::shared_ptr<Mesh> mesh;
            std::vector<DrawRange> ranges;
            std::int32_t skin = -1; // into Skins(): the skeleton that bends it, or -1
        };

        /*!***********************************************************************
        @brief          One uploaded node: its local transform, the meshes it
                        carries and the children below it.
        *************************************************************************/
        struct Node
        {
            std::string name;
            glm::mat4 localTransform = glm::mat4(1.0f);
            std::vector<std::uint32_t> meshIndices; // into Meshes()
            std::vector<std::uint32_t> children;    // into Nodes()
        };

        /*!***********************************************************************
        @brief          Default constructor that creates an empty Model (no meshes, materials or nodes).
        *************************************************************************/
        Model() = default;

        /*!***********************************************************************
        @brief          Uploads a CPU-side ModelData: builds a Graphics::Mesh per
                        mesh (folding each submesh's baseVertex into its own index
                        run), builds the Material instances and asks the resolver for
                        the GPU texture of every texture slot.
        @param          data            CPU-side model to upload.
        @param          defaultShader   Shader given to every material (the renderer's
                                        lit standard shader); materials draw nothing
                                        without one. Held by reference, so it must
                                        outlive the model.
        @param          resolveTexture  Turns a material's texture path into a GPU
                                        texture (loaded and shared by the caller).
        @return         Uploaded Model, ready to Draw.
        *************************************************************************/
        static Model FromModelData(const Assets::ModelData& data, const Shader& defaultShader,
                                   const TextureResolver& resolveTexture);

        /*!***********************************************************************
        @brief          Draws the whole model: every root node (or every mesh when
                        the file had no hierarchy) with model composed on top of each
                        node's own transform.
        @param          model         Object's model matrix (position/rotation/scale).
        @param          camera        Camera the frame is seen through.
        *************************************************************************/
        void Draw(const glm::mat4& model, const Camera& camera) const;

        /*!***********************************************************************
        @brief          Draws the whole model posed at a moment of one of its
                        animations: the clip moves the nodes, skinned meshes bend
                        with their skeletons. A clip of -1 (or one the model does
                        not have) draws it at rest, as Draw(model, camera).
        @param          model         Object's model matrix (position/rotation/scale).
        @param          camera        Camera the frame is seen through.
        @param          clip          Index into Animations(), or -1.
        @param          time          Seconds into the clip.
        @param          material      Drawn with in place of every one of the model's own materials (an object's
                                      Material); nullptr for its own.
        *************************************************************************/
        void Draw(const glm::mat4& model, const Camera& camera, int clip, float time,
                  const Material* material = nullptr) const;

        /*!***********************************************************************
        @brief          As above, posed by a clip of the caller's (one of another file, made for this model's nodes:
                        Assets::Retarget); nullptr draws it at rest.
        *************************************************************************/
        void Draw(const glm::mat4& model, const Camera& camera, const Assets::AnimationClip* clip, float time,
                  const Material* material = nullptr) const;

        /*!***********************************************************************
        @brief          Retrieves the model's animations ("Walk", "Run", ...).
        @return         Const reference to the list of clips.
        *************************************************************************/
        const std::vector<Assets::AnimationClip>& Animations() const { return animations_; }

        /*!***********************************************************************
        @brief          Where a node (such as a hand's bone) is inside the model, posed by a clip at a moment.
        @param          name          The node's name in the model file.
        @param          clip          The clip it is posed by; nullptr: at rest.
        @param          time          Seconds into the clip.
        @return         Its transform from the model's root; nothing if the model has no node of that name.
        *************************************************************************/
        std::optional<glm::mat4> NodeWorld(std::string_view name, const Assets::AnimationClip* clip, float time) const;
        /*!***********************************************************************
        @brief          Retrieves the skeletons that bend the model's skinned meshes.
        @return         Const reference to the list of skins.
        *************************************************************************/
        const std::vector<Assets::Skin>& Skins() const { return skins_; }

        /*!***********************************************************************
        @brief          Checks whether the model has no uploaded meshes.
        @return         True if the model holds no meshes, otherwise false.
        *************************************************************************/
        bool IsEmpty() const { return meshes_.empty(); }

        /*!***********************************************************************
        @brief          Retrieves the name the model was uploaded under.
        @return         Const reference to the model name.
        *************************************************************************/
        const std::string& GetName() const { return name_; }
        /*!***********************************************************************
        @brief          Retrieves the uploaded meshes with the runs each one is drawn in.
        @return         Const reference to the list of MeshEntry.
        *************************************************************************/
        const std::vector<MeshEntry>& Meshes() const { return meshes_; }
        /*!***********************************************************************
        @brief          Retrieves the material instances the draw runs refer to by index.
        @return         Const reference to the list of materials.
        *************************************************************************/
        const std::vector<std::shared_ptr<Material>>& Materials() const { return materials_; }
        /*!***********************************************************************
        @brief          Retrieves the uploaded node hierarchy.
        @return         Const reference to the list of nodes.
        *************************************************************************/
        const std::vector<Node>& Nodes() const { return nodes_; }
        /*!***********************************************************************
        @brief          Retrieves the indices of the nodes that have no parent.
        @return         Const reference to the list of root node indices.
        *************************************************************************/
        const std::vector<std::uint32_t>& Roots() const { return roots_; }

    private:
        /*!***********************************************************************
        @brief          Draws every run of one mesh with its own material.
        @param          meshIndex     Which entry of Meshes() to draw.
        @param          model         World transform for this mesh.
        @param          camera        Camera the frame is seen through.
        @param          bones         A skinned mesh's bone matrices (nullptr for one without a skeleton).
        @param          override      Drawn with in place of its own materials (nullptr for its own).
        *************************************************************************/
        void DrawMesh(std::uint32_t meshIndex, const glm::mat4& model, const Camera& camera,
                      const std::vector<glm::mat4>* bones = nullptr, const Material* override = nullptr) const;

        std::string name_;
        std::vector<MeshEntry> meshes_;
        std::vector<std::shared_ptr<Material>> materials_;
        std::vector<Node> nodes_;
        std::vector<std::uint32_t> roots_;
        std::vector<glm::mat4> restLocals_; // every node's local transform from the file, for posing
        std::vector<Assets::Skin> skins_;
        std::vector<Assets::AnimationClip> animations_;
        std::shared_ptr<Material> defaultMaterial_; // used by a run whose material slot is missing
    };
}
