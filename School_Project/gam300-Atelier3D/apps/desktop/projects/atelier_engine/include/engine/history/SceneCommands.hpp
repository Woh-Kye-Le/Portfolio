/* !
@file    	    SceneCommands.hpp
@author  	    sanni.go@digipen.edu [100%]
@date    		25/10/2026
@brief
    Declares the undoable operations on the scene open in the editor: creating
    and deleting game objects, moving / rotating / scaling one with the gizmo,
    and changing property values in the Inspector.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#pragma once

#include "engine/editor/EditorScene.hpp"
#include "engine/history/ICommand.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Engine::History
{
    // An object and everything under it, each with its place in the scene's object list (in list order), so they
    // can be put back exactly where they were
    struct ObjectSubtree
    {
        struct Entry
        {
            std::size_t index = 0;
            Scene::GameObject object;
        };

        std::uint32_t root = 0;
        std::string name; // the root's
        std::vector<Entry> entries;

        // Copies id and everything under it out of the scene (empty if there is no such object)
        static ObjectSubtree Capture(const Editor::EditorScene& scene, std::uint32_t id);
        // Puts them all back with their own ids and selects the root
        void Insert(Editor::EditorScene& scene) const;
    };

    // Create, Duplicate, Paste, Add Player. Undo removes it, redo puts the same
    // objects (same ids and components) back at the same places in the list
    class CreateObjectCommand : public ICommand
    {
    public:
        // The new object, already in the scene; everything under it (a duplicate's or a paste's copies) comes too
        CreateObjectCommand(Editor::EditorScene& scene, std::uint32_t id);

        void Execute() override;
        void Undo() override;
        std::string Name() const override;

    private:
        Editor::EditorScene& scene_;
        ObjectSubtree objects_;
    };

    // Keeps a copy of the object and everything under it (Remove takes those too) so undo can bring them back
    // where they were
    class DeleteObjectCommand : public ICommand
    {
    public:
        DeleteObjectCommand(Editor::EditorScene& scene, std::uint32_t id);

        void Execute() override;
        void Undo() override;
        std::string Name() const override;

    private:
        Editor::EditorScene& scene_;
        ObjectSubtree objects_;
    };

    // Object's Transform (position, rotation, scale) before and after
    class TransformCommand : public ICommand
    {
    public:
        TransformCommand(Editor::EditorScene& scene, std::uint32_t id, Scene::TransformComponent before,
                         Scene::TransformComponent after, std::string name);

        void Execute() override;
        void Undo() override;
        std::string Name() const override;

    private:
        void Apply(const Scene::TransformComponent& transform);

        Editor::EditorScene& scene_;
        std::uint32_t id_;
        Scene::TransformComponent before_;
        Scene::TransformComponent after_;
        std::string name_;
    };

    // Each changed object before and after
    class PropertyChangeCommand : public ICommand
    {
    public:
        struct Change
        {
            Scene::GameObject before;
            Scene::GameObject after;
        };

        // name: what Undo and Redo call it (empty: "Edit <object>")
        PropertyChangeCommand(Editor::EditorScene& scene, std::vector<Change> changes, std::string name = {});

        void Execute() override;
        void Undo() override;
        std::string Name() const override;

    private:
        Editor::EditorScene& scene_;
        std::vector<Change> changes_;
        std::string name_;
    };

    // What a change of blocks did, for its undo step: "Place block", "Delete 3 blocks", or "Edit blocks" when as many
    // are there as before (moved, or a field changed)
    std::string BlockEditName(const std::optional<Logic::LogicDesc>& before,
                              const std::optional<Logic::LogicDesc>& after);

    // Blocks that are not in the scene (a My Block open in its window), where find says they are now
    class BlockEditCommand : public ICommand
    {
    public:
        using Find = std::function<std::optional<Logic::LogicDesc>*()>;

        BlockEditCommand(Find find, std::optional<Logic::LogicDesc> before, std::optional<Logic::LogicDesc> after);

        void Execute() override;
        void Undo() override;
        std::string Name() const override;

    private:
        Find find_;
        std::optional<Logic::LogicDesc> before_;
        std::optional<Logic::LogicDesc> after_;
    };

    // The whole scene before and after: for edits that add, remove or reorder objects without a command of their
    // own (placing a prefab, Apply to Prefab, Unpack, instances updated from their prefab, reparenting)
    class SceneStateCommand : public ICommand
    {
    public:
        SceneStateCommand(Editor::EditorScene& scene, Editor::EditorScene::State before,
                          Editor::EditorScene::State after, std::string name);

        void Execute() override;
        void Undo() override;
        std::string Name() const override;

    private:
        Editor::EditorScene& scene_;
        Editor::EditorScene::State before_;
        Editor::EditorScene::State after_;
        std::string name_;
    };

    // The undo step for whatever changed in the scene from before to now, for edits with no command of their own:
    // nullptr if nothing did. Blocks changed on one object are named for what happened to them ("Place block on
    // Player"); other changes to the same objects are property changes; anything that also added, removed or reordered
    // objects is a snapshot of the whole scene. The change has already been made.
    std::unique_ptr<ICommand> MakeEditCommand(Editor::EditorScene& scene, const Editor::EditorScene::State& before,
                                              const Editor::EditorScene::State& now);
}
