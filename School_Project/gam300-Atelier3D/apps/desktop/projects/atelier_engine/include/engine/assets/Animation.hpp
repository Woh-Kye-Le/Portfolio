/* !
@file               Animation.hpp
@author             c.kojat@digipen.edu [100%]
@date               02/11/2026
@brief
    Declares the skeletal animation maths on the CPU side: sampling a clip's
    keyframes into node transforms, placing every node in the model, and the
    bone matrices a skinned mesh is drawn with.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Assets Headers
#include "engine/assets/data/ModelData.hpp"

// Third-Party Headers
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Standard Library Headers
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace Engine::Assets
{
    /*!***********************************************************************
    @brief          Most bones one skinned mesh can be drawn with (the size of
                    the standard shader's u_Bones array). Bones past it are left
                    at rest.
    *************************************************************************/
    inline constexpr std::size_t kMaxBones = 128;

    /*!***********************************************************************
    @brief          A node's transform split into the parts clips animate.
    *************************************************************************/
    struct NodePose
    {
        glm::vec3 translation = glm::vec3(0.0f);
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 scale = glm::vec3(1.0f);
    };

    /*!***********************************************************************
    @brief          Splits a transform into translation, rotation and scale
                    (shear is dropped; a mirrored transform keeps its mirror in
                    the scale's x).
    @param          transform     A node's local transform.
    @return         Its parts.
    *************************************************************************/
    NodePose Decompose(const glm::mat4& transform);

    /*!***********************************************************************
    @brief          Puts translation, rotation and scale back into one transform
                    (scale first, then rotation, then translation).
    @param          pose          The parts.
    @return         The transform.
    *************************************************************************/
    glm::mat4 Compose(const NodePose& pose);

    /*!***********************************************************************
    @brief          Finds a clip by name, ignoring case.
    @param          clips         A model's animations.
    @param          name          The clip's name ("Walk").
    @return         Its index, or -1 when there is none of that name.
    *************************************************************************/
    int FindClip(const std::vector<AnimationClip>& clips, std::string_view name);

    /*!***********************************************************************
    @brief          The local transform of every node at a moment of a clip:
                    the nodes it animates are moved, the others keep their rest
                    transform. Times before the first key hold the first one and
                    times after the last key hold the last one (looping is the
                    caller's: it wraps the time).
    @param          clip          The animation.
    @param          time          Seconds into it.
    @param          rest          Every node's rest (file) local transform.
    @return         One local transform per node.
    *************************************************************************/
    std::vector<glm::mat4> SampleClip(const AnimationClip& clip, float time, const std::vector<glm::mat4>& rest);

    /*!***********************************************************************
    @brief          Every node's transform from the model's root: the locals
                    composed down the hierarchy from each root. A node that no
                    root reaches keeps its own local transform.
    @param          nodes         The hierarchy: anything with a `children` list
                                  of node indices (ModelNode, Graphics::Model::Node).
    @param          roots         Nodes with no parent.
    @param          locals        One local transform per node.
    @return         One model-space transform per node.
    *************************************************************************/
    template <class NodeT>
    std::vector<glm::mat4> NodeWorlds(const std::vector<NodeT>& nodes, const std::vector<std::uint32_t>& roots,
                                      const std::vector<glm::mat4>& locals)
    {
        std::vector<glm::mat4> worlds = locals;
        worlds.resize(nodes.size(), glm::mat4(1.0f));
        std::vector<bool> placed(nodes.size(), false);
        // (node, parent's world) pairs still to place; a node reached twice (a broken file) is placed once
        std::vector<std::pair<std::uint32_t, glm::mat4>> pending;
        for (const std::uint32_t root : roots)
        {
            pending.emplace_back(root, glm::mat4(1.0f));
        }
        while (!pending.empty())
        {
            const auto [index, parent] = pending.back();
            pending.pop_back();
            if (index >= nodes.size() || placed[index])
            {
                continue;
            }
            placed[index] = true;
            worlds[index] = parent * (index < locals.size() ? locals[index] : glm::mat4(1.0f));
            for (const std::uint32_t child : nodes[index].children)
            {
                pending.emplace_back(child, worlds[index]);
            }
        }
        return worlds;
    }

    /*!***********************************************************************
    @brief          The bone matrices of a skin, for the shader's u_Bones:
                    jointWorld * inverseBind for each joint (at most kMaxBones).
    @param          skin          The skeleton.
    @param          worlds        Every node's model-space transform (NodeWorlds).
    @return         One matrix per joint.
    *************************************************************************/
    std::vector<glm::mat4> SkinMatrices(const Skin& skin, const std::vector<glm::mat4>& worlds);

    /*!***********************************************************************
    @brief          The box around a model or mesh, in its own units.
    *************************************************************************/
    struct BoundingBox
    {
        glm::vec3 min = glm::vec3(0.0f);
        glm::vec3 max = glm::vec3(0.0f);

        glm::vec3 Center() const { return 0.5f * (min + max); }
        glm::vec3 Half() const { return 0.5f * (max - min); }
    };

    /*!***********************************************************************
    @brief          The box around a whole model as it stands at rest: each mesh
                    placed by its node, a skinned one bent by its skeleton's
                    rest pose (which is how the model is drawn without a clip).
    @param          model         The model.
    @return         Its box, or nullopt when it has no vertices.
    *************************************************************************/
    std::optional<BoundingBox> RestBounds(const ModelData& model);

    /*!***********************************************************************
    @brief          The triangles of a whole model as it stands at rest, placed
                    the way RestBounds places its vertices: for what has to hit
                    the model itself rather than its box (clicking on it).
    @param          model         The model.
    @return         Three corners per triangle, in the model's own units.
    *************************************************************************/
    std::vector<glm::vec3> RestTriangles(const ModelData& model);

    /*!***********************************************************************
    @brief          The box around a box turned round Y (a model turned inside
                    its object).
    @param          box           The box.
    @param          degrees       How far it is turned.
    @return         The box around the turned one.
    *************************************************************************/
    BoundingBox Turned(const BoundingBox& box, float degrees);

    /*!***********************************************************************
    @brief          A clip of one model made to move another with the same
                    skeleton: each channel moves the node of the same name in the
                    other model (channels of nodes it does not have are left out).
    @param          clip          The clip.
    @param          from          The nodes of the model the clip came with.
    @param          to            The nodes of the model it is to move.
    @return         The clip for that model.
    *************************************************************************/
    AnimationClip Retarget(const AnimationClip& clip, const std::vector<ModelNode>& from,
                           const std::vector<ModelNode>& to);
}
