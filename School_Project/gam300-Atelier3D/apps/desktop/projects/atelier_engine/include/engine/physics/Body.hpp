/* !
@file    	    Body.hpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		26/10/2026
@brief
    Declares the types PhysicsWorld uses for bodies: the BodyId handle, the
    motion types, colliders (box, sphere, capsule), surface materials (friction
    and bounciness), the settings a body is made from, a body's pose, the
    contact events reported when bodies start or stop touching, what a ray
    hits, how far apart bodies are, and the ground a body stands on.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <vector>

namespace Engine::Physics
{
    // A body in a PhysicsWorld. Default-constructed is invalid. An id whose body was removed stays invalid even if the
    // world reuses its slot, so a stale id never reaches another body.
    struct BodyId
    {
        static constexpr std::uint32_t kInvalid = 0xFFFFFFFFu;

        std::uint32_t value = kInvalid; // Jolt's BodyID: slot index and sequence number

        bool IsValid() const { return value != kInvalid; }
        bool operator==(const BodyId&) const = default;
    };

    // How a body moves
    enum class MotionType : std::uint8_t
    {
        Static,    // never moves on its own; moving it places it somewhere else
        Kinematic, // moved by the game, not by forces; pushes dynamic bodies out of its way
        Dynamic,   // moved by the simulation: gravity, collisions
    };

    // The shape a body collides with
    enum class ShapeType : std::uint8_t
    {
        Box,
        Sphere,
        Capsule,  // a cylinder with a half sphere on each end, standing along the body's Y axis
        Mesh,     // triangles, for static and kinematic bodies; a dynamic body gets the hull around them
        Compound, // several shapes moving as one
    };

    // The collision layers a body is on and a query looks at, one bit each
    inline constexpr std::uint32_t kDefaultLayer = 1u;
    inline constexpr std::uint32_t kAllLayers = 0xFFFFFFFFu;

    // What a dynamic body may not do, in the world's axes. Locking everything is refused (logged): make it static.
    struct AxisLocks
    {
        bool moveX = false;
        bool moveY = false;
        bool moveZ = false;
        bool turnX = false;
        bool turnY = false;
        bool turnZ = false;

        bool operator==(const AxisLocks&) const = default;
    };

    // A body's collision shape, in world units (already scaled), in the body's own frame. Only the fields of its
    // shape are used.
    struct Collider
    {
        ShapeType shape = ShapeType::Box;
        glm::vec3 halfExtents{0.5f};      // Box: half its size on each axis
        float radius = 0.5f;              // Sphere, Capsule
        float halfHeight = 0.5f;          // Capsule: half the length of its straight middle, without the rounded ends
        glm::vec3 center{0.0f};           // the shape's centre relative to the body's position, before its rotation
        std::vector<glm::vec3> triangles; // Mesh: three corners each, in the body's own frame
        std::vector<Collider> parts;      // Compound: its shapes, each placed by its own center

        bool operator==(const Collider&) const = default;
    };

    // How a body's surface behaves in a contact. Of the two bodies touching, the lower friction and the higher
    // bounciness are used.
    struct Material
    {
        float friction = 0.5f;   // 0 slides freely, 1 grips about as hard as it is pressed; at least 0
        float bounciness = 0.0f; // the share of the closing speed it bounces back with, 0 to 1

        bool operator==(const Material&) const = default;
    };

    // What AddBody makes
    struct BodyDesc
    {
        MotionType motion = MotionType::Dynamic;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // w, x, y, z: no rotation
        Collider collider;
        float mass = 1.0f;          // in kilograms; dynamic bodies only
        float gravityScale = 1.0f;  // how much of the world's gravity pulls it: 0 floats, 2 falls twice as fast
        std::uint64_t userData = 0; // the caller's own id for the body (e.g. its entity), handed back in contacts
        Material material;          // its surface: friction and bounciness
        std::uint32_t layers = kDefaultLayer; // what CastRay's layer mask sees it as
        AxisLocks locks;                      // dynamic bodies only
    };

    // A push on a body (PhysicsWorld::TakePushes): where it is, and the force or impulse
    struct Push
    {
        glm::vec3 point{0.0f};
        glm::vec3 amount{0.0f};
    };

    // Where a body is and how it is turned
    struct BodyPose
    {
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    };

    // Two bodies started or stopped touching. a is the one that moves when only one of them does (dynamic before
    // kinematic before static); two of a kind come in no particular order.
    struct ContactEvent
    {
        enum class Kind : std::uint8_t
        {
            Began, // they were apart and now touch
            Ended, // they touched and now are apart, or one of them was removed
        };

        Kind kind = Kind::Began;
        BodyId a;
        BodyId b;
        std::uint64_t userDataA = 0; // the BodyDesc::userData each was made with
        std::uint64_t userDataB = 0;

        // Began only (zero for Ended):
        glm::vec3 point{0.0f};    // where they touch, in world space
        glm::vec3 normal{0.0f};   // unit length, pointing from a to b: moving b along it separates them
        float impactSpeed = 0.0f; // how fast they were closing in along the normal, in units per second
    };

    // The first body a ray hits (PhysicsWorld::CastRay)
    struct RayHit
    {
        BodyId body;
        std::uint64_t userData = 0; // the BodyDesc::userData it was made with
        glm::vec3 point{0.0f};      // where the ray meets its surface, in world space
        glm::vec3 normal{0.0f};     // unit length, out of the surface there (back along the ray if it started inside)
        float distance = 0.0f;      // from the ray's start to the point, in units
    };

    // Farthest apart PhysicsWorld::DistanceBetween measures by default, in units
    inline constexpr float DefaultMaxGap = 1000.0f;

    // How far apart two bodies' colliders are (PhysicsWorld::DistanceBetween)
    struct Gap
    {
        float distance = 0.0f;    // between the nearest points of the two surfaces; 0 when they touch or overlap
        glm::vec3 pointOnA{0.0f}; // the point of the first collider nearest the second, in world space
        glm::vec3 pointOnB{0.0f}; // the point of the second nearest the first (when they overlap, the deepest points)
    };

    // How steep a surface can be and still count as ground, in degrees from flat (PhysicsWorld::GroundUnder)
    inline constexpr float DefaultMaxSlope = 45.0f;

    // How far below a body something can be and still count as what it stands on, in units: a resting body sinks
    // into the ground or hovers above it by a hair (PhysicsWorld::GroundUnder)
    inline constexpr float GroundTolerance = 0.05f;

    // What a body stands on (PhysicsWorld::GroundUnder)
    struct Ground
    {
        BodyId body;
        std::uint64_t userData = 0; // the BodyDesc::userData it was made with
        glm::vec3 point{0.0f};      // where the body meets it, on its surface, in world space
        glm::vec3 normal{0.0f};     // unit length, out of its surface there: up on flat ground
    };

    // A body near a point (PhysicsWorld::BodiesWithin)
    struct NearbyBody
    {
        BodyId body;
        std::uint64_t userData = 0; // the BodyDesc::userData it was made with
        float distance = 0.0f;      // from the point to the nearest point of its collider; 0 if the point is inside
    };
}
