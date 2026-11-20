/* !
@file    	    PhysicsSystem.cpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		26/10/2026
@brief
    Implements the ECS PhysicsSystem: making and removing bodies for entities
    with a RigidBody, keeping static and kinematic bodies on their entities,
    moving the entities of dynamic bodies after each step, and turning the
    world's contact events into collisions between entities, pushing an
    entity's body, casting rays at entities, distances between entities and
    from a point, and the ground an entity stands on.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/scene/systems/PhysicsSystem.hpp"

#include "engine/core/Log.hpp"

#include "engine/physics/PhysicsWorld.hpp"
#include "engine/scene/Hierarchy.hpp"
#include "engine/scene/Scene.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

namespace Engine::Systems
{
    namespace
    {
        // How far an entity has to be from where the physics last put it to count as moved by something else.
        // Not zero: a child entity's Transform is worked out again from its parent each frame, with rounding.
        constexpr float kMovedDistance = 1e-3f;       // units
        constexpr float kTurnedQuaternionDot = 1e-7f; // 1 - |dot| of the rotations: about 0.05 degrees

        // A plane is flat; its collider is a thin slab whose top is the plane itself
        constexpr float kPlaneHalfThickness = 0.05f;

        // The size of the entity on each axis: its uniform size times its per-axis scale
        glm::vec3 SizeOf(const Scene::TransformComponent& transform)
        {
            return glm::abs(transform.scale * glm::vec3(transform.scaleX, transform.scaleY, transform.scaleZ));
        }

        // The Transform's rotations (degrees, applied X, then Y, then Z: Graphics::Transform's order) as a quaternion
        glm::quat RotationOf(const Scene::TransformComponent& transform)
        {
            return glm::angleAxis(glm::radians(transform.rotationZ), glm::vec3(0.0f, 0.0f, 1.0f)) *
                   glm::angleAxis(glm::radians(transform.rotation), glm::vec3(0.0f, 1.0f, 0.0f)) *
                   glm::angleAxis(glm::radians(transform.rotationX), glm::vec3(1.0f, 0.0f, 0.0f));
        }

        Physics::BodyPose PoseOf(const Scene::TransformComponent& transform)
        {
            return {.position = {transform.x, transform.y, transform.z}, .rotation = RotationOf(transform)};
        }

        // The entity's Transform with the body's position and rotation, keeping its size. The size is copied, not
        // read back out of a matrix: that would round it a little differently every frame, until the collider no
        // longer matched and the body was made again.
        Scene::TransformComponent Placed(const Scene::TransformComponent& transform, const Physics::BodyPose& pose)
        {
            const glm::mat4 model = glm::translate(glm::mat4(1.0f), pose.position) * glm::mat4_cast(pose.rotation);
            Scene::TransformComponent placed = Scene::TransformFromMatrix(model);
            placed.scale = transform.scale;
            placed.scaleX = transform.scaleX;
            placed.scaleY = transform.scaleY;
            placed.scaleZ = transform.scaleZ;
            return placed;
        }

        bool Moved(const Scene::TransformComponent& from, const Scene::TransformComponent& to)
        {
            const float distance = glm::length(glm::vec3(to.x - from.x, to.y - from.y, to.z - from.z));
            const float dot = std::abs(glm::dot(RotationOf(from), RotationOf(to)));
            return distance > kMovedDistance || 1.0f - dot > kTurnedQuaternionDot;
        }

        float LargestOf(const glm::vec3& values)
        {
            return std::max({values.x, values.y, values.z});
        }

        // The built-in shape an entity draws; nullopt when it draws nothing, or a mesh file (drawn instead of the shape
        // when one is loaded) whose size is not known here
        std::optional<Game::Shape> DrawnShape(const Scene::MeshRendererComponent* renderer)
        {
            if (renderer == nullptr || renderer->mesh.IsValid() || renderer->model.IsValid())
            {
                return std::nullopt;
            }
            return renderer->shape;
        }

        // The box around a built-in shape, in the object's own units: the built-in meshes are a 1 x 1 x 1 cube, a
        // sphere of radius 1 and a flat 1 x 1 plane (the renderer draws its 10 x 10 plane mesh at a tenth of its
        // size: Renderer::DrawPrimitive)
        struct Bounds
        {
            glm::vec3 half{0.5f};
            glm::vec3 center{0.0f};
        };

        Bounds BoundsOf(std::optional<Game::Shape> drawn)
        {
            if (drawn == Game::Shape::Sphere)
            {
                return {.half = glm::vec3(1.0f)};
            }
            if (drawn == Game::Shape::Plane)
            {
                return {.half = {0.5f, kPlaneHalfThickness, 0.5f}, .center = {0.0f, -kPlaneHalfThickness, 0.0f}};
            }
            return {};
        }

        Physics::Material MaterialOf(const Scene::RigidBodyComponent& body)
        {
            return {.friction = body.friction, .bounciness = body.bounciness};
        }

        // Whether two RigidBodies differ in nothing a body can take on without being made again: its friction and
        // bounciness (changed on the body) and its start velocity (only used when Play starts)
        bool SameBody(Scene::RigidBodyComponent a, const Scene::RigidBodyComponent& b)
        {
            a.friction = b.friction;
            a.bounciness = b.bounciness;
            a.startVelocityX = b.startVelocityX;
            a.startVelocityY = b.startVelocityY;
            a.startVelocityZ = b.startVelocityZ;
            return a == b;
        }

        // How a body moves, carried over to the body made in its place
        struct Motion
        {
            glm::vec3 velocity{0.0f};
            glm::vec3 spin{0.0f};
        };

        bool SameCollider(const Physics::Collider& a, const Physics::Collider& b)
        {
            constexpr float kTolerance = 1e-5f;
            const auto close = [](const glm::vec3& x, const glm::vec3& y)
            { return glm::all(glm::lessThanEqual(glm::abs(x - y), glm::vec3(kTolerance))); };
            return a.shape == b.shape && close(a.halfExtents, b.halfExtents) && close(a.center, b.center) &&
                   std::abs(a.radius - b.radius) <= kTolerance && std::abs(a.halfHeight - b.halfHeight) <= kTolerance;
        }

        Physics::MotionType MotionOf(Scene::BodyType type)
        {
            switch (type)
            {
            case Scene::BodyType::Static: return Physics::MotionType::Static;
            case Scene::BodyType::Kinematic: return Physics::MotionType::Kinematic;
            case Scene::BodyType::Dynamic: break;
            }
            return Physics::MotionType::Dynamic;
        }

        // Moves an entity to where its body is. A child's Transform is where it is in the world; HierarchySystem
        // works it out again from the child's local transform every frame, so that is changed to match.
        void MoveEntity(Scene::Scene& scene, Scene::Entity entity, const Scene::TransformComponent& world)
        {
            if (auto* link = scene.TryGetComponent<Scene::HierarchyComponent>(entity))
            {
                const Scene::TransformComponent parent =
                    scene.GetTransform(scene.FindEntity(link->parent)).value_or(Scene::TransformComponent{});
                link->local = Scene::TransformFromMatrix(
                    glm::inverse(Scene::ModelMatrixOf(parent)) * Scene::ModelMatrixOf(world), link->local.scale);
            }
            scene.SetTransform(entity, world);
        }

        // Keeps one side's touching list: other is added when they begin touching, taken off when they stop
        void UpdateTouching(Scene::Scene& scene, Scene::Entity entity, Scene::Entity::Id other, bool began)
        {
            auto* body = scene.TryGetComponent<Scene::PhysicsBodyComponent>(entity);
            if (body == nullptr)
            {
                return;
            }
            const auto same = [other](Scene::Entity e) { return e.GetId() == other; };
            std::erase_if(body->touching, same);
            if (began)
            {
                body->touching.push_back(scene.FindEntity(other));
            }
        }

        // The world's contact events as collisions between entities, each body's userData being its entity's id
        std::vector<Collision> Collisions(Scene::Scene& scene, const std::vector<Physics::ContactEvent>& events)
        {
            std::vector<Collision> collisions;
            for (const Physics::ContactEvent& event : events)
            {
                const auto idA = static_cast<Scene::Entity::Id>(event.userDataA);
                const auto idB = static_cast<Scene::Entity::Id>(event.userDataB);
                const Scene::Entity a = scene.FindEntity(idA);
                const Scene::Entity b = scene.FindEntity(idB);
                const bool began = event.kind == Physics::ContactEvent::Kind::Began;

                // A side that has been destroyed is never added to the other's list
                UpdateTouching(scene, a, idB, began && b);
                UpdateTouching(scene, b, idA, began && a);
                if (a && b)
                {
                    collisions.push_back({.a = a,
                                          .b = b,
                                          .began = began,
                                          .point = event.point,
                                          .normal = event.normal,
                                          .impactSpeed = event.impactSpeed});
                }
            }
            return collisions;
        }
    }

    Physics::Collider PhysicsSystem::ColliderFor(const Scene::TransformComponent& transform,
                                                 const Scene::MeshRendererComponent* renderer,
                                                 const Scene::RigidBodyComponent& body)
    {
        const glm::vec3 size = SizeOf(transform);
        Physics::Collider collider;
        switch (body.shape)
        {
        case Scene::ColliderShape::Compound:
            if (!body.parts.empty())
            {
                collider.shape = Physics::ShapeType::Compound;
                for (const Scene::ColliderPart& part : body.parts)
                {
                    Scene::RigidBodyComponent partBody;
                    partBody.shape = part.shape;
                    partBody.centerX = part.centerX;
                    partBody.centerY = part.centerY;
                    partBody.centerZ = part.centerZ;
                    partBody.sizeX = part.sizeX;
                    partBody.sizeY = part.sizeY;
                    partBody.sizeZ = part.sizeZ;
                    partBody.radius = part.radius;
                    partBody.height = part.height;
                    collider.parts.push_back(ColliderFor(transform, nullptr, partBody));
                }
                break;
            }
            [[fallthrough]];             // no parts yet: the same as a Mesh without triangles
        case Scene::ColliderShape::Mesh: // its triangles are added by MeshColliderFor; here, and without them, Auto
        case Scene::ColliderShape::Auto:
        {
            const std::optional<Game::Shape> drawn = DrawnShape(renderer);
            // A built-in shape is drawn at its own size and centre inside the object (RenderableDesc)
            const glm::vec3 shapeSize = drawn ? renderer->shapeSize : glm::vec3(1.0f);
            const glm::vec3 shapeCenter = drawn ? renderer->shapeCenter : glm::vec3(0.0f);
            if (drawn == Game::Shape::Sphere)
            {
                collider.shape = Physics::ShapeType::Sphere;
                collider.radius = 1.0f * LargestOf(size * shapeSize); // the built-in sphere has radius 1
                collider.center = shapeCenter * size;
                break;
            }
            // A model or mesh: the box around it; a built-in shape: its own box
            Bounds bounds = renderer && renderer->hasBounds && !drawn
                                ? Bounds{.half = 0.5f * (renderer->boundsMax - renderer->boundsMin),
                                         .center = 0.5f * (renderer->boundsMax + renderer->boundsMin)}
                                : BoundsOf(drawn);
            bounds.half *= shapeSize;
            bounds.center = bounds.center * shapeSize + shapeCenter;
            collider.shape = Physics::ShapeType::Box;
            collider.halfExtents = bounds.half * size;
            collider.center = bounds.center * size;
            if (drawn == Game::Shape::Plane)
            {
                // Thick enough to stand on, whatever the plane's Y scale; its top is where the plane is
                collider.halfExtents.y = kPlaneHalfThickness;
                collider.center.y = -kPlaneHalfThickness;
            }
            return collider;
        }
        case Scene::ColliderShape::Box:
            collider.shape = Physics::ShapeType::Box;
            collider.halfExtents = 0.5f * glm::abs(glm::vec3(body.sizeX, body.sizeY, body.sizeZ)) * size;
            break;
        case Scene::ColliderShape::Sphere:
            collider.shape = Physics::ShapeType::Sphere;
            collider.radius = std::abs(body.radius) * LargestOf(size);
            break;
        case Scene::ColliderShape::Capsule:
        {
            // The radius goes round the Y axis, so it scales with X and Z; the height scales with Y
            collider.shape = Physics::ShapeType::Capsule;
            collider.radius = std::abs(body.radius) * std::max(size.x, size.z);
            const float height = std::abs(body.height) * size.y;
            collider.halfHeight = std::max(0.5f * height - collider.radius, 0.0f);
            break;
        }
        }
        collider.center = glm::vec3(body.centerX, body.centerY, body.centerZ) * size;
        return collider;
    }

    Physics::Collider PhysicsSystem::MeshColliderFor(const Scene::TransformComponent& transform,
                                                     const Scene::MeshRendererComponent* renderer,
                                                     Assets::ResourceManager& resources)
    {
        Physics::Collider collider;
        collider.shape = Physics::ShapeType::Mesh;
        if (renderer == nullptr)
        {
            return collider;
        }
        const glm::vec3 size = SizeOf(transform);
        const glm::mat4 turn =
            glm::rotate(glm::mat4(1.0f), glm::radians(renderer->modelTurn), glm::vec3(0.0f, 1.0f, 0.0f));
        const auto addMesh = [&](const Assets::MeshData& mesh, const glm::mat4& place)
        {
            const auto corner = [&](std::uint32_t index)
            {
                const std::size_t at = static_cast<std::size_t>(index) * 3;
                return glm::vec3(place *
                                 glm::vec4(mesh.positions[at], mesh.positions[at + 1], mesh.positions[at + 2], 1.0f)) *
                       size;
            };
            if (mesh.indices.empty())
            {
                for (std::uint32_t i = 0; i < mesh.positions.size() / 3; ++i)
                {
                    collider.triangles.push_back(corner(i));
                }
                return;
            }
            for (const std::uint32_t index : mesh.indices)
            {
                collider.triangles.push_back(corner(index));
            }
        };
        if (const Assets::Model* model = renderer->model.IsValid() ? resources.Get(renderer->model) : nullptr)
        {
            const auto visit = [&](const auto& self, std::uint32_t node, const glm::mat4& parent) -> void
            {
                if (node >= model->nodes.size())
                {
                    return;
                }
                const glm::mat4 place = parent * model->nodes[node].localTransform;
                for (const std::uint32_t mesh : model->nodes[node].meshIndices)
                {
                    if (mesh < model->meshes.size())
                    {
                        addMesh(model->meshes[mesh], place);
                    }
                }
                for (const std::uint32_t child : model->nodes[node].children)
                {
                    self(self, child, place);
                }
            };
            for (const std::uint32_t root : model->roots)
            {
                visit(visit, root, turn);
            }
            if (model->roots.empty())
            {
                for (const Assets::MeshData& mesh : model->meshes)
                {
                    addMesh(mesh, turn);
                }
            }
        }
        else if (const Assets::Mesh* mesh = renderer->mesh.IsValid() ? resources.Get(renderer->mesh) : nullptr)
        {
            addMesh(*mesh, turn);
        }
        return collider;
    }

    void PhysicsSystem::FitCollider(Scene::RigidBodyComponent& body, std::optional<Game::Shape> drawn,
                                    const std::optional<Assets::BoundingBox>& drawnBox)
    {
        const Bounds bounds =
            drawnBox ? Bounds{.half = drawnBox->Half(), .center = drawnBox->Center()} : BoundsOf(drawn);
        body.centerX = bounds.center.x;
        body.centerY = bounds.center.y;
        body.centerZ = bounds.center.z;
        body.sizeX = 2.0f * bounds.half.x;
        body.sizeY = 2.0f * bounds.half.y;
        body.sizeZ = 2.0f * bounds.half.z;
        // A capsule stands along Y, so its radius fits the X and Z sizes; a sphere has to fit all three
        const float roundRadius = std::max(bounds.half.x, bounds.half.z);
        body.radius = body.shape == Scene::ColliderShape::Capsule ? roundRadius : LargestOf(bounds.half);
        body.height = std::max(2.0f * bounds.half.y, 2.0f * roundRadius);
    }

    void PhysicsSystem::RemoveBody(Scene::Scene& scene, Physics::PhysicsWorld& world, Scene::Entity entity)
    {
        if (const auto* body = scene.TryGetComponent<Scene::PhysicsBodyComponent>(entity))
        {
            world.RemoveBody(body->body);
            scene.RemoveComponent<Scene::PhysicsBodyComponent>(entity);
        }
    }

    void PhysicsSystem::SyncMoved(Scene::Scene& scene, Physics::PhysicsWorld& world)
    {
        if (!world.IsInitialized())
        {
            return;
        }
        scene.ForEach<Scene::PhysicsBodyComponent, const Scene::TransformComponent>(
            [&](Scene::Entity, Scene::PhysicsBodyComponent& body, const Scene::TransformComponent& transform)
            {
                if (Moved(body.lastTransform, transform))
                {
                    world.SetPose(body.body, PoseOf(transform));
                    body.lastTransform = transform;
                }
            });
    }

    std::vector<Collision> PhysicsSystem::Update(Scene::Scene& scene, Physics::PhysicsWorld& world, double deltaSeconds,
                                                 Assets::ResourceManager* resources)
    {
        if (!world.IsInitialized())
        {
            return {};
        }

        // Adding and removing components is not allowed while ForEach walks the entities, so each pass gathers
        // first and changes the entities after

        // 1. Bodies that are no longer wanted (the RigidBody or the Transform is gone), or that have to be made again
        //    (the RigidBody, the size or the shape changed). A new friction or bounciness is changed on the body
        //    itself, and a body made again keeps moving as the old one did, so a moving body keeps going.
        std::vector<Scene::Entity> remove;
        std::vector<std::pair<Scene::Entity, Motion>> carried; // bodies made again: how the old ones moved
        scene.ForEach<Scene::PhysicsBodyComponent>(
            [&](Scene::Entity entity, Scene::PhysicsBodyComponent& body)
            {
                const auto* settings = scene.TryGetComponent<Scene::RigidBodyComponent>(entity);
                const auto transform = scene.GetTransform(entity);
                if (settings != nullptr && *settings != body.settings && SameBody(*settings, body.settings))
                {
                    if (MaterialOf(*settings) != MaterialOf(body.settings))
                    {
                        world.SetMaterial(body.body, MaterialOf(*settings));
                    }
                    body.settings = *settings;
                }
                if (settings == nullptr || !transform || *settings != body.settings || !world.HasBody(body.body) ||
                    !SameCollider(
                        ColliderFor(*transform, scene.TryGetComponent<Scene::MeshRendererComponent>(entity), *settings),
                        body.collider))
                {
                    remove.push_back(entity);
                    if (settings != nullptr && world.HasBody(body.body))
                    {
                        carried.emplace_back(
                            entity, Motion{.velocity = world.Velocity(body.body).value_or(glm::vec3(0.0f)),
                                           .spin = world.AngularVelocity(body.body).value_or(glm::vec3(0.0f))});
                    }
                }
            });
        for (const Scene::Entity entity : remove)
        {
            RemoveBody(scene, world, entity);
        }

        // 2. A body for every entity with a Transform and a RigidBody that has none, where the entity is
        std::vector<std::pair<Scene::Entity, Scene::PhysicsBodyComponent>> made;
        scene.ForEach<const Scene::RigidBodyComponent, const Scene::TransformComponent>(
            [&](Scene::Entity entity, const Scene::RigidBodyComponent& settings,
                const Scene::TransformComponent& transform)
            {
                if (scene.HasComponent<Scene::PhysicsBodyComponent>(entity))
                {
                    return;
                }
                // The body carries its entity's id, so contacts can be traced back to the entity
                const auto* renderer = scene.TryGetComponent<Scene::MeshRendererComponent>(entity);
                Physics::Collider collider = ColliderFor(transform, renderer, settings);
                if (settings.shape == Scene::ColliderShape::Mesh && resources != nullptr)
                {
                    Physics::Collider mesh = MeshColliderFor(transform, renderer, *resources);
                    if (mesh.triangles.size() >= 3)
                    {
                        collider = std::move(mesh);
                    }
                    else
                    {
                        LOG_WARN(Physics,
                                 "A Mesh collider needs a model or mesh to take its triangles from: using Auto");
                    }
                }
                const Physics::BodyPose pose = PoseOf(transform);
                const Physics::BodyId body = world.AddBody({.motion = MotionOf(settings.type),
                                                            .position = pose.position,
                                                            .rotation = pose.rotation,
                                                            .collider = collider,
                                                            .mass = settings.mass,
                                                            .gravityScale = settings.gravityScale,
                                                            .userData = entity.GetId(),
                                                            .material = MaterialOf(settings),
                                                            .layers = static_cast<std::uint32_t>(settings.layers),
                                                            .locks = {.moveX = settings.lockMoveX,
                                                                      .moveY = settings.lockMoveY,
                                                                      .moveZ = settings.lockMoveZ,
                                                                      .turnX = settings.lockTurnX,
                                                                      .turnY = settings.lockTurnY,
                                                                      .turnZ = settings.lockTurnZ}});
                if (body.IsValid())
                {
                    made.emplace_back(entity, Scene::PhysicsBodyComponent{.body = body,
                                                                          .settings = settings,
                                                                          .collider = collider,
                                                                          .lastTransform = transform});
                }
            });
        for (const auto& [entity, body] : made)
        {
            scene.AddComponent(entity, body);

            // A new body starts with its start velocity; one made again in place of another moves as that one did
            const auto old = std::ranges::find(carried, entity, &std::pair<Scene::Entity, Motion>::first);
            if (old != carried.end())
            {
                world.SetVelocity(body.body, old->second.velocity);
                world.SetAngularVelocity(body.body, old->second.spin);
            }
            else
            {
                world.SetVelocity(body.body, glm::vec3(body.settings.startVelocityX, body.settings.startVelocityY,
                                                       body.settings.startVelocityZ));
            }
        }

        // 3. Something other than the physics moved an entity
        SyncMoved(scene, world);

        // 4. Simulate
        world.Update(deltaSeconds);

        // 5. Dynamic bodies move their entities
        std::vector<std::pair<Scene::Entity, Scene::TransformComponent>> moved;
        scene.ForEach<Scene::PhysicsBodyComponent, const Scene::TransformComponent>(
            [&](Scene::Entity entity, Scene::PhysicsBodyComponent& body, const Scene::TransformComponent& transform)
            {
                if (body.settings.type != Scene::BodyType::Dynamic)
                {
                    return;
                }
                if (const auto pose = world.Pose(body.body))
                {
                    body.lastTransform = Placed(transform, *pose);
                    moved.emplace_back(entity, body.lastTransform);
                }
            });
        for (const auto& [entity, transform] : moved)
        {
            MoveEntity(scene, entity, transform);
        }

        // 6. Contacts, which also include the ones ended by bodies removed in pass 1
        return Collisions(scene, world.TakeContactEvents());
    }

    namespace
    {
        // The body made for an entity, if it has one
        std::optional<Physics::BodyId> BodyOf(const Scene::Scene& scene, Scene::Entity entity)
        {
            const auto* body = scene.TryGetComponent<Scene::PhysicsBodyComponent>(entity);
            return body != nullptr ? std::optional(body->body) : std::nullopt;
        }
    }

    bool PhysicsSystem::AddImpulse(const Scene::Scene& scene, Physics::PhysicsWorld& world, Scene::Entity entity,
                                   const glm::vec3& impulse)
    {
        const auto body = BodyOf(scene, entity);
        if (body)
        {
            world.AddImpulse(*body, impulse);
        }
        return body.has_value();
    }

    bool PhysicsSystem::SetConstantForce(const Scene::Scene& scene, Physics::PhysicsWorld& world, Scene::Entity entity,
                                         const glm::vec3& force)
    {
        const auto body = BodyOf(scene, entity);
        if (body)
        {
            world.SetConstantForce(*body, force);
        }
        return body.has_value();
    }

    bool PhysicsSystem::SetVelocity(const Scene::Scene& scene, Physics::PhysicsWorld& world, Scene::Entity entity,
                                    const glm::vec3& velocity)
    {
        const auto body = BodyOf(scene, entity);
        if (body)
        {
            world.SetVelocity(*body, velocity);
        }
        return body.has_value();
    }

    std::optional<glm::vec3> PhysicsSystem::VelocityOf(const Scene::Scene& scene, const Physics::PhysicsWorld& world,
                                                       Scene::Entity entity)
    {
        const auto body = BodyOf(scene, entity);
        return body ? world.Velocity(*body) : std::nullopt;
    }

    std::optional<RaycastHit> PhysicsSystem::Raycast(const Scene::Scene& scene, const Physics::PhysicsWorld& world,
                                                     const glm::vec3& origin, const glm::vec3& direction,
                                                     float maxDistance, Scene::Entity ignore, std::uint32_t layerMask)
    {
        const std::optional<Physics::RayHit> hit =
            world.CastRay(origin, direction, maxDistance, BodyOf(scene, ignore).value_or(Physics::BodyId{}), layerMask);
        if (!hit)
        {
            return std::nullopt;
        }
        // Each body's userData is its entity's id
        const Scene::Entity entity = scene.FindEntity(static_cast<Scene::Entity::Id>(hit->userData));
        if (!entity)
        {
            return std::nullopt;
        }
        return RaycastHit{.entity = entity, .point = hit->point, .normal = hit->normal, .distance = hit->distance};
    }

    std::optional<Physics::Gap> PhysicsSystem::DistanceBetween(const Scene::Scene& scene,
                                                               const Physics::PhysicsWorld& world, Scene::Entity a,
                                                               Scene::Entity b, float maxDistance)
    {
        const auto bodyA = BodyOf(scene, a);
        const auto bodyB = BodyOf(scene, b);
        return bodyA && bodyB ? world.DistanceBetween(*bodyA, *bodyB, maxDistance) : std::nullopt;
    }

    std::vector<NearbyEntity> PhysicsSystem::EntitiesWithin(const Scene::Scene& scene,
                                                            const Physics::PhysicsWorld& world, const glm::vec3& point,
                                                            float radius, Scene::Entity ignore)
    {
        std::vector<NearbyEntity> nearby;
        for (const Physics::NearbyBody& body :
             world.BodiesWithin(point, radius, BodyOf(scene, ignore).value_or(Physics::BodyId{})))
        {
            // Each body's userData is its entity's id; a body left behind by a destroyed entity is skipped
            const Scene::Entity entity = scene.FindEntity(static_cast<Scene::Entity::Id>(body.userData));
            if (entity)
            {
                nearby.push_back({.entity = entity, .distance = body.distance});
            }
        }
        return nearby;
    }

    std::optional<GroundHit> PhysicsSystem::GroundUnder(const Scene::Scene& scene, const Physics::PhysicsWorld& world,
                                                        Scene::Entity entity, float maxSlope)
    {
        const auto body = BodyOf(scene, entity);
        const std::optional<Physics::Ground> ground = body ? world.GroundUnder(*body, maxSlope) : std::nullopt;
        if (!ground)
        {
            return std::nullopt;
        }
        // Each body's userData is its entity's id
        const Scene::Entity under = scene.FindEntity(static_cast<Scene::Entity::Id>(ground->userData));
        if (!under)
        {
            return std::nullopt;
        }
        return GroundHit{.entity = under, .point = ground->point, .normal = ground->normal};
    }

    bool PhysicsSystem::IsOnGround(const Scene::Scene& scene, const Physics::PhysicsWorld& world, Scene::Entity entity,
                                   float maxSlope)
    {
        return GroundUnder(scene, world, entity, maxSlope).has_value();
    }

    std::vector<ColliderOutline> PhysicsSystem::Outlines(Scene::Scene& scene, const Physics::PhysicsWorld* world,
                                                         int segments)
    {
        std::vector<ColliderOutline> outlines;
        scene.ForEach<const Scene::RigidBodyComponent, const Scene::TransformComponent>(
            [&](Scene::Entity entity, const Scene::RigidBodyComponent& settings,
                const Scene::TransformComponent& transform)
            {
                ColliderOutline outline{
                    .entity = entity, .type = settings.type, .layers = static_cast<std::uint32_t>(settings.layers)};
                Physics::Collider collider =
                    ColliderFor(transform, scene.TryGetComponent<Scene::MeshRendererComponent>(entity), settings);
                Physics::BodyPose pose = PoseOf(transform);

                const auto* body = scene.TryGetComponent<Scene::PhysicsBodyComponent>(entity);
                const std::optional<Physics::BodyPose> live =
                    body != nullptr && world != nullptr ? world->Pose(body->body) : std::nullopt;
                if (live)
                {
                    collider = body->collider;
                    pose = *live;
                    outline.asleep = settings.type != Scene::BodyType::Static && !world->IsAwake(body->body);
                    outline.touching = !body->touching.empty();
                }
                outline.lines = Physics::OutlineOf(collider, pose, segments);
                outlines.push_back(std::move(outline));
            });
        return outlines;
    }

    std::vector<Scene::Entity> PhysicsSystem::Touching(const Scene::Scene& scene, Scene::Entity entity)
    {
        std::vector<Scene::Entity> touching;
        if (const auto* body = scene.TryGetComponent<Scene::PhysicsBodyComponent>(entity))
        {
            std::ranges::copy_if(body->touching, std::back_inserter(touching),
                                 [&scene](Scene::Entity other) { return scene.IsAlive(other); });
        }
        return touching;
    }

    bool PhysicsSystem::AreTouching(const Scene::Scene& scene, Scene::Entity a, Scene::Entity b)
    {
        const auto* body = scene.TryGetComponent<Scene::PhysicsBodyComponent>(a);
        return body != nullptr && scene.IsAlive(b) && std::ranges::find(body->touching, b) != body->touching.end();
    }
}
