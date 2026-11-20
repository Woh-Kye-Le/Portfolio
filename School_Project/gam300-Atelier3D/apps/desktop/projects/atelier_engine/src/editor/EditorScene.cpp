/* !
@file    	    EditorScene.cpp
@author  	    sanni.go@digipen.edu [100%]
@date    		29/09/2026
@brief
    Implements EditorScene: creating, finding, removing, duplicating and pasting
    game objects, loading a scene description and restoring undo snapshots.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/editor/EditorScene.hpp"

#include "engine/scene/Hierarchy.hpp"

#include <algorithm>
#include <map>
#include <numeric>

namespace Engine::Editor
{
    namespace
    {
        // Name for objects nobody named yet, e.g. "GameObject_7"
        std::string DefaultName(std::uint32_t id)
        {
            return "GameObject_" + std::to_string(id);
        }
    }

    // Empty scene, ids start from 1 again
    void EditorScene::Clear()
    {
        scene_.objects.clear();
        nextId_ = 1;
        selected_ = 0;
    }

    // New objects are selected straight away, like in Unity, so you can edit them in the Inspector right away
    GameObject& EditorScene::CreateObject(std::string name)
    {
        GameObject object;
        object.id = nextId_++;
        object.name = name.empty() ? DefaultName(object.id) : std::move(name);
        scene_.objects.push_back(std::move(object));
        selected_ = scene_.objects.back().id;
        return scene_.objects.back();
    }

    GameObject* EditorScene::Find(std::uint32_t id)
    {
        const auto it = std::find_if(scene_.objects.begin(), scene_.objects.end(),
                                     [id](const GameObject& object) { return object.id == id; });
        return it == scene_.objects.end() ? nullptr : &*it;
    }

    const GameObject* EditorScene::Find(std::uint32_t id) const
    {
        const auto it = std::find_if(scene_.objects.begin(), scene_.objects.end(),
                                     [id](const GameObject& object) { return object.id == id; });
        return it == scene_.objects.end() ? nullptr : &*it;
    }

    // Returns false if there was no object with that id. Children go with their parent.
    bool EditorScene::Remove(std::uint32_t id)
    {
        const auto subtree = Scene::SubtreeOf(scene_, id);
        std::erase_if(scene_.objects, [&subtree](const GameObject& object)
                      { return std::find(subtree.begin(), subtree.end(), object.id) != subtree.end(); });
        if (std::find(subtree.begin(), subtree.end(), selected_) != subtree.end())
        {
            selected_ = 0;
        }
        return !subtree.empty();
    }

    // Duplicate (right-click menu) and Paste (Ctrl+V) both copy every component, and everything under the object,
    // but get fresh ids. Duplicate renames the copy, Paste keeps the name and adds " Copy".
    GameObject& EditorScene::Duplicate(std::uint32_t id)
    {
        const GameObject* source = Find(id);
        if (!source)
        {
            return CreateObject();
        }
        // Next to the original: the same parent, so the same Transform relative to it
        const std::uint32_t parent = source->parent;
        std::vector<GameObject> copies = CopyOf(id);
        copies.front().transform = source->transform;
        GameObject& copy = Paste(copies);
        copy.name = DefaultName(copy.id);
        copy.parent = parent;
        return copy;
    }

    GameObject& EditorScene::Paste(const GameObject& source)
    {
        return Paste(std::vector<GameObject>{source});
    }

    GameObject& EditorScene::Paste(const std::vector<GameObject>& objects)
    {
        if (objects.empty())
        {
            return CreateObject();
        }
        // New ids, and parents to match; anything whose parent was not copied goes under the first object
        std::map<std::uint32_t, std::uint32_t> ids;
        for (const GameObject& source : objects)
        {
            ids[source.id] = nextId_++;
        }
        // Prefab links only stay on a whole instance; a part copied without its root is an ordinary object
        const Scene::SceneDescription copied{.objects = objects};
        const auto keepsLink = [&copied](const GameObject& source)
        { return Scene::InstanceRootOf(copied, source.id) != 0; };
        const std::uint32_t firstId = ids.at(objects.front().id);
        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            GameObject copy = objects[i];
            copy.id = ids.at(copy.id);
            copy.parent = i == 0 ? 0 : ids.contains(copy.parent) ? ids.at(copy.parent) : firstId;
            if (copy.prefabInstance && !keepsLink(objects[i]))
            {
                copy.prefabInstance.reset();
            }
            scene_.objects.push_back(std::move(copy));
        }
        GameObject& first = *Find(firstId);
        first.name = objects.front().name + " Copy";
        selected_ = firstId;
        return first;
    }

    std::vector<GameObject> EditorScene::CopyOf(std::uint32_t id) const
    {
        std::vector<GameObject> copies;
        for (const std::uint32_t member : Scene::SubtreeOf(scene_, id))
        {
            copies.push_back(*Find(member));
        }
        if (!copies.empty() && copies.front().transform)
        {
            copies.front().transform = Scene::WorldTransformOf(scene_, id);
        }
        return copies;
    }

    bool EditorScene::SetParent(std::uint32_t child, std::uint32_t parent)
    {
        if (!Scene::CanSetParent(scene_, child, parent))
        {
            return false;
        }
        GameObject* object = Find(child);
        const glm::mat4 oldParent = Scene::ParentMatrixOf(scene_, child);
        const glm::mat4 newParent = Scene::WorldMatrixOf(scene_, parent);
        if (object->transform && oldParent != newParent)
        {
            // Same place in the world, now measured from the new parent
            const glm::mat4 world = oldParent * Scene::ModelMatrixOf(*object->transform);
            *object->transform = Scene::TransformFromMatrix(glm::inverse(newParent) * world, object->transform->scale);
        }
        object->parent = parent;

        // Last among its new siblings (the Hierarchy lists children in scene order)
        const auto it = std::find_if(scene_.objects.begin(), scene_.objects.end(),
                                     [child](const GameObject& other) { return other.id == child; });
        std::rotate(it, it + 1, scene_.objects.end());
        return true;
    }

    GameObject& EditorScene::Insert(GameObject object, std::size_t index)
    {
        // new objects must not reuse its id
        nextId_ = std::max(nextId_, object.id + 1);
        const std::size_t at = std::min(index, scene_.objects.size());
        const auto it =
            scene_.objects.insert(scene_.objects.begin() + static_cast<std::ptrdiff_t>(at), std::move(object));
        selected_ = it->id;
        return *it;
    }

    std::size_t EditorScene::IndexOf(std::uint32_t id) const
    {
        const auto it = std::find_if(scene_.objects.begin(), scene_.objects.end(),
                                     [id](const GameObject& object) { return object.id == id; });
        return static_cast<std::size_t>(it - scene_.objects.begin());
    }

    void EditorScene::SetDescription(Scene::SceneDescription scene)
    {
        scene_ = std::move(scene);
        // start counting after the biggest id in the file so new objects never clash with loaded ones
        nextId_ = std::accumulate(scene_.objects.begin(), scene_.objects.end(), decltype(nextId_){1},
                                  [](decltype(nextId_) next, const GameObject& object)
                                  { return std::max<decltype(nextId_)>(next, object.id + 1); });
        selected_ = 0;
    }

    // Keep the selection if that object still exists after going back
    void EditorScene::RestoreState(const State& state)
    {
        scene_ = state.scene;
        nextId_ = state.nextId;
        if (!Find(selected_))
        {
            selected_ = 0;
        }
    }
}
