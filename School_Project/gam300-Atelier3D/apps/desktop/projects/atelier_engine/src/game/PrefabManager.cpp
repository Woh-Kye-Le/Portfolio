/* !
@file    	    PrefabManager.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		20/10/2026
@brief
    Implements PrefabManager: finding, caching, capturing and saving prefabs,
    placing instances, and comparing, reverting, applying and updating
    properties property by property through the scene file's JSON layout.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/game/PrefabManager.hpp"

#include "../scene/GameObjectJson.hpp"

#include "engine/core/Log.hpp"
#include "engine/game/GameProject.hpp"
#include "engine/scene/Hierarchy.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <random>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace Engine::Game
{
    namespace
    {
        std::string NewGuid()
        {
            std::random_device random;
            return std::format("{:08x}{:08x}{:08x}{:08x}", random(), random(), random(), random());
        }

        using nlohmann::json;
        using Scene::GameObject;
        using Scene::SceneDescription;

        constexpr std::array<std::string_view, 1> kPrefabExtensions{Scene::kPrefabExtension};

        // Keys of an object's JSON that are not properties: where it sits in its scene and its link. The root's name
        // and Transform are the instance's own (where it was placed).
        bool IsProperty(const std::string& key, bool root)
        {
            return key != "id" && key != "parent" && key != "prefab" &&
                   !(root && (key == "name" || key == "transform"));
        }

        // The properties where two objects' JSON differ: "key/field" inside a component both have, "key" for one only
        // one has (or a plain value such as the name)
        std::vector<std::string> Diff(const json& object, const json& prefabObject, bool root)
        {
            std::set<std::string> keys;
            for (const json* node : {&object, &prefabObject})
            {
                for (const auto& item : node->items())
                {
                    keys.insert(item.key());
                }
            }

            std::vector<std::string> properties;
            for (const std::string& key : keys)
            {
                if (!IsProperty(key, root))
                {
                    continue;
                }
                if (!object.contains(key) || !prefabObject.contains(key))
                {
                    properties.push_back(key);
                    continue;
                }
                const json& mine = object.at(key);
                const json& theirs = prefabObject.at(key);
                if (!mine.is_object() || !theirs.is_object())
                {
                    if (mine != theirs)
                    {
                        properties.push_back(key);
                    }
                    continue;
                }
                std::set<std::string> fields;
                for (const json* node : {&mine, &theirs})
                {
                    for (const auto& item : node->items())
                    {
                        fields.insert(item.key());
                    }
                }
                for (const std::string& field : fields)
                {
                    if (!mine.contains(field) || !theirs.contains(field) || mine.at(field) != theirs.at(field))
                    {
                        properties.push_back(key + "/" + field);
                    }
                }
            }
            std::sort(properties.begin(), properties.end());
            return properties;
        }

        // Makes property of target what it is in source (gone, if source does not have it)
        void CopyProperty(json& target, const json& source, const std::string& property)
        {
            const auto slash = property.find('/');
            const std::string key = property.substr(0, slash);
            if (!source.contains(key))
            {
                target.erase(key);
                return;
            }
            if (slash == std::string::npos || !target.contains(key) || !target.at(key).is_object())
            {
                target[key] = source.at(key);
                return;
            }
            const std::string field = property.substr(slash + 1);
            if (source.at(key).contains(field))
            {
                target[key][field] = source.at(key).at(field);
            }
            else
            {
                target[key].erase(field);
            }
        }

        // Overridden itself, or inside a component that is ("light" covers "light/range")
        bool IsOverridden(const std::vector<std::string>& overrides, const std::string& property)
        {
            const std::string component = property.substr(0, property.find('/'));
            return std::binary_search(overrides.begin(), overrides.end(), property) ||
                   std::binary_search(overrides.begin(), overrides.end(), component);
        }

        // A variant, or a prefab with other prefabs nested in it: some of its objects copy another prefab's
        bool UsesOtherPrefabs(const Scene::Prefab& prefab)
        {
            return std::any_of(prefab.objects.begin(), prefab.objects.end(),
                               [](const GameObject& object) { return object.prefabInstance.has_value(); });
        }

        void RemoveSubtree(SceneDescription& scene, std::uint32_t id)
        {
            const auto subtree = Scene::SubtreeOf(scene, id);
            std::erase_if(scene.objects, [&subtree](const GameObject& object)
                          { return std::find(subtree.begin(), subtree.end(), object.id) != subtree.end(); });
        }
    }

    PrefabManager::PrefabManager(std::filesystem::path projectDir) : projectDir_(std::move(projectDir)) {}

    void PrefabManager::SetProjectDir(std::filesystem::path projectDir)
    {
        projectDir_ = std::move(projectDir);
        cache_.clear();
    }

    const std::filesystem::path& PrefabManager::ProjectDir() const
    {
        return projectDir_;
    }

    std::vector<std::string> PrefabManager::List() const
    {
        if (projectDir_.empty())
        {
            return {};
        }
        return GameProject(projectDir_).FindAssets(Scene::kPrefabsFolder, kPrefabExtensions);
    }

    const Scene::Prefab* PrefabManager::Find(const std::string& path)
    {
        if (projectDir_.empty() || path.empty())
        {
            return nullptr;
        }
        auto found = cache_.find(path);
        if (found == cache_.end())
        {
            if (resolving_.contains(path))
            {
                // Asked for again while it is being brought up to date: its bases or nested prefabs lead back to it
                LOG_ERROR(Prefab,
                          "{} contains itself (through its bases or the prefabs nested in it): the copies that lead "
                          "back to it keep what they have",
                          path);
                return nullptr;
            }
            Scene::Prefab prefab;
            const auto report = Scene::LoadPrefabFile(projectDir_ / path, prefab); // problems are logged
            if (report.ok && UsesOtherPrefabs(prefab))
            {
                resolving_.insert(path);
                Resolve(prefab, path);
                resolving_.erase(path);
            }
            found = cache_.emplace(path, report.ok ? std::optional(std::move(prefab)) : std::nullopt).first;
        }
        std::optional<Scene::Prefab>& cached = found->second;
        return cached ? &*cached : nullptr;
    }

    void PrefabManager::Resolve(Scene::Prefab& prefab, const std::string& path)
    {
        // Laid out like a scene with instances in it: a variant is an instance of its base (the loader made sure its
        // root is linked to the base's root), and each nested prefab is an instance under the root
        SceneDescription scene{.objects = std::move(prefab.objects)};
        std::uint32_t nextId = Scene::kPrefabRootId;
        for (const GameObject& object : scene.objects)
        {
            nextId = std::max(nextId, object.id + 1);
        }
        for (const GameObject& object : scene.objects)
        {
            const std::string other = object.prefabInstance ? object.prefabInstance->prefab : std::string{};
            if (object.prefabInstance && object.prefabInstance->IsRoot() && !resolving_.contains(other) && !Find(other))
            {
                LOG_WARN(Prefab, "{}: {} cannot be read, so '{}' keeps the objects it was saved with", path, other,
                         object.name);
            }
        }
        UpdateInstances(scene, nextId); // parts the other prefabs gained are added after their parents
        RefreshOverrides(scene);
        prefab.objects = std::move(scene.objects);
    }

    std::string PrefabManager::NewPrefabPath(const std::string& name) const
    {
        if (projectDir_.empty())
        {
            throw std::runtime_error("Open a game before making a prefab");
        }
        if (!GameProject::IsValidName(name))
        {
            throw std::runtime_error("Invalid prefab name '" + name +
                                     "': use 1-64 letters, digits, '_' or '-', starting with a letter or digit");
        }
        std::string path = PathFor(name);
        std::error_code ec;
        if (std::filesystem::exists(projectDir_ / path, ec))
        {
            throw std::runtime_error(path + " already exists: pick another name, or use Apply to Prefab on one of its "
                                            "instances");
        }
        return path;
    }

    void PrefabManager::Store(const std::string& path, Scene::Prefab prefab)
    {
        if (projectDir_.empty())
        {
            throw std::runtime_error("Open a game before saving a prefab");
        }
        if (UsesOtherPrefabs(prefab))
        {
            // What a variant changed from its base, or this prefab from a prefab nested in it, is what it keeps when
            // that prefab changes (see Resolve)
            SceneDescription scene{.objects = std::move(prefab.objects)};
            RefreshOverrides(scene);
            prefab.objects = std::move(scene.objects);
        }
        if (prefab.guid.empty())
        {
            // Saved over itself it stays the same prefab, whatever links to it
            Scene::Prefab old;
            std::error_code ec;
            if (std::filesystem::is_regular_file(projectDir_ / path, ec))
            {
                Scene::LoadPrefabFile(projectDir_ / path, old);
            }
            prefab.guid = old.guid.empty() ? NewGuid() : old.guid;
        }
        Scene::SavePrefabFile(projectDir_ / path, prefab); // throws if it cannot be written
        LOG_INFO(Prefab, "Saved prefab {} ({} objects{})", path, prefab.objects.size(),
                 prefab.IsVariant() ? ", a variant of " + prefab.base : std::string{});
        Keep(path, std::move(prefab));
    }

    void PrefabManager::Keep(const std::string& path, Scene::Prefab prefab)
    {
        // Variants and prefabs with nested prefabs are brought up to date with those prefabs when read, and this
        // prefab may be one of them (or one of theirs): they are read again when next used
        std::erase_if(cache_, [&path](const auto& entry)
                      { return entry.first != path && entry.second && UsesOtherPrefabs(*entry.second); });
        cache_[path] = std::move(prefab);
    }

    void PrefabManager::Forget()
    {
        cache_.clear();
    }

    std::string PrefabManager::PathFor(const std::string& name)
    {
        return std::string(Scene::kPrefabsFolder) + "/" + name + Scene::kPrefabExtension;
    }

    Scene::Prefab PrefabManager::Capture(const SceneDescription& scene, std::uint32_t rootId, std::string name)
    {
        const auto subtree = Scene::SubtreeOf(scene, rootId);
        if (subtree.empty())
        {
            throw std::invalid_argument("There is no game object " + std::to_string(rootId) + " to make a prefab of");
        }
        std::map<std::uint32_t, std::uint32_t> ids; // scene id -> prefab id
        for (std::size_t i = 0; i < subtree.size(); ++i)
        {
            ids[subtree[i]] = Scene::kPrefabRootId + static_cast<std::uint32_t>(i);
        }

        Scene::Prefab prefab{.name = std::move(name)};
        for (const std::uint32_t id : subtree)
        {
            GameObject copy = *Scene::FindObject(scene, id);
            copy.id = ids.at(id);
            copy.parent = id == rootId ? 0 : ids.at(copy.parent);
            // Whole instances under the root stay linked to their prefabs: they become nested prefabs. The root's own
            // instance does not (the new prefab is not a variant of it).
            const std::uint32_t instanceRoot = Scene::InstanceRootOf(scene, id);
            const bool nested = instanceRoot != 0 && instanceRoot != rootId &&
                                std::find(subtree.begin(), subtree.end(), instanceRoot) != subtree.end();
            if (!nested)
            {
                copy.prefabInstance.reset();
            }
            prefab.objects.push_back(std::move(copy));
        }
        return prefab;
    }

    std::string PrefabManager::CreatePrefab(SceneDescription& scene, std::uint32_t rootId, const std::string& name)
    {
        const std::string path = NewPrefabPath(name); // throws if it cannot be made

        const auto subtree = Scene::SubtreeOf(scene, rootId);
        for (const std::uint32_t id : subtree)
        {
            const std::uint32_t instanceRoot = Scene::InstanceRootOf(scene, id);
            const GameObject* object = Scene::FindObject(scene, id);
            if (object->prefabInstance &&
                (instanceRoot == 0 || std::find(subtree.begin(), subtree.end(), instanceRoot) == subtree.end()))
            {
                throw std::runtime_error("'" + object->name + "' is part of an instance of " +
                                         object->prefabInstance->prefab + ": unpack that instance first");
            }
        }

        Scene::Prefab prefab = Capture(scene, rootId, name); // throws if there is no such object
        prefab.objects.front().name = name; // the source keeps its own name: a root's name is each instance's own
        std::vector<std::uint32_t> prefabIds;
        for (const GameObject& object : prefab.objects)
        {
            prefabIds.push_back(object.id);
        }
        Store(path, std::move(prefab)); // throws if it cannot be written

        // The objects it was made from are its first instance
        for (std::size_t i = 0; i < subtree.size(); ++i)
        {
            Scene::FindObject(scene, subtree[i])->prefabInstance =
                Scene::PrefabInstance{.prefab = path, .object = prefabIds[i]};
        }
        return path;
    }

    std::string PrefabManager::CreateVariant(const std::string& basePath, const std::string& name)
    {
        const std::string path = NewPrefabPath(name); // throws if it cannot be made
        const Scene::Prefab* base = Find(basePath);
        if (!base)
        {
            throw std::runtime_error("The prefab " + basePath + " could not be read (see Console)");
        }
        // An instance of the base that changes nothing yet, with the base's ids
        Scene::Prefab variant{.name = name, .base = basePath};
        for (const GameObject& source : base->objects)
        {
            GameObject copy = source;
            copy.prefabInstance = Scene::PrefabInstance{.prefab = basePath, .object = source.id};
            variant.objects.push_back(std::move(copy));
        }
        variant.objects.front().name = name;
        Store(path, std::move(variant)); // throws if it cannot be written
        return path;
    }

    bool PrefabManager::Contains(const std::string& outer, const std::string& inner)
    {
        std::set<std::string> seen;
        std::vector<std::string> pending{outer};
        while (!pending.empty())
        {
            const std::string path = std::move(pending.back());
            pending.pop_back();
            if (path == inner)
            {
                return true;
            }
            const Scene::Prefab* prefab = seen.insert(path).second ? Find(path) : nullptr;
            if (!prefab)
            {
                continue;
            }
            // Its base (a variant's objects are linked to it) and the prefabs nested in it
            for (const GameObject& object : prefab->objects)
            {
                if (object.prefabInstance)
                {
                    pending.push_back(object.prefabInstance->prefab);
                }
            }
        }
        return false;
    }

    std::vector<std::string> PrefabManager::VariantsOf(const std::string& path)
    {
        std::vector<std::string> variants;
        for (const std::string& candidate : List())
        {
            const Scene::Prefab* prefab = candidate != path ? Find(candidate) : nullptr;
            if (prefab && prefab->base == path)
            {
                variants.push_back(candidate);
            }
        }
        return variants;
    }

    std::uint32_t PrefabManager::Instantiate(SceneDescription& scene, const std::string& path, std::uint32_t& nextId,
                                             const std::optional<Scene::TransformComponent>& placement,
                                             const std::string& name)
    {
        const Scene::Prefab* prefab = Find(path);
        if (!prefab)
        {
            return 0;
        }
        std::map<std::uint32_t, std::uint32_t> ids; // prefab id -> scene id
        for (const GameObject& source : prefab->objects)
        {
            ids[source.id] = nextId++;
        }
        // Parents come first in a prefab, so every parent is already in the scene when its children are added
        for (const GameObject& source : prefab->objects)
        {
            GameObject copy = source;
            copy.id = ids.at(source.id);
            copy.parent = source.parent == 0 ? 0 : ids.at(source.parent);
            copy.prefabInstance = Scene::PrefabInstance{.prefab = path, .object = source.id};
            if (source.id == Scene::kPrefabRootId)
            {
                if (placement)
                {
                    copy.transform = placement;
                }
                if (!name.empty())
                {
                    copy.name = name;
                }
            }
            scene.objects.push_back(std::move(copy));
        }
        LOG_INFO(Prefab, "Placed an instance of {} ({} objects)", path, prefab->objects.size());
        return ids.at(Scene::kPrefabRootId);
    }

    std::vector<std::string> PrefabManager::Differences(const GameObject& object, const GameObject& prefabObject)
    {
        return Diff(Scene::Detail::ToJson(object), Scene::Detail::ToJson(prefabObject),
                    prefabObject.id == Scene::kPrefabRootId);
    }

    const GameObject* PrefabManager::PrefabObjectOf(const GameObject& object)
    {
        if (!object.prefabInstance)
        {
            return nullptr;
        }
        const Scene::PrefabInstance& link = *object.prefabInstance;
        const Scene::Prefab* prefab = Find(link.prefab);
        return prefab ? prefab->Find(link.object) : nullptr;
    }

    std::size_t PrefabManager::RelinkMoved(SceneDescription& scene)
    {
        std::size_t moved = 0;
        for (GameObject& object : scene.objects)
        {
            if (!object.prefabInstance)
            {
                continue;
            }
            Scene::PrefabInstance& link = *object.prefabInstance;
            std::error_code ec;
            if (link.guid.empty() || std::filesystem::is_regular_file(projectDir_ / link.prefab, ec))
            {
                continue;
            }
            for (const std::string& path : List())
            {
                const Scene::Prefab* prefab = Find(path);
                if (prefab && prefab->guid == link.guid)
                {
                    LOG_INFO(Prefab, "{} moved to {}", link.prefab, path);
                    link.prefab = path;
                    ++moved;
                    break;
                }
            }
        }
        return moved;
    }

    bool PrefabManager::RefreshOverrides(SceneDescription& scene)
    {
        for (GameObject& object : scene.objects)
        {
            if (object.prefabInstance && object.prefabInstance->guid.empty())
            {
                if (const Scene::Prefab* prefab = Find(object.prefabInstance->prefab))
                {
                    object.prefabInstance->guid = prefab->guid;
                }
            }
        }
        bool changed = false;
        for (GameObject& object : scene.objects)
        {
            const GameObject* source = PrefabObjectOf(object);
            if (!source || !object.prefabInstance)
            {
                continue;
            }
            std::vector<std::string>& overrides = object.prefabInstance->overrides;
            auto differences = Differences(object, *source);
            if (differences != overrides)
            {
                overrides = std::move(differences);
                changed = true;
            }
        }
        return changed;
    }

    bool PrefabManager::UpdateInstances(SceneDescription& scene, std::uint32_t& nextId, const std::string& path)
    {
        // Roots first: the scene changes as instances are updated
        std::vector<std::uint32_t> roots;
        for (const GameObject& object : scene.objects)
        {
            if (object.prefabInstance && object.prefabInstance->IsRoot() &&
                (path.empty() || object.prefabInstance->prefab == path) &&
                Scene::InstanceRootOf(scene, object.id) == object.id)
            {
                roots.push_back(object.id);
            }
        }
        bool changed = false;
        for (const std::uint32_t root : roots)
        {
            changed = UpdateInstance(scene, root, nextId) || changed;
        }
        return changed;
    }

    bool PrefabManager::UpdateInstance(SceneDescription& scene, std::uint32_t rootId, std::uint32_t& nextId)
    {
        const std::string path = Scene::FindObject(scene, rootId)->prefabInstance->prefab;
        const Scene::Prefab* prefab = Find(path);
        if (!prefab)
        {
            return false;
        }

        // Which scene object is each prefab object. One the prefab no longer has (or a second copy of the same one)
        // goes, with whatever is under it.
        bool changed = false;
        std::map<std::uint32_t, std::uint32_t> members; // prefab id -> scene id
        std::vector<std::uint32_t> stale;
        for (const std::uint32_t id : Scene::InstanceMembers(scene, rootId))
        {
            const std::uint32_t prefabId = Scene::FindObject(scene, id)->prefabInstance->object;
            if (prefab->Find(prefabId) && !members.contains(prefabId))
            {
                members[prefabId] = id;
            }
            else
            {
                stale.push_back(id);
            }
        }
        for (const std::uint32_t id : stale)
        {
            RemoveSubtree(scene, id);
            changed = true;
        }
        std::erase_if(members, [&scene](const auto& member) { return !Scene::FindObject(scene, member.second); });

        for (const GameObject& source : prefab->objects) // parents first
        {
            const auto found = members.find(source.id);
            if (found == members.end())
            {
                // A part the prefab gained (its parent is always in the instance by now: the root is never stale)
                GameObject copy = source;
                copy.id = nextId++;
                copy.parent = members.at(source.parent);
                copy.prefabInstance = Scene::PrefabInstance{.prefab = path, .object = source.id};
                members[source.id] = copy.id;
                scene.objects.push_back(std::move(copy));
                changed = true;
                continue;
            }

            GameObject& object = *Scene::FindObject(scene, found->second);
            json target = Scene::Detail::ToJson(object);
            const json from = Scene::Detail::ToJson(source);
            bool updated = false;
            for (const std::string& property : Diff(target, from, source.id == Scene::kPrefabRootId))
            {
                if (!IsOverridden(object.prefabInstance->overrides, property))
                {
                    CopyProperty(target, from, property);
                    updated = true;
                }
            }
            if (updated)
            {
                object = Scene::Detail::FromJson(target);
                changed = true;
            }
        }
        return changed;
    }

    bool PrefabManager::Revert(SceneDescription& scene, std::uint32_t objectId, const std::string& property)
    {
        GameObject* object = Scene::FindObject(scene, objectId);
        const GameObject* source = object ? PrefabObjectOf(*object) : nullptr;
        if (!source)
        {
            return false;
        }
        json target = Scene::Detail::ToJson(*object);
        const json from = Scene::Detail::ToJson(*source);
        bool reverted = false;
        for (const std::string& difference : Diff(target, from, source->id == Scene::kPrefabRootId))
        {
            if (property.empty() || difference == property || difference.starts_with(property + "/"))
            {
                CopyProperty(target, from, difference);
                reverted = true;
            }
        }
        if (!reverted)
        {
            return false;
        }
        *object = Scene::Detail::FromJson(target);
        if (object->prefabInstance)
        {
            object->prefabInstance->overrides = Differences(*object, *source);
        }
        return true;
    }

    void PrefabManager::ApplyToPrefab(SceneDescription& scene, std::uint32_t objectId, std::uint32_t& nextId)
    {
        const std::uint32_t rootId = Scene::InstanceRootOf(scene, objectId);
        if (rootId == 0)
        {
            throw std::invalid_argument("Only a part of a prefab instance can be applied to its prefab");
        }
        const std::string path = Scene::FindObject(scene, rootId)->prefabInstance->prefab;
        const Scene::Prefab* old = Find(path);
        if (!old)
        {
            throw std::runtime_error("The prefab " + path + " could not be read (see Console)");
        }

        // What every instance changed, measured against the prefab as it still is
        RefreshOverrides(scene);

        // Which instance each object belongs to, before any link changes. Whole instances of other prefabs added
        // under it become prefabs nested in this one, keeping their links, unless that would make the prefab contain
        // itself.
        const auto subtree = Scene::SubtreeOf(scene, rootId);
        std::map<std::uint32_t, std::uint32_t> instanceOf; // scene id -> its instance's root (0: none)
        std::map<std::uint32_t, Scene::PrefabInstance> nestedLinks;
        for (const std::uint32_t id : subtree)
        {
            instanceOf[id] = Scene::InstanceRootOf(scene, id);
        }
        for (const std::uint32_t id : subtree)
        {
            const std::uint32_t instanceRoot = instanceOf.at(id);
            if (instanceRoot == 0 || instanceRoot == rootId || !instanceOf.contains(instanceRoot))
            {
                continue;
            }
            const Scene::PrefabInstance& link = *Scene::FindObject(scene, id)->prefabInstance;
            if (Contains(link.prefab, path))
            {
                throw std::runtime_error("'" + Scene::FindObject(scene, instanceRoot)->name + "' is an instance of " +
                                         link.prefab + ", which is or contains " + path +
                                         ": a prefab cannot contain itself. Unpack it first.");
            }
            nestedLinks[id] = link;
        }

        // The instance's parts keep their prefab ids; objects added under it get new ones and join the instance
        std::uint32_t nextPrefabId = Scene::kPrefabRootId;
        for (const GameObject& source : old->objects)
        {
            nextPrefabId = std::max(nextPrefabId, source.id + 1);
        }
        std::map<std::uint32_t, std::uint32_t> ids; // scene id -> prefab id
        std::set<std::uint32_t> used;
        for (const std::uint32_t id : subtree)
        {
            GameObject& object = *Scene::FindObject(scene, id);
            const bool part = instanceOf.at(id) == rootId;
            if (part && used.insert(object.prefabInstance->object).second)
            {
                ids[id] = object.prefabInstance->object;
                continue;
            }
            ids[id] = nextPrefabId++;
            used.insert(ids[id]);
            object.prefabInstance = Scene::PrefabInstance{.prefab = path, .object = ids[id]};
        }

        // A variant stays one: its parts keep their links to its base (Store records what they now override), and
        // objects added to it are its own
        Scene::Prefab updated{.name = old->name, .base = old->base};
        for (const std::uint32_t id : subtree)
        {
            GameObject copy = *Scene::FindObject(scene, id);
            copy.id = ids.at(id);
            copy.parent = id == rootId ? 0 : ids.at(copy.parent);
            // A part keeps the link it has in the prefab (a variant's to its base, a nested prefab's to that prefab)
            const GameObject* was = old->Find(copy.id);
            const auto nested = nestedLinks.find(id);
            copy.prefabInstance = was                           ? was->prefabInstance
                                  : nested != nestedLinks.end() ? std::optional(nested->second)
                                                                : std::nullopt;
            if (id == rootId)
            {
                const GameObject& oldRoot = old->objects.front();
                copy.name = oldRoot.name;
                copy.transform = oldRoot.transform;
            }
            updated.objects.push_back(std::move(copy));
        }

        LOG_INFO(Prefab, "Applied '{}' to {} ({} objects)", Scene::FindObject(scene, rootId)->name, path,
                 updated.objects.size());
        Store(path, std::move(updated)); // throws if it cannot be written

        // Every instance: those of its variants follow it too
        UpdateInstances(scene, nextId);
        RefreshOverrides(scene);
    }

    bool PrefabManager::Unpack(SceneDescription& scene, std::uint32_t objectId)
    {
        const std::uint32_t rootId = Scene::InstanceRootOf(scene, objectId);
        if (rootId == 0)
        {
            return false;
        }
        for (const std::uint32_t id : Scene::InstanceMembers(scene, rootId))
        {
            Scene::FindObject(scene, id)->prefabInstance.reset();
        }
        return true;
    }

    bool PrefabManager::ApplyProperty(SceneDescription& scene, std::uint32_t objectId, const std::string& property,
                                      std::uint32_t& nextId)
    {
        const GameObject* object = Scene::FindObject(scene, objectId);
        if (!object || !object->prefabInstance)
        {
            throw std::invalid_argument("Only a part of a prefab instance can apply a change to its prefab");
        }
        const std::string path = object->prefabInstance->prefab;
        const Scene::Prefab* old = Find(path);
        if (!old)
        {
            throw std::runtime_error("The prefab " + path + " could not be read (see Console)");
        }
        if (!old->Find(object->prefabInstance->object))
        {
            throw std::runtime_error("The prefab " + path + " no longer has '" + object->name + "'");
        }

        // What every instance changed, measured against the prefab as it still is
        RefreshOverrides(scene);

        Scene::Prefab updated = *old;
        GameObject& target =
            *std::find_if(updated.objects.begin(), updated.objects.end(), [object](const GameObject& candidate)
                          { return candidate.id == object->prefabInstance->object; });
        json to = Scene::Detail::ToJson(target);
        const json from = Scene::Detail::ToJson(*object);
        bool applied = false;
        for (const std::string& difference : Diff(from, to, target.id == Scene::kPrefabRootId))
        {
            if (difference == property || difference.starts_with(property + "/"))
            {
                CopyProperty(to, from, difference);
                applied = true;
            }
        }
        if (!applied)
        {
            return false;
        }
        // Only properties were copied: the prefab object keeps its id, its parent and its link (a variant's, to its
        // base)
        const auto link = target.prefabInstance;
        target = Scene::Detail::FromJson(to);
        target.prefabInstance = link;

        LOG_INFO(Prefab, "Applied {} of '{}' to {}", property, object->name, path);
        Store(path, std::move(updated)); // throws if it cannot be written
        // Every instance: those of its variants follow it too
        UpdateInstances(scene, nextId);
        RefreshOverrides(scene);
        return true;
    }

    std::vector<std::uint32_t> PrefabManager::InstancesOf(const SceneDescription& scene, const std::string& path)
    {
        std::vector<std::uint32_t> roots;
        for (const GameObject& object : scene.objects)
        {
            if (object.prefabInstance && object.prefabInstance->IsRoot() && object.prefabInstance->prefab == path &&
                Scene::InstanceRootOf(scene, object.id) == object.id)
            {
                roots.push_back(object.id);
            }
        }
        return roots;
    }

    PrefabManager::DeleteResult PrefabManager::Delete(SceneDescription& scene, const std::string& path)
    {
        DeleteResult result;
        // The prefabs made from it first (its variants, and those it is nested in), while it can still bring them up
        // to date: each keeps the objects it has, no longer linked to it
        for (const std::string& otherPath : List())
        {
            const Scene::Prefab* found = otherPath != path ? Find(otherPath) : nullptr;
            if (!found || std::none_of(found->objects.begin(), found->objects.end(), [&path](const GameObject& object)
                                       { return object.prefabInstance && object.prefabInstance->prefab == path; }))
            {
                continue;
            }
            Scene::Prefab other = *found;
            if (other.base == path)
            {
                other.base.clear();
                ++result.variants;
            }
            else
            {
                ++result.containers;
            }
            for (GameObject& object : other.objects)
            {
                if (object.prefabInstance && object.prefabInstance->prefab == path)
                {
                    object.prefabInstance.reset();
                }
            }
            Store(otherPath, std::move(other)); // throws if it cannot be written
        }

        const auto roots = InstancesOf(scene, path);
        for (const std::uint32_t root : roots)
        {
            Unpack(scene, root);
        }
        result.instances = roots.size();
        std::error_code ec;
        std::filesystem::remove(projectDir_ / path, ec);
        if (ec)
        {
            throw std::runtime_error("Could not delete " + path + ": " + ec.message());
        }
        cache_.erase(path);
        LOG_INFO(Prefab,
                 "Deleted prefab {} ({} instances in the scene unpacked, {} variants made ordinary prefabs, {} prefabs "
                 "it was nested in keep its objects)",
                 path, result.instances, result.variants, result.containers);
        return result;
    }

    SceneDescription PrefabManager::ToScene(const Scene::Prefab& prefab)
    {
        // Links stay: a variant's scene is an instance of its base, and nested prefabs are instances under the root
        return SceneDescription{.objects = prefab.objects};
    }

    Scene::Prefab PrefabManager::FromScene(const SceneDescription& scene, std::string name)
    {
        const auto subtree = Scene::SubtreeOf(scene, Scene::kPrefabRootId);
        if (subtree.empty())
        {
            throw std::invalid_argument("The prefab has no root object (id " + std::to_string(Scene::kPrefabRootId) +
                                        ")");
        }
        // A root linked to a prefab's root: a variant of that prefab (unpacked, it is an ordinary prefab again)
        const GameObject* root = Scene::FindObject(scene, Scene::kPrefabRootId);
        const bool variant = root->prefabInstance && root->prefabInstance->IsRoot();
        Scene::Prefab prefab{.name = std::move(name), .base = variant ? root->prefabInstance->prefab : std::string{}};
        for (const std::uint32_t id : subtree) // parents first
        {
            GameObject copy = *Scene::FindObject(scene, id);
            if (id == Scene::kPrefabRootId)
            {
                copy.parent = 0;
            }
            // Kept: links of a variant's base instance (the root's) and of whole instances under the root (nested
            // prefabs); a part of an instance whose root is gone is an ordinary object
            const std::uint32_t instanceRoot = Scene::InstanceRootOf(scene, id);
            if (instanceRoot == 0 || (instanceRoot == Scene::kPrefabRootId && !prefab.IsVariant()))
            {
                copy.prefabInstance.reset();
            }
            prefab.objects.push_back(std::move(copy));
        }
        return prefab;
    }

    void PrefabManager::Save(const std::string& path, Scene::Prefab prefab)
    {
        Store(path, std::move(prefab)); // throws if no game is open or it cannot be written
    }
}
