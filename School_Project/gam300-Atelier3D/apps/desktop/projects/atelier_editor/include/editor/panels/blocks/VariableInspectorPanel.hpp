/* !
@file    	    VariableInspectorPanel.hpp
@date    		29/10/2026
@brief
    Declares VariableInspectorPanel, the Variable Inspector: the selected
    object's block variables and components with their live values while the
    game runs, what one Step changed, and which blocks set each value.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "editor/panels/Panel.hpp"
#include "engine/logic/Blocks.hpp"
#include "engine/logic/LogicRunner.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Engine::Game
{
    class GameRuntime;
}

namespace Engine::Scene
{
    struct SceneDescription;
    struct GameObject;
}

namespace Editor::Panels
{
    // The Variable Inspector (F03): the Inspector's contents while the game runs (and, if asked for, a window of its
    // own that floats over the editor):
    //
    //   v Variables
    //      Speed       7
    //      IsAlive     true
    //   v Transform                      <- the Inspector's own fields, locked, with the values the game has now
    //      Position    2, 0, -3
    //   v Light
    //      Intensity   1
    //
    // The block variables come first. Then comes whatever the Inspector draws for the object (the editor passes a
    // function that draws it, see SetDrawFields), so a component or field added to the Inspector is here with no
    // change to this panel. Hovering a value lists the blocks that set it and which ran last; clicking shows that
    // block in the Blocks window. It only reads: values are changed in the Inspector when the game is stopped, and
    // Pause / Step are the top bar's (the status bar says which block is next).
    class VariableInspectorPanel : public Panel
    {
    public:
        // Where one of the Inspector's fields was drawn (screen coordinates) and which saved fields it shows, by the
        // names the scene file uses ("transform": "x", "y", "z" for Position)
        struct FieldBox
        {
            std::string component;
            std::vector<std::string> fields;
            std::string label;
            float x0 = 0.0f;     // the row, from its label
            float xValue = 0.0f; // where its value starts
            float x1 = 0.0f;
            float y0 = 0.0f;
            float y1 = 0.0f;
        };

        VariableInspectorPanel();

        // What to show this frame: the scene, the selected object, and the running game (nullptr when not playing)
        void SetTarget(const Engine::Scene::SceneDescription* scene, std::uint32_t selected,
                       const Engine::Game::GameRuntime* runtime);
        void SetScale(float scale) { scale_ = scale; }
        // Called with an object and one of its blocks, to show the block in the Blocks window
        void SetOnOpen(std::function<void(std::uint32_t objectId, std::uint32_t blockId)> onOpen);
        // Draws the Inspector's fields for the object it is given (a copy with the game's values in it, which the
        // function may not keep) and fills in where each field is, so this panel can mark the ones that change
        void SetDrawFields(std::function<void(Engine::Scene::GameObject&, std::vector<FieldBox>&)> drawFields);

        void DrawContents() override;

    private:
        // One value shown
        struct Row
        {
            std::string key;             // unique in the panel: "variable/Speed", "Transform/Position", ...
            std::string label;           // e.g. "Speed"
            std::string text;            // the value as shown, e.g. "2, 0, -3"
            std::vector<double> numbers; // the exact value, to notice any change and show it in full
            std::string variable;        // a block variable: its name
            std::optional<Engine::Logic::Property> property; // what blocks write, if not a variable
            bool trueFalse = false;                          // a true / false variable (a number inside)
            std::string detail;    // its type and scope, after the label: "number, this object"
            bool measured = false; // read from the physics as it runs (velocity): no block or field sets it
        };
        struct Section
        {
            std::string name;
            std::vector<Row> rows;
        };

        // The block variables: the values that are not fields of the Inspector
        std::optional<Section> VariablesOf(const Engine::Scene::GameObject& object) const;
        // What the physics has for the object's rigid body while the game runs: its velocity
        std::optional<Section> PhysicsOf(const Engine::Scene::GameObject& object) const;
        // The object with the game's values in place of the saved ones
        Engine::Scene::GameObject LiveObject(const Engine::Scene::GameObject& object) const;
        // Explains the Inspector's fields just drawn, found in boxes: which blocks set them
        void MarkFields(const std::vector<FieldBox>& boxes, const Engine::Scene::GameObject& live);
        void DrawSection(const Section& section, const Engine::Scene::GameObject& object);
        void DrawRow(const Row& row, const Engine::Scene::GameObject& object);
        // Who sets the row's value: in a tooltip, or (interactive) as a list to pick a block from
        void DrawSetters(const Row& row, const Engine::Scene::GameObject& object, bool interactive);
        // The blocks that write the row's value, and the one that wrote it last
        std::vector<Engine::Logic::BlockAt> WritersOf(const Row& row, const Engine::Scene::GameObject& object) const;
        std::optional<Engine::Logic::LogicRunner::Write> LastWrite(const Row& row, std::uint32_t objectId) const;
        const Engine::Scene::GameObject* Find(std::uint32_t objectId) const;
        const Engine::Logic::LogicRunner* Runner() const;

        const Engine::Scene::SceneDescription* scene_ = nullptr;
        std::uint32_t selected_ = 0;
        const Engine::Game::GameRuntime* runtime_ = nullptr;
        float scale_ = 1.0f;
        std::function<void(std::uint32_t, std::uint32_t)> onOpen_;
        std::function<void(Engine::Scene::GameObject&, std::vector<FieldBox>&)> drawFields_;
    };
}
