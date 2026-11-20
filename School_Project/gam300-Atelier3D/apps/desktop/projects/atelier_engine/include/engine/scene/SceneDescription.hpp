/* !
@file    	    SceneDescription.hpp
@author  	    c.kojat@digipen.edu [63%]
         	    k.thantwin@digipen.edu [12%]
         	    sung.h@digipen.edu [9%]
         	    sanni.go@digipen.edu [6%]
         	    w.kyele@digipen.edu [6%]
         	    j.cai@digipen.edu [4%]
@date    		02/10/2026
@brief
    Declares SceneDescription and GameObject, the authored scene shared by the
    scene file, the editor and the runtime: game objects with optional
    components.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/game/Shape.hpp"
#include "engine/logic/Blocks.hpp"
#include "engine/scene/PrefabInstance.hpp"
#include "engine/scene/components/AnimatorComponent.hpp"
#include "engine/scene/components/CameraComponent.hpp"
#include "engine/scene/components/LightComponent.hpp"
#include "engine/scene/components/PlayerControllerComponent.hpp"
#include "engine/scene/components/RigidBodyComponent.hpp"
#include "engine/scene/components/ScreenElementComponent.hpp"
#include "engine/scene/components/SkyComponent.hpp"
#include "engine/scene/components/TransformComponent.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Engine::Scene
{
    // A scene as it is saved and edited: game objects and the components they have. The scene file, the editor and
    // the game all use it, and GameRuntime turns it into ECS entities (the only place that happens).
    //
    // Components that are plain data are the ECS's own (TransformComponent, CameraComponent, LightComponent,
    // PlayerControllerComponent, RigidBodyComponent). The two that refer to assets have a saved form
    // of their own here, holding file paths where the ECS component holds loaded handles: RenderableDesc ->
    // MeshRendererComponent (+ SpinComponent) and AudioSourceDesc -> AudioSourceComponent.

    // What an object draws: a built-in shape, a whole model file, or a single mesh file, with an optional texture,
    // shader and tint. Paths are relative to the game folder; empty means none.
    struct RenderableDesc
    {
        Game::Shape shape = Game::Shape::Cube;
        float rotationSpeed = 0.0f; // degrees per second around Y in Play mode (the ECS SpinComponent); 0: still
        std::string mesh;           // one mesh, drawn with the texture below
        std::string model;          // a whole model: its own meshes, materials and textures
        std::string texture;
        std::string shader;
        std::string material;   // a .amat: its shader, colour, texture... (in place of the texture and shader)
        float modelTurn = 0.0f; // degrees the model or mesh is turned round Y inside the object (a model made facing
                                // the other way), without turning the object, nor the way it walks
        std::string holdBone;   // a bone of its parent's model it is held in as that animates ("handslot.r")
        // Its Transform is relative to the camera, and it is drawn over everything else: first-person arms and the
        // gun they hold, which then never go into a wall. A child of a view model is one too.
        bool viewModel = false;
        float albedoR = 1.0f;
        float albedoG = 1.0f;
        float albedoB = 1.0f;
        float albedoA = 1.0f;
        // A built-in shape's size and centre inside its object, before the Transform scales it: what a model it
        // stands in for takes up (a crate 1 x 1 x 1 standing on the ground: size 1, centre y 0.5). Not used for a
        // mesh or model file.
        float shapeSizeX = 1.0f;
        float shapeSizeY = 1.0f;
        float shapeSizeZ = 1.0f;
        float shapeCenterX = 0.0f;
        float shapeCenterY = 0.0f;
        float shapeCenterZ = 0.0f;

        bool operator==(const RenderableDesc&) const = default;
    };

    // A sound the object emits, placed at its Transform. Independent of the renderable on purpose: an object can be
    // heard without being drawn (room ambience, background music), and drawn without being heard.
    struct AudioSourceDesc
    {
        std::string clip;    // path relative to the game folder; empty = silent
        float volume = 1.0f; // 0 to 1
        bool loop = true;
        bool playOnStart = true;   // off: the clip is assigned but nothing plays yet
        bool spatial = true;       // off: plays flat, the same wherever the listener is
        float minDistance = 2.0f;  // full volume within this radius of the object
        float maxDistance = 50.0f; // silent from here; fades evenly between the two
        std::string bus = "sfx";   // "sfx" or "music" (Audio::BusFromString reads it)

        bool operator==(const AudioSourceDesc&) const = default;
    };

    // A named container with no built-in data: an object has a position only once it is given a Transform, and is
    // drawn only once it has a Transform and a renderable. Components it does not have hold no data at all.
    //
    // Objects form a tree through parent: a child's Transform is relative to its parent's (Scene/Hierarchy.hpp works
    // out where it is in the world), so moving a parent moves its children with it. A parent without a Transform
    // places its children as if it were at the origin.
    struct GameObject
    {
        std::uint32_t id = 0; // unique within its scene; the matching ECS entity is found by it
        std::string name;
        std::string luaFile;
        std::string luaSource;    // Empty means no Lua script.
        std::uint32_t parent = 0; // id of the parent object; 0 = at the top of the scene
        std::optional<TransformComponent> transform;
        std::optional<RenderableDesc> renderable;
        std::optional<AudioSourceDesc> audioSource;
        std::optional<PlayerControllerComponent> player; // the object the move keys drive in Play mode
        std::optional<CameraComponent> camera;           // the primary camera is the one Play mode renders with
        std::optional<LightComponent> light;
        std::optional<RigidBodyComponent> rigidBody;  // a body in the physics simulation in Play mode
        std::optional<Logic::LogicDesc> logic;        // its block scripts (the Blocks window), run in Play mode
        std::optional<PrefabInstance> prefabInstance; // set on objects made from a prefab (see PrefabInstance.hpp)
        std::optional<ScreenElementComponent> screen; // a text, picture, bar or button on the game's screen
        std::optional<AnimatorComponent> animator;    // plays an animation of its model
        std::optional<SkyComponent> sky;              // the sky the scene is drawn in

        bool operator==(const GameObject&) const = default;
    };

    // A whole scene: what a scene file holds, what the editor edits and what GameRuntime plays
    struct SceneDescription
    {
        std::vector<GameObject> objects;

        bool operator==(const SceneDescription&) const = default;
    };
}
