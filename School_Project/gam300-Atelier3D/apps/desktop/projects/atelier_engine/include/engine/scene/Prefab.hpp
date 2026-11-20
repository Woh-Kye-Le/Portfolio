/* !
@file    	    Prefab.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		20/10/2026
@brief
    Declares Prefab, a saved group of game objects to place again and again,
    and LoadPrefabFile / SavePrefabFile for its JSON file format.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/scene/SceneFile.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Engine::Scene
{
    // Prefab files live in a game's assets/prefabs/<name>.prefab and are read and written with
    // Serializer::JSONSerializer:
    //   { "version": 1, "name": "Enemy",
    //     "objects": [ { "id": 1, "name": "Enemy", "transform": { ... }, "health": { ... } },
    //                  { "id": 2, "name": "Head", "parent": 1, "transform": { ... }, "renderable": { ... } } ] }
    // The objects are laid out exactly like a scene file's (SceneFile.hpp). The first is the root: it has id
    // kPrefabRootId and no parent, and every other object is under it, listed after its parent. A child's transform is
    // relative to its parent, as in a scene; the root's is where a new instance is placed by default. Objects in a
    // prefab are only linked to a prefab in a variant (see below) or in a nested prefab.
    //
    // A nested prefab is a whole instance of another prefab under the root (like Unity's nested prefabs), laid out as
    // an instance in a scene is: each of its objects is linked to the object of the other prefab it copies, with the
    // properties this prefab changed on it as its overrides; its own root is linked to the other prefab's root.
    // Game::PrefabManager brings nested instances up to date with their prefabs when it reads a prefab, so a change
    // to the inner prefab reaches every instance of the outer one, except what the outer prefab (or an instance)
    // changed. A prefab never contains itself, directly or through the prefabs in it.
    //
    // A variant (like Unity's prefab variants) is a prefab based on another one, its base:
    //   { "version": 1, "name": "EnemyBoss", "base": "assets/prefabs/Enemy.prefab", "objects": [ ... ] }
    // Its objects are laid out as an instance of the base in a scene is: each one copied from the base is linked to
    // the base object it copies (PrefabInstance, with the properties the variant changed as its overrides), and
    // objects the variant added are not linked. Like an instance, the file holds every object's own data, so a
    // variant still loads if its base is gone; Game::PrefabManager brings it up to date with its base when it reads
    // it. The root is linked to the base's root. A variant can be the base of another variant.
    inline constexpr int kPrefabFormatVersion = 1;
    inline constexpr const char* kPrefabsFolder = "assets/prefabs";
    inline constexpr const char* kPrefabExtension = ".prefab";

    struct Prefab
    {
        std::string name;
        std::string guid;                // stays the same when the file moves; made when it is first saved
        std::string base;                // a variant's base prefab file, relative to the game folder; empty if none
        std::vector<GameObject> objects; // objects[0] is the root

        bool IsVariant() const { return !base.empty(); }

        // The object with this id, or nullptr
        const GameObject* Find(std::uint32_t id) const;

        // What the prefab holds: its guid says which prefab it is, not what is in it
        bool operator==(const Prefab& other) const
        {
            return name == other.name && base == other.base && objects == other.objects;
        }
    };

    // Reads a prefab file. Never throws; forgiving in the same way as LoadSceneFile, and logs the same way. Also fixes
    // the layout: a root that is not first or not kPrefabRootId, an object outside the root, or a prefab link on an
    // object that is not part of a whole instance (of a variant's base, or a nested prefab) are each corrected with a
    // warning; a variant whose root is not linked to its base's root becomes an ordinary prefab. Fails if the file has no objects. prefab is only changed
    // when the report is ok.
    SceneLoadReport LoadPrefabFile(const std::filesystem::path& path, Prefab& prefab);

    // Writes a prefab file, creating its folder if needed. Throws std::runtime_error if the file cannot be written.
    void SavePrefabFile(const std::filesystem::path& path, const Prefab& prefab);
}
