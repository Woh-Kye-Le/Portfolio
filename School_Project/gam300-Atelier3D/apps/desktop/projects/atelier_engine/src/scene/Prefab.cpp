/* !
@file    	    Prefab.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		20/10/2026
@brief
    Implements the prefab file format: the version check, the object list
    shared with scene files, and fixing the root and the order of the objects.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/scene/Prefab.hpp"

#include "GameObjectJson.hpp"

#include "engine/core/Log.hpp"
#include "engine/scene/Hierarchy.hpp"
#include "engine/scene/PrefabInstance.hpp"
#include "engine/serializer/JsonSerializer.hpp"

#include <algorithm>
#include <exception>
#include <format>
#include <utility>

namespace Engine::Scene
{
    namespace
    {
        using Serializer::JSONSerializer;

        // The root first with id kPrefabRootId, everything under it, each object after its parent. The only links
        // are a variant's to its base (base is cleared if the variant's root is not linked to the base's) and those
        // of nested prefabs: whole instances of another prefab under the root
        void FixLayout(std::vector<GameObject>& objects, std::string& base, SceneLoadReport& report)
        {
            GameObject& root = objects.front();
            if (root.parent != 0)
            {
                report.warnings.push_back(
                    std::format("object 0 ({}): the root has no parent - parent dropped", root.name));
                root.parent = 0;
            }
            if (root.id != kPrefabRootId)
            {
                // Swap ids with the object that has the root's id, so every other id (instances refer to them) stays
                const std::uint32_t old = root.id;
                for (GameObject& object : objects)
                {
                    object.id = object.id == kPrefabRootId ? old : object.id == old ? kPrefabRootId : object.id;
                    object.parent = object.parent == kPrefabRootId ? old
                                    : object.parent == old         ? kPrefabRootId
                                                                   : object.parent;
                }
                report.warnings.push_back(std::format("object 0 ({}): the root's id must be {} - ids {} and {} swapped",
                                                      root.name, kPrefabRootId, old, kPrefabRootId));
            }
            for (std::size_t i = 1; i < objects.size(); ++i)
            {
                if (objects[i].parent == 0)
                {
                    report.warnings.push_back(
                        std::format("object {} ({}): not under the root - put under it", i, objects[i].name));
                    objects[i].parent = kPrefabRootId;
                }
            }
            // A variant's root is linked to its base's root; an ordinary prefab's root is not linked at all
            if (!base.empty() && !(root.prefabInstance && root.prefabInstance->prefab == base &&
                                   root.prefabInstance->object == kPrefabRootId))
            {
                report.warnings.push_back(std::format("object 0 ({}): the root of a variant of {} must be linked to "
                                                      "its root - loaded as an ordinary prefab",
                                                      root.name, base));
                base.clear();
            }
            if (base.empty() && root.prefabInstance)
            {
                report.warnings.push_back(std::format("object 0 ({}): the root of a prefab that is not a variant is "
                                                      "not linked to a prefab - link dropped",
                                                      root.name));
                root.prefabInstance.reset();
            }

            // Every object now leads up to the root, so the root's subtree is all of them, parents first
            SceneDescription scene{.objects = std::move(objects)};
            objects.clear();
            for (const std::uint32_t id : SubtreeOf(scene, kPrefabRootId))
            {
                objects.push_back(*FindObject(scene, id));
            }

            // A linked object must be part of a whole instance: the variant's of its base (under the root, which is
            // that instance's root) or a nested prefab's (with its own root under the prefab's root). Decided before
            // any link is dropped, since dropping one changes which instance the objects under it belong to.
            const SceneDescription laidOut{.objects = objects};
            for (std::size_t i = 0; i < objects.size(); ++i)
            {
                GameObject& object = objects[i];
                if (!object.prefabInstance)
                {
                    continue;
                }
                const std::uint32_t instanceRoot = InstanceRootOf(laidOut, object.id);
                const bool ofBase = instanceRoot == kPrefabRootId && !base.empty();
                const bool nested = instanceRoot != 0 && instanceRoot != kPrefabRootId;
                if (!ofBase && !nested)
                {
                    report.warnings.push_back(
                        std::format("object {} ({}): linked to {}, but not part of a whole instance of it (or of "
                                    "the base) - link dropped",
                                    i, object.name, object.prefabInstance->prefab));
                    object.prefabInstance.reset();
                }
            }
        }

        Prefab Parse(const std::filesystem::path& path, SceneLoadReport& report)
        {
            const auto fail = [&](std::string error)
            {
                report.ok = false;
                report.error = std::move(error);
                return Prefab{};
            };

            std::error_code ec;
            if (!std::filesystem::is_regular_file(path, ec))
            {
                return fail("file not found: " + path.string());
            }
            JSONSerializer json;
            try
            {
                json.LoadFile(path.string());
            }
            catch (const std::exception& e)
            {
                return fail(std::string("not valid JSON - ") + e.what());
            }
            if (!json.GetCurrentNode().is_object())
            {
                return fail("the file must contain a JSON object: { \"version\": ..., \"objects\": [ ... ] }");
            }
            int version = 0;
            if (!json.TryReadInt("version", version) || version != kPrefabFormatVersion)
            {
                return fail(std::format("missing or unsupported \"version\" (this build reads version {})",
                                        kPrefabFormatVersion));
            }
            if (!json.HasArray("objects"))
            {
                return fail("missing the \"objects\" list: { \"objects\": [ ... ] }");
            }

            Prefab prefab;
            prefab.name = path.stem().string();
            if (json.HasKey("name") && !json.TryReadString("name", prefab.name))
            {
                report.warnings.push_back("\"name\" is not text - using " + prefab.name);
            }
            if (json.HasKey("guid") && !json.TryReadString("guid", prefab.guid))
            {
                report.warnings.push_back("\"guid\" is not text - a new one is made when it is saved");
                prefab.guid.clear();
            }
            if (json.HasKey("base") && !json.TryReadString("base", prefab.base))
            {
                report.warnings.push_back("\"base\" is not text - loaded as an ordinary prefab");
                prefab.base.clear();
            }
            prefab.objects = Detail::ReadObjects(json, report);
            if (prefab.objects.empty())
            {
                return fail("the prefab has no objects");
            }
            FixLayout(prefab.objects, prefab.base, report);
            report.ok = true;
            return prefab;
        }
    }

    const GameObject* Prefab::Find(std::uint32_t id) const
    {
        const auto found =
            std::find_if(objects.begin(), objects.end(), [id](const GameObject& object) { return object.id == id; });
        return found != objects.end() ? &*found : nullptr;
    }

    SceneLoadReport LoadPrefabFile(const std::filesystem::path& path, Prefab& prefab)
    {
        SceneLoadReport report;
        try
        {
            Prefab loaded = Parse(path, report);
            if (report.ok)
            {
                prefab = std::move(loaded);
            }
        }
        catch (const std::exception& e) // never expected; a bad prefab must not take the editor down
        {
            report.ok = false;
            report.error = e.what();
        }

        if (!report.ok)
        {
            LOG_ERROR(IO, "Prefab {}: {}", path.string(), report.error);
        }
        for (const auto& warning : report.warnings)
        {
            LOG_ERROR(IO, "Prefab {}: {}", path.string(), warning);
        }
        return report;
    }

    void SavePrefabFile(const std::filesystem::path& path, const Prefab& prefab)
    {
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }
        JSONSerializer json;
        json.ParseJSON("{}");
        json.WriteInt("version", kPrefabFormatVersion);
        json.WriteString("name", prefab.name);
        if (!prefab.guid.empty())
        {
            json.WriteString("guid", prefab.guid);
        }
        if (prefab.IsVariant())
        {
            json.WriteString("base", prefab.base);
        }
        Detail::WriteObjects(json, prefab.objects);
        json.SaveFile(path.string()); // throws std::runtime_error if it cannot be written
    }
}
