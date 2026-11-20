/* !
@file    	    SceneCommands.cpp
@author  	    sanni.go@digipen.edu [100%]
@date    		25/10/2026
@brief
    Implements the scene commands: create/delete game objects, gizmo
    transforms and property changes, each with its undo.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/history/SceneCommands.hpp"

#include "engine/scene/Hierarchy.hpp"

#include <algorithm>
#include <utility>

namespace Engine::History
{
    ObjectSubtree ObjectSubtree::Capture(const Editor::EditorScene& scene, std::uint32_t id)
    {
        ObjectSubtree subtree;
        for (const std::uint32_t member : Scene::SubtreeOf(scene.Description(), id))
        {
            subtree.entries.push_back({scene.IndexOf(member), *scene.Find(member)});
        }
        if (!subtree.entries.empty())
        {
            subtree.root = id;
            subtree.name = subtree.entries.front().object.name;
        }
        // In list order: inserting each at its old index then puts every one back exactly where it was
        std::sort(subtree.entries.begin(), subtree.entries.end(),
                  [](const Entry& a, const Entry& b) { return a.index < b.index; });
        return subtree;
    }

    void ObjectSubtree::Insert(Editor::EditorScene& scene) const
    {
        for (const Entry& entry : entries)
        {
            scene.Insert(entry.object, entry.index);
        }
        if (root != 0)
        {
            scene.Select(root);
        }
    }

    CreateObjectCommand::CreateObjectCommand(Editor::EditorScene& scene, std::uint32_t id)
        : scene_(scene), objects_(ObjectSubtree::Capture(scene, id))
    {
    }

    void CreateObjectCommand::Execute()
    {
        objects_.Insert(scene_);
    }

    void CreateObjectCommand::Undo()
    {
        if (objects_.root != 0)
        {
            scene_.Remove(objects_.root);
        }
    }

    std::string CreateObjectCommand::Name() const
    {
        return "Create " + objects_.name;
    }

    DeleteObjectCommand::DeleteObjectCommand(Editor::EditorScene& scene, std::uint32_t id)
        : scene_(scene), objects_(ObjectSubtree::Capture(scene, id))
    {
    }

    void DeleteObjectCommand::Execute()
    {
        if (objects_.root != 0)
        {
            scene_.Remove(objects_.root);
        }
    }

    // Back in the Hierarchy and selected so changes can be seen
    void DeleteObjectCommand::Undo()
    {
        objects_.Insert(scene_);
    }

    std::string DeleteObjectCommand::Name() const
    {
        return "Delete " + objects_.name;
    }

    TransformCommand::TransformCommand(Editor::EditorScene& scene, std::uint32_t id, Scene::TransformComponent before,
                                       Scene::TransformComponent after, std::string name)
        : scene_(scene), id_(id), before_(before), after_(after), name_(std::move(name))
    {
    }

    void TransformCommand::Execute()
    {
        Apply(after_);
    }

    void TransformCommand::Undo()
    {
        Apply(before_);
    }

    std::string TransformCommand::Name() const
    {
        return name_;
    }

    // Jic the object lost its Transform
    void TransformCommand::Apply(const Scene::TransformComponent& transform)
    {
        if (Scene::GameObject* object = scene_.Find(id_))
        {
            object->transform = transform;
        }
    }

    PropertyChangeCommand::PropertyChangeCommand(Editor::EditorScene& scene, std::vector<Change> changes,
                                                 std::string name)
        : scene_(scene), changes_(std::move(changes)), name_(std::move(name))
    {
    }

    void PropertyChangeCommand::Execute()
    {
        for (const Change& change : changes_)
        {
            if (Scene::GameObject* object = scene_.Find(change.after.id))
            {
                *object = change.after;
            }
        }
    }

    void PropertyChangeCommand::Undo()
    {
        for (const Change& change : changes_)
        {
            if (Scene::GameObject* object = scene_.Find(change.before.id))
            {
                *object = change.before;
            }
        }
    }

    std::string PropertyChangeCommand::Name() const
    {
        if (!name_.empty())
        {
            return name_;
        }
        if (changes_.size() == 1)
        {
            return "Edit " + changes_.front().after.name;
        }
        return "Edit " + std::to_string(changes_.size()) + " objects";
    }

    std::string BlockEditName(const std::optional<Logic::LogicDesc>& before,
                              const std::optional<Logic::LogicDesc>& after)
    {
        const std::size_t was = before ? Logic::CountBlocks(*before) : 0;
        const std::size_t is = after ? Logic::CountBlocks(*after) : 0;
        if (was == is)
        {
            return "Edit blocks";
        }
        const std::size_t count = is > was ? is - was : was - is;
        const std::string what = count == 1 ? "block" : std::to_string(count) + " blocks";
        return (is > was ? "Place " : "Delete ") + what;
    }

    BlockEditCommand::BlockEditCommand(Find find, std::optional<Logic::LogicDesc> before,
                                       std::optional<Logic::LogicDesc> after)
        : find_(std::move(find)), before_(std::move(before)), after_(std::move(after))
    {
    }

    void BlockEditCommand::Execute()
    {
        if (std::optional<Logic::LogicDesc>* logic = find_())
        {
            *logic = after_;
        }
    }

    void BlockEditCommand::Undo()
    {
        if (std::optional<Logic::LogicDesc>* logic = find_())
        {
            *logic = before_;
        }
    }

    std::string BlockEditCommand::Name() const
    {
        return BlockEditName(before_, after_);
    }

    std::unique_ptr<ICommand> MakeEditCommand(Editor::EditorScene& scene, const Editor::EditorScene::State& before,
                                              const Editor::EditorScene::State& now)
    {
        if (now == before)
        {
            return nullptr;
        }
        // Only changed objects (the same ones, in the same order) and nothing else about the scene: property changes
        const bool sameObjects =
            std::equal(before.scene.objects.begin(), before.scene.objects.end(), now.scene.objects.begin(),
                       now.scene.objects.end(),
                       [](const Scene::GameObject& a, const Scene::GameObject& b) { return a.id == b.id; });
        Scene::SceneDescription withEdits = before.scene;
        withEdits.objects = now.scene.objects;
        if (!sameObjects || withEdits != now.scene)
        {
            return std::make_unique<SceneStateCommand>(scene, before, now, "Edit scene");
        }
        std::vector<PropertyChangeCommand::Change> changes;
        for (std::size_t i = 0; i < now.scene.objects.size(); ++i)
        {
            if (before.scene.objects[i] != now.scene.objects[i])
            {
                changes.push_back({before.scene.objects[i], now.scene.objects[i]});
            }
        }
        if (changes.empty())
        {
            return nullptr; // only something undo does not keep changed
        }
        std::string name;
        if (changes.size() == 1 && changes.front().before.logic != changes.front().after.logic)
        {
            // Nothing but its blocks (and what an instance of a prefab records about them)
            Scene::GameObject same = changes.front().before;
            same.logic = changes.front().after.logic;
            same.prefabInstance = changes.front().after.prefabInstance;
            if (same == changes.front().after)
            {
                name = BlockEditName(changes.front().before.logic, changes.front().after.logic) + " on " +
                       changes.front().after.name;
            }
        }
        return std::make_unique<PropertyChangeCommand>(scene, std::move(changes), std::move(name));
    }

    SceneStateCommand::SceneStateCommand(Editor::EditorScene& scene, Editor::EditorScene::State before,
                                         Editor::EditorScene::State after, std::string name)
        : scene_(scene), before_(std::move(before)), after_(std::move(after)), name_(std::move(name))
    {
    }

    void SceneStateCommand::Execute()
    {
        scene_.RestoreState(after_);
    }

    void SceneStateCommand::Undo()
    {
        scene_.RestoreState(before_);
    }

    std::string SceneStateCommand::Name() const
    {
        return name_;
    }
}
