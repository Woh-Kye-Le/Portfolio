/* !
@file    	    Hierarchy.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		20/10/2026
@brief
    Implements the object tree helpers, transform composition and the
    decomposition of a model matrix back into a transform.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/scene/Hierarchy.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Engine::Scene
{
    glm::mat4 ModelMatrixOf(const TransformComponent& transform)
    {
        return ToGraphicsTransform(transform).ComposeModelMatrix();
    }

    // The inverse of T * Rz * Ry * Rx * S, the same way ImGuizmo::DecomposeMatrixToComponents does it
    TransformComponent TransformFromMatrix(const glm::mat4& model, float size)
    {
        glm::vec3 axisX(model[0]);
        glm::vec3 axisY(model[1]);
        glm::vec3 axisZ(model[2]);
        glm::vec3 scale(glm::length(axisX), glm::length(axisY), glm::length(axisZ));
        if (glm::dot(glm::cross(axisX, axisY), axisZ) < 0.0f)
        {
            scale.x = -scale.x; // mirrored
        }
        const auto nonZero = [](float value) { return std::abs(value) > 1e-8f ? value : 1.0f; };
        axisX /= nonZero(scale.x);
        axisY /= nonZero(scale.y);
        axisZ /= nonZero(scale.z);

        // glm is column major: axisX is the first column of Rz * Ry * Rx = (cy cz, cy sz, -sy)
        const float cosY = std::sqrt(axisY.z * axisY.z + axisZ.z * axisZ.z);
        const float rotationY = std::atan2(-axisX.z, cosY);
        float rotationX = std::atan2(axisY.z, axisZ.z);
        float rotationZ = std::atan2(axisX.y, axisX.x);
        if (cosY < 1e-5f)
        {
            // Turned straight up or down (Y = +-90): X and Z then turn around the same axis, and only their
            // difference counts. Put it all in X, rather than reading both out of rounding noise (180, 90, 180).
            rotationX = std::atan2(rotationY > 0.0f ? axisY.x : -axisY.x, axisY.y);
            rotationZ = 0.0f;
        }

        const float uniform = size > 0.0f ? size : 1.0f;
        return TransformComponent{.x = model[3].x,
                                  .y = model[3].y,
                                  .z = model[3].z,
                                  .rotation = glm::degrees(rotationY),
                                  .rotationX = glm::degrees(rotationX),
                                  .rotationZ = glm::degrees(rotationZ),
                                  .scale = uniform,
                                  .scaleX = scale.x / uniform,
                                  .scaleY = scale.y / uniform,
                                  .scaleZ = scale.z / uniform};
    }

    TransformComponent Compose(const TransformComponent& parentWorld, const TransformComponent& local)
    {
        return TransformFromMatrix(ModelMatrixOf(parentWorld) * ModelMatrixOf(local));
    }

    const GameObject* FindObject(const SceneDescription& scene, std::uint32_t id)
    {
        const auto found = std::find_if(scene.objects.begin(), scene.objects.end(),
                                        [id](const GameObject& object) { return object.id == id; });
        return id != 0 && found != scene.objects.end() ? &*found : nullptr;
    }

    GameObject* FindObject(SceneDescription& scene, std::uint32_t id)
    {
        return const_cast<GameObject*>(FindObject(std::as_const(scene), id));
    }

    std::vector<std::uint32_t> ChildrenOf(const SceneDescription& scene, std::uint32_t id)
    {
        std::vector<std::uint32_t> children;
        for (const GameObject& object : scene.objects)
        {
            if (object.parent == id && object.id != id)
            {
                children.push_back(object.id);
            }
        }
        return children;
    }

    std::vector<std::uint32_t> SubtreeOf(const SceneDescription& scene, std::uint32_t id)
    {
        std::vector<std::uint32_t> subtree;
        if (!FindObject(scene, id))
        {
            return subtree;
        }
        // Depth first; the size check stops a loop of parents (never made by the editor) from running forever
        std::vector<std::uint32_t> pending{id};
        while (!pending.empty() && subtree.size() < scene.objects.size())
        {
            const std::uint32_t next = pending.back();
            pending.pop_back();
            subtree.push_back(next);
            const auto children = ChildrenOf(scene, next);
            pending.insert(pending.end(), children.rbegin(), children.rend());
        }
        return subtree;
    }

    bool IsAncestorOf(const SceneDescription& scene, std::uint32_t ancestor, std::uint32_t id)
    {
        const GameObject* object = FindObject(scene, id);
        for (std::size_t steps = 0; object && object->parent != 0 && steps < scene.objects.size(); ++steps)
        {
            if (object->parent == ancestor)
            {
                return true;
            }
            object = FindObject(scene, object->parent);
        }
        return false;
    }

    bool CanSetParent(const SceneDescription& scene, std::uint32_t child, std::uint32_t parent)
    {
        if (!FindObject(scene, child))
        {
            return false;
        }
        return parent == 0 || (parent != child && FindObject(scene, parent) && !IsAncestorOf(scene, child, parent));
    }

    glm::mat4 WorldMatrixOf(const SceneDescription& scene, std::uint32_t id)
    {
        glm::mat4 world(1.0f);
        const GameObject* object = FindObject(scene, id);
        for (std::size_t steps = 0; object && steps <= scene.objects.size(); ++steps)
        {
            if (object->transform)
            {
                world = ModelMatrixOf(*object->transform) * world;
            }
            object = FindObject(scene, object->parent);
        }
        return world;
    }

    glm::mat4 ParentMatrixOf(const SceneDescription& scene, std::uint32_t id)
    {
        const GameObject* object = FindObject(scene, id);
        return object ? WorldMatrixOf(scene, object->parent) : glm::mat4(1.0f);
    }

    std::optional<TransformComponent> WorldTransformOf(const SceneDescription& scene, std::uint32_t id)
    {
        const GameObject* object = FindObject(scene, id);
        if (!object || !object->transform)
        {
            return std::nullopt;
        }
        if (object->parent == 0)
        {
            return object->transform; // exactly as written, not through a matrix
        }
        return TransformFromMatrix(WorldMatrixOf(scene, id), object->transform->scale);
    }
}
