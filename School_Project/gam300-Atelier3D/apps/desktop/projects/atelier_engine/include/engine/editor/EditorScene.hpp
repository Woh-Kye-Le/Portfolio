/* !
@file    	    EditorScene.hpp
@author  	    sanni.go@digipen.edu [100%]
@date    		29/09/2026
@brief
    Declares EditorScene, the scene open in the editor: a SceneDescription plus
    selection, new object ids and snapshots for undo/redo.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#pragma once

#include "engine/scene/SceneDescription.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Engine::Editor
{
    using Scene::GameObject;

    // The scene open in the editor: a Scene::SceneDescription (the same data the scene file and the game use) plus
    // what editing needs on top - the selection, new-object ids and snapshots for undo. It runs nothing itself;
    // GameRuntime turns Description() into ECS entities.
    class EditorScene
    {
    public:
        void Clear();
        // A new object has no components. It is selected.
        GameObject& CreateObject(std::string name = {});
        GameObject* Find(std::uint32_t id);
        const GameObject* Find(std::uint32_t id) const;
        // Removes the object and everything under it
        bool Remove(std::uint32_t id);
        // A copy of an object and everything under it (with the same components), under new ids and the same parent;
        // the copy is selected. A copied prefab instance is another instance of the prefab; a copied part of one is
        // not linked to the prefab.
        GameObject& Duplicate(std::uint32_t id);
        // Adds copies of objects: objects[0] and the ones under it (as CopyOf gives them), at the top of the scene.
        // The first is selected and returned.
        GameObject& Paste(const std::vector<GameObject>& objects);
        GameObject& Paste(const GameObject& source);
        // The object and everything under it, ready to Paste: the object's Transform is where it is in the world
        std::vector<GameObject> CopyOf(std::uint32_t id) const;

        // Puts child under parent (0: the top of the scene) without moving it in the world: its Transform becomes
        // relative to the new parent. It goes last among its new siblings. False (and nothing changes) if either
        // object is missing or parent is child itself or under it.
        bool SetParent(std::uint32_t child, std::uint32_t parent);
        // Puts an object back with its own id (undo of a delete, redo of a create) at index in the list, or end if index is past it
        GameObject& Insert(GameObject object, std::size_t index);
        // Place of the object in Objects()
        std::size_t IndexOf(std::uint32_t id) const;

        // The scene as saved and played
        const Scene::SceneDescription& Description() const { return scene_; }
        // For edits made outside this class (e.g. Game::PrefabManager), with the next free id to use for new objects
        Scene::SceneDescription& Description() { return scene_; }
        std::uint32_t& NextId() { return nextId_; }
        // Replaces the scene (e.g. one just loaded); new objects get ids above every id in it. Nothing is selected.
        void SetDescription(Scene::SceneDescription scene);

        struct State
        {
            Scene::SceneDescription scene;
            std::uint32_t nextId = 1;

            bool operator==(const State&) const = default;
        };
        State CaptureState() const { return {scene_, nextId_}; }
        void RestoreState(const State& state);

        const std::vector<GameObject>& Objects() const { return scene_.objects; }
        std::vector<GameObject>& Objects() { return scene_.objects; }
        std::uint32_t Selected() const { return selected_; }
        void Select(std::uint32_t id) { selected_ = id; }

    private:
        Scene::SceneDescription scene_;
        std::uint32_t nextId_ = 1;
        std::uint32_t selected_ = 0;
    };
}
