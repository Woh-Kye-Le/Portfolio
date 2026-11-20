/* !
@file    	    PrefabManager.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		20/10/2026
@brief
    Declares PrefabManager: a game's prefab files, saving groups of objects as
    prefabs, placing instances of them, and keeping each instance's own
    property changes (overrides) while the prefab changes.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/scene/Prefab.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace Engine::Game
{
    // The prefabs of one game project (Scene::Prefab, files in assets/prefabs/) and the operations that tie scene
    // objects to them. A prefab instance is an ordinary group of scene objects, each linked to the prefab object it
    // is a copy of by a Scene::PrefabInstance; see that header for what the link records.
    //
    // Overrides: a property an instance object has set differently from its prefab object. RefreshOverrides records
    // them, so call it after the scene is edited. UpdateInstances then brings instances up to date with their
    // prefabs (after a prefab changed): properties that are not overridden take the prefab's values, and parts the
    // prefab gained or lost are added or removed. The root's name and Transform are each instance's own.
    //
    // Operations that add objects take nextId, the next free object id of the scene, and move it on for each new
    // object (EditorScene keeps it, so ids are never reused).
    //
    // Variants (Scene::Prefab::base, like Unity's prefab variants) are prefabs laid out as an instance of their base,
    // so the same rules apply one level up: Find brings a variant up to date with its base each time it reads it
    // (keeping what the variant overrides), and saving any prefab makes every variant be read again, so variants of
    // variants follow too. A variant that is, through its bases, its own base is read as it is saved (logged).
    class PrefabManager
    {
    public:
        explicit PrefabManager(std::filesystem::path projectDir = {});

        // Another game: prefabs are found and saved in its folder (and the cache is emptied)
        void SetProjectDir(std::filesystem::path projectDir);
        const std::filesystem::path& ProjectDir() const;

        // ---- The game's prefabs

        // Every .prefab file under assets/prefabs, relative to the game folder with '/' separators, sorted
        std::vector<std::string> List() const;
        // The prefab at path (relative to the game folder), read on first use and kept; a variant is first brought up
        // to date with its base. nullptr if it is missing or cannot be read (logged once; Forget() to try again).
        // Saving a prefab drops the variants kept, so a pointer to a variant lasts until the next save.
        const Scene::Prefab* Find(const std::string& path);
        // Drops every kept prefab, so each is read from its file again (e.g. after editing one by hand)
        void Forget();
        // Where a prefab of this name is saved: "assets/prefabs/<name>.prefab"
        static std::string PathFor(const std::string& name);

        // ---- Making prefabs

        // The object and everything under it as a prefab named name: ids 1 (the root) to n in SubtreeOf order,
        // parents to match. Whole instances of other prefabs under the root keep their links (they are nested
        // prefabs); every other link is dropped. The root keeps its Transform (where new instances go by default).
        // Throws std::invalid_argument if there is no such object.
        static Scene::Prefab Capture(const Scene::SceneDescription& scene, std::uint32_t rootId, std::string name);

        // Saves the object and everything under it as a new prefab file (PathFor(name)), its root named name, and
        // turns them into an instance of it. Returns the prefab's path. Throws std::runtime_error if no game is open, the name is not
        // 1-64 letters, digits, '_' or '-', a prefab of that name exists, the file cannot be written, or the objects
        // are only part of another prefab's instance (unpack it first); std::invalid_argument if there is no such
        // object.
        std::string CreatePrefab(Scene::SceneDescription& scene, std::uint32_t rootId, const std::string& name);

        // Saves a new variant of the prefab at basePath (PathFor(name)): a copy of every base object, each linked to
        // the one it copies, with nothing changed yet, its root named name. Returns its path. Throws
        // std::runtime_error for the same reasons as CreatePrefab, or if the base cannot be read.
        std::string CreateVariant(const std::string& basePath, const std::string& name);

        // Whether the prefab at outer is inner, or has it as a base or nested in it (at any depth). Placing inner in
        // outer would then make a prefab that contains itself.
        bool Contains(const std::string& outer, const std::string& inner);
        // The prefabs whose base is the one at path (not their own variants), sorted
        std::vector<std::string> VariantsOf(const std::string& path);

        // ---- Instances

        // Adds a copy of the prefab to the end of the scene, every object linked to the one it copies. The root goes
        // at placement (default: the prefab root's Transform) and is called name (default: the prefab's name).
        // Returns the new root's id, or 0 if the prefab cannot be read.
        std::uint32_t Instantiate(Scene::SceneDescription& scene, const std::string& path, std::uint32_t& nextId,
                                  const std::optional<Scene::TransformComponent>& placement = std::nullopt,
                                  const std::string& name = {});

        // The properties where object differs from prefabObject (see PrefabInstance::overrides), sorted
        static std::vector<std::string> Differences(const Scene::GameObject& object,
                                                    const Scene::GameObject& prefabObject);

        // The prefab object a linked scene object is a copy of; nullptr if it is not linked or the prefab is gone
        const Scene::GameObject* PrefabObjectOf(const Scene::GameObject& object);

        // Records the overrides of every linked object in the scene. Objects whose prefab cannot be read keep the
        // list they had. Returns true if any list changed.
        bool RefreshOverrides(Scene::SceneDescription& scene);
        // Points links whose prefab file is gone at the file with their guid, if there is one (a prefab that was moved
        // or renamed outside the editor). Returns how many it changed.
        std::size_t RelinkMoved(Scene::SceneDescription& scene);

        // Brings every instance (of path only, when given) up to date with its prefab, keeping the overrides each
        // object has recorded. Returns true if the scene changed.
        bool UpdateInstances(Scene::SceneDescription& scene, std::uint32_t& nextId, const std::string& path = {});

        // Sets one property of a linked object back to its prefab's value (a component key, such as "light", reverts
        // the whole component); every property when property is empty. Returns false if nothing changed.
        bool Revert(Scene::SceneDescription& scene, std::uint32_t objectId, const std::string& property = {});

        // Writes the instance that objectId belongs to back into its prefab file: its parts, their properties and
        // any objects added under it (which become parts of the prefab; whole instances of other prefabs among them
        // become prefabs nested in it). The prefab keeps its own root name and Transform. Then every other instance in
        // the scene is updated, keeping its overrides. Throws std::invalid_argument if the object is not part of an
        // instance, std::runtime_error if the prefab cannot be read or written or an instance added under it is (or
        // contains) the prefab itself.
        void ApplyToPrefab(Scene::SceneDescription& scene, std::uint32_t objectId, std::uint32_t& nextId);

        // Turns the instance objectId belongs to back into ordinary objects (they keep all their data). Returns
        // false if it is not part of one.
        static bool Unpack(Scene::SceneDescription& scene, std::uint32_t objectId);

        // Writes one property of a linked object (see PrefabInstance::overrides; a component key such as "light"
        // writes the whole component) into the prefab object it is a copy of, then updates every other instance of
        // that prefab in the scene, keeping their own overrides. Returns false if the object does not override it.
        // Throws std::invalid_argument if the object is not linked, std::runtime_error if the prefab cannot be read or
        // written or no longer has that object.
        bool ApplyProperty(Scene::SceneDescription& scene, std::uint32_t objectId, const std::string& property,
                           std::uint32_t& nextId);

        // The roots of the scene's instances of the prefab at path, in scene order
        static std::vector<std::uint32_t> InstancesOf(const Scene::SceneDescription& scene, const std::string& path);

        // Deletes the prefab file at path. The scene's instances of it are unpacked first, so they keep their
        // objects; instances in other scenes keep theirs too, but are no longer updated. Its variants become ordinary
        // prefabs, and prefabs it is nested in keep its objects (no longer linked to it). Throws std::runtime_error if
        // a file cannot be written or removed.
        struct DeleteResult
        {
            std::size_t instances = 0;  // unpacked in the scene
            std::size_t variants = 0;   // turned into ordinary prefabs
            std::size_t containers = 0; // prefabs it was nested in
        };
        DeleteResult Delete(Scene::SceneDescription& scene, const std::string& path);

        // ---- Editing a prefab on its own (the editor's Prefab Mode)

        // The prefab's objects as a scene to edit, with the ids they have in the prefab (the root is kPrefabRootId).
        // A variant's objects keep their links to its base, so the scene is an instance of the base; nested prefabs
        // are instances under the root.
        static Scene::SceneDescription ToScene(const Scene::Prefab& prefab);

        // The prefab a scene made by ToScene now describes: its root (kPrefabRootId) and everything under it, parents
        // first, each keeping its id so instances still know which of their parts is which. Objects that are not
        // under the root are left out. A root linked to a prefab's root makes it a variant of that prefab; whole
        // instances under the root are nested prefabs (other links are dropped). Throws std::invalid_argument if the
        // scene has no root.
        static Scene::Prefab FromScene(const Scene::SceneDescription& scene, std::string name);

        // Writes prefab to path (relative to the game folder) and keeps it, so Find and UpdateInstances use it (a
        // variant's overrides are recorded first). Throws std::runtime_error if no game is open or the file cannot be
        // written.
        void Save(const std::string& path, Scene::Prefab prefab);

    private:
        bool UpdateInstance(Scene::SceneDescription& scene, std::uint32_t rootId, std::uint32_t& nextId);
        // The path a new prefab called name is saved at; throws std::runtime_error if it cannot be made (see
        // CreatePrefab)
        std::string NewPrefabPath(const std::string& name) const;
        // Brings a prefab just read from path up to date with the prefabs it is made from: a variant's base, and the
        // prefabs nested in it
        void Resolve(Scene::Prefab& prefab, const std::string& path);
        // Every prefab write: records the overrides of a variant or of nested prefabs, writes the file and keeps it
        // (see Keep)
        void Store(const std::string& path, Scene::Prefab prefab);
        // Keeps prefab as the one at path, and drops every other variant or prefab with nested prefabs kept: one may
        // be made from it
        void Keep(const std::string& path, Scene::Prefab prefab);

        std::filesystem::path projectDir_;
        std::map<std::string, std::optional<Scene::Prefab>> cache_; // nullopt: could not be read
        std::set<std::string> resolving_; // prefabs being brought up to date, to stop at one that contains itself
    };
}
