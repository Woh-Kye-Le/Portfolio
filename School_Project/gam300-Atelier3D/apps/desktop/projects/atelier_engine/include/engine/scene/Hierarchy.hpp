/* !
@file    	    Hierarchy.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		20/10/2026
@brief
    Declares the parent / child helpers of a scene description: walking the
    object tree, and turning transforms relative to a parent into world ones
    and back.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/scene/SceneDescription.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace Engine::Scene
{
    // ---- Transforms

    // The transform's model matrix (Graphics::Transform::ComposeModelMatrix)
    glm::mat4 ModelMatrixOf(const TransformComponent& transform);

    // The transform a model matrix was made from: position, the rotations in the order ComposeModelMatrix applies
    // them, and the scale on each axis divided by size (which the result keeps as its uniform size). A matrix with
    // shear (a non-uniformly scaled parent with a rotated child) has no exact transform; this is the closest one.
    TransformComponent TransformFromMatrix(const glm::mat4& model, float size = 1.0f);

    // Where local (relative to its parent) is in the world, given where the parent is
    TransformComponent Compose(const TransformComponent& parentWorld, const TransformComponent& local);

    // ---- The object tree (GameObject::parent)

    const GameObject* FindObject(const SceneDescription& scene, std::uint32_t id);
    GameObject* FindObject(SceneDescription& scene, std::uint32_t id);

    // The objects whose parent is id (0: the top-level objects), in scene order
    std::vector<std::uint32_t> ChildrenOf(const SceneDescription& scene, std::uint32_t id);

    // id, then everything under it, depth first with children in scene order. Empty if there is no such object.
    std::vector<std::uint32_t> SubtreeOf(const SceneDescription& scene, std::uint32_t id);

    // True if ancestor is id's parent, its parent's parent, ...
    bool IsAncestorOf(const SceneDescription& scene, std::uint32_t ancestor, std::uint32_t id);

    // Whether child can be put under parent (0: the top of the scene): both exist, and parent is not child itself or
    // under it (which would make a loop)
    bool CanSetParent(const SceneDescription& scene, std::uint32_t child, std::uint32_t parent);

    // The object's model matrix in the world: its Transform after each of its parents'. Objects without a Transform
    // count as the identity, as does a missing id.
    glm::mat4 WorldMatrixOf(const SceneDescription& scene, std::uint32_t id);

    // WorldMatrixOf the object's parent (the identity at the top of the scene)
    glm::mat4 ParentMatrixOf(const SceneDescription& scene, std::uint32_t id);

    // Where the object is in the world; nullopt if it has no Transform
    std::optional<TransformComponent> WorldTransformOf(const SceneDescription& scene, std::uint32_t id);
}
