/* !
@file    	    RigidBodyComponent.hpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		26/10/2026
@brief
    RigidBodyComponent: makes an object part of the physics simulation in Play
    mode, as a static, dynamic or kinematic body, with a box, sphere or capsule
    collider (or one fitted to what it draws).

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Scene
{
    // How a rigid body moves in Play mode
    enum class BodyType : std::uint8_t
    {
        Static,    // never moves: floors, walls, platforms. Other bodies land on it and bump into it
        Dynamic,   // moved by the physics: falls, is pushed, comes to rest on things
        Kinematic, // moved by its Transform (by other systems or game code), and pushes dynamic bodies away
    };

    // The name a body type has in scene files: "static", "dynamic" or "kinematic"
    inline const char* BodyTypeName(BodyType type)
    {
        switch (type)
        {
        case BodyType::Static: return "static";
        case BodyType::Kinematic: return "kinematic";
        case BodyType::Dynamic: break;
        }
        return "dynamic";
    }

    // Reads a body type as scene files write it; nullopt if the name is not one
    inline std::optional<BodyType> ParseBodyType(std::string_view name)
    {
        for (const BodyType type : {BodyType::Static, BodyType::Dynamic, BodyType::Kinematic})
        {
            if (name == BodyTypeName(type))
            {
                return type;
            }
        }
        return std::nullopt;
    }

    // The shape a rigid body collides with
    enum class ColliderShape : std::uint8_t
    {
        Auto,     // fitted to what the object draws: a sphere for a sphere, a box for anything else
        Box,      // RigidBodyComponent::size
        Sphere,   // RigidBodyComponent::radius
        Capsule,  // RigidBodyComponent::radius and height, standing along the object's Y axis
        Mesh,     // the triangles of its model or mesh (the hull around them on a dynamic body); built-in shapes: Auto
        Compound, // RigidBodyComponent::parts, moving as one
    };

    // The surfaces the Inspector offers (any name works: "play footstep" plays step_<name> sounds)
    inline constexpr std::array<std::string_view, 8> kSurfaces{"",     "grass", "stone", "concrete",
                                                               "wood", "sand",  "metal", "water"};

    // The name a collider shape has in scene files: "auto", "box", "sphere" or "capsule"
    inline const char* ColliderShapeName(ColliderShape shape)
    {
        switch (shape)
        {
        case ColliderShape::Box: return "box";
        case ColliderShape::Sphere: return "sphere";
        case ColliderShape::Capsule: return "capsule";
        case ColliderShape::Mesh: return "mesh";
        case ColliderShape::Compound: return "compound";
        case ColliderShape::Auto: break;
        }
        return "auto";
    }

    // Reads a collider shape as scene files write it; nullopt if the name is not one
    inline std::optional<ColliderShape> ParseColliderShape(std::string_view name)
    {
        for (const ColliderShape shape : {ColliderShape::Auto, ColliderShape::Box, ColliderShape::Sphere,
                                          ColliderShape::Capsule, ColliderShape::Mesh, ColliderShape::Compound})
        {
            if (name == ColliderShapeName(shape))
            {
                return shape;
            }
        }
        return std::nullopt;
    }

    // One shape of a Compound collider, in the object's own units like the rest of the collider
    struct ColliderPart
    {
        ColliderShape shape = ColliderShape::Box; // Box, Sphere or Capsule
        float centerX = 0.0f;
        float centerY = 0.0f;
        float centerZ = 0.0f;
        float sizeX = 1.0f;
        float sizeY = 1.0f;
        float sizeZ = 1.0f;
        float radius = 0.5f;
        float height = 2.0f;

        bool operator==(const ColliderPart&) const = default;
    };

    // Makes an object a body in the physics simulation (Systems::PhysicsSystem). It needs a Transform.
    //
    // The collider's sizes are in the object's own units: the object's Transform scales them, so a box of size 1 on
    // an object scaled 2 is 2 wide. A sphere can only be round, so it takes the largest scale; a capsule's radius
    // takes the larger of the X and Z scales, and its height the Y scale.
    struct RigidBodyComponent
    {
        BodyType type = BodyType::Dynamic;
        float mass = 1.0f;         // in kilograms; only dynamic bodies use it
        float gravityScale = 1.0f; // how much of the game's gravity pulls it (dynamic only): 0 floats, 2 falls twice
                                   // as fast, below 0 rises

        // What happens when it touches another body (any type). Of two bodies, the slipperier friction and the
        // bouncier bounciness count, so an icy floor is slippery for everything and a bouncy ball bounces anywhere.
        float friction = 0.5f;   // how much it grips what it slides on: 0 ice, 0.5 most things, 1 rubber
        float bounciness = 0.0f; // how much of its speed it bounces back with: 0 none, 1 back as high as it fell
        // What it is made of, for what walks on it: "play footstep" plays the game's step_<surface> sounds
        // ("stone": step_stone_1, step_stone_2, ...). Empty: step_default.
        std::string surface;

        // Stays standing whatever hits it, like a player or something walking (a character standing still)
        bool keepUpright = false;
        // Dynamic only: what it may not do, in the world's axes
        bool lockMoveX = false;
        bool lockMoveY = false;
        bool lockMoveZ = false;
        bool lockTurnX = false;
        bool lockTurnY = false;
        bool lockTurnZ = false;
        // The layers it is on, one bit each, for rays that only look at some: 1 is the first layer
        int layers = 1;

        // How fast and which way it is already moving when Play starts (dynamic only), in units per second: a thrown
        // ball, a shot. Changing it while playing does not throw it again.
        float startVelocityX = 0.0f;
        float startVelocityY = 0.0f;
        float startVelocityZ = 0.0f;

        ColliderShape shape = ColliderShape::Auto;
        float centerX = 0.0f; // the collider's centre relative to the object
        float centerY = 0.0f;
        float centerZ = 0.0f;
        float sizeX = 1.0f; // Box: its full size on each axis
        float sizeY = 1.0f;
        float sizeZ = 1.0f;
        float radius = 0.5f;             // Sphere, Capsule
        float height = 2.0f;             // Capsule: from end to end, rounded ends included
        std::vector<ColliderPart> parts; // Compound

        bool operator==(const RigidBodyComponent&) const = default;
    };
}
