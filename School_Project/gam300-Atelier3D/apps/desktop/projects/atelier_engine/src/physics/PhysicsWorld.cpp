/* !
@file    	    PhysicsWorld.cpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		26/10/2026
@brief
    Implements PhysicsWorld on top of Jolt Physics: starting and stopping Jolt
    once per process, the object and broad-phase layers, the worker threads,
    adding and removing bodies, their friction and bounciness, pushing them
    (impulses, steady forces, velocities), the fixed-rate update, turning
    Jolt's contact callbacks into began and ended events between bodies, ray
    casts, distances between bodies and to a point, and finding the ground a
    body stands on.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/physics/PhysicsWorld.hpp"

#include "engine/core/Log.hpp"

// Jolt.h has to come before any other Jolt header: it sets up the platform and the macros they all use
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <format>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Engine::Physics
{
    namespace
    {
        // Object layers: every body is in one. Static bodies (floors, walls) never collide with each other, which
        // saves Jolt from testing them against each other every step.
        namespace Layers
        {
            constexpr JPH::ObjectLayer NonMoving = 0;
            constexpr JPH::ObjectLayer Moving = 1;
            constexpr JPH::uint Count = 2;
        }

        // Broad-phase layers: Jolt keeps one bounding-volume tree per layer. Static bodies get their own tree, which
        // is only rebuilt when one is added or removed.
        namespace BroadPhaseLayers
        {
            constexpr JPH::BroadPhaseLayer NonMoving{0};
            constexpr JPH::BroadPhaseLayer Moving{1};
            constexpr JPH::uint Count = 2;
        }

        // Scratch memory Jolt uses during a step, allocated once per world
        constexpr std::uint32_t kTempAllocatorBytes = 10 * 1024 * 1024;

        // Upper limits Jolt sizes its buffers for. A step that runs out of room is logged (see Update)
        constexpr std::uint32_t kMaxBodyPairs = 10240;
        constexpr std::uint32_t kMaxContactConstraints = 10240;

        // Jolt reports through these two hooks; route them to the engine log so they show up in the Console
        void TraceToLog(const char* format, ...)
        {
            std::array<char, 1024> message{};
            va_list args;
            va_start(args, format);
            std::vsnprintf(message.data(), message.size(), format, args);
            va_end(args);
            LOG_INFO(Physics, "{}", message.data());
        }

#ifdef JPH_ENABLE_ASSERTS
        // Only compiled when Jolt's asserts are on (its CMake option USE_ASSERTS). Returning true breaks into the
        // debugger.
        bool AssertToLog(const char* expression, const char* message, const char* file, JPH::uint line)
        {
            LOG_ERROR(Physics, "Jolt assert failed: {} {} ({}:{})", expression, message != nullptr ? message : "", file,
                      line);
            return true;
        }
#endif

        // Jolt's allocator, type registry and factory are global, so they are set up by the first world to start and
        // torn down by the last one to stop. Only the main thread starts and stops worlds; the mutex is there so
        // that never has to be true.
        std::mutex g_joltMutex;
        int g_joltUsers = 0;

        bool AcquireJolt()
        {
            const std::lock_guard lock(g_joltMutex);
            if (g_joltUsers == 0)
            {
                JPH::RegisterDefaultAllocator();
                JPH::Trace = TraceToLog;
#ifdef JPH_ENABLE_ASSERTS
                JPH::AssertFailed = AssertToLog;
#endif

                // Jolt's headers and its library were built with different settings: the engine would crash later on
                // in ways that are hard to trace, so refuse to start instead
                if (!JPH::VerifyJoltVersionID())
                {
                    LOG_ERROR(Physics, "Jolt was built with different settings than the engine; physics is off");
                    return false;
                }

                JPH::Factory::sInstance = new JPH::Factory();
                JPH::RegisterTypes();
            }
            ++g_joltUsers;
            return true;
        }

        void ReleaseJolt()
        {
            const std::lock_guard lock(g_joltMutex);
            if (g_joltUsers == 0)
            {
                return; // more releases than acquires: nothing to undo
            }
            --g_joltUsers;
            if (g_joltUsers == 0)
            {
                JPH::UnregisterTypes();
                delete JPH::Factory::sInstance;
                JPH::Factory::sInstance = nullptr;
            }
        }

        // glm and Jolt store a quaternion's parts in a different order
        JPH::Quat ToJolt(const glm::quat& rotation)
        {
            return JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w).Normalized();
        }

        glm::quat ToGlm(JPH::QuatArg rotation)
        {
            return glm::quat(rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ());
        }

        JPH::Vec3 ToJolt(const glm::vec3& v)
        {
            return {v.x, v.y, v.z};
        }

        glm::vec3 ToGlm(JPH::Vec3Arg v)
        {
            return {v.GetX(), v.GetY(), v.GetZ()};
        }

        JPH::BodyID ToJolt(BodyId body)
        {
            return JPH::BodyID(body.value);
        }

        // The collision shape, moved off the body's origin when its centre is not zero. Sizes too small for Jolt are
        // raised to a minimum rather than refused. nullptr (logged) if Jolt refuses the shape anyway.
        JPH::ShapeRefC MakeShape(const Collider& collider, bool dynamic)
        {
            constexpr float kMinSize = 0.001f;
            JPH::ShapeSettings::ShapeResult result;
            switch (collider.shape)
            {
            case ShapeType::Sphere:
                result = JPH::SphereShapeSettings(std::max(collider.radius, kMinSize)).Create();
                break;
            case ShapeType::Capsule:
            {
                const float radius = std::max(collider.radius, kMinSize);
                // With no straight middle a capsule is a sphere, which Jolt wants made as one
                result = collider.halfHeight < kMinSize
                             ? JPH::SphereShapeSettings(radius).Create()
                             : JPH::CapsuleShapeSettings(collider.halfHeight, radius).Create();
                break;
            }
            case ShapeType::Box:
            {
                // Jolt rounds a box's corners by its convex radius, which must fit inside the box. Very thin boxes
                // (a floor) get a smaller radius instead of a refused shape.
                const glm::vec3 half = glm::max(collider.halfExtents, glm::vec3(kMinSize));
                const float convexRadius =
                    std::min(JPH::cDefaultConvexRadius, 0.5f * std::min({half.x, half.y, half.z}));
                result = JPH::BoxShapeSettings(JPH::Vec3(half.x, half.y, half.z), convexRadius).Create();
                break;
            }
            case ShapeType::Mesh:
            {
                if (dynamic)
                {
                    // Jolt's triangle meshes cannot move by themselves: a dynamic body has no inside to weigh
                    JPH::Array<JPH::Vec3> points;
                    for (const glm::vec3& corner : collider.triangles)
                    {
                        points.push_back(JPH::Vec3(corner.x, corner.y, corner.z));
                    }
                    result = JPH::ConvexHullShapeSettings(points).Create();
                    break;
                }
                JPH::TriangleList triangles;
                for (std::size_t i = 0; i + 2 < collider.triangles.size(); i += 3)
                {
                    const auto corner = [&collider, i](std::size_t which)
                    {
                        const glm::vec3& at = collider.triangles[i + which];
                        return JPH::Float3(at.x, at.y, at.z);
                    };
                    triangles.push_back(JPH::Triangle(corner(0), corner(1), corner(2)));
                }
                result = JPH::MeshShapeSettings(triangles).Create();
                break;
            }
            case ShapeType::Compound:
            {
                JPH::StaticCompoundShapeSettings compound;
                for (const Collider& part : collider.parts)
                {
                    const JPH::ShapeRefC shape = MakeShape(part, dynamic);
                    if (shape != nullptr)
                    {
                        compound.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(), shape);
                    }
                }
                result = compound.Create();
                break;
            }
            }
            if (!result.HasError() && collider.center != glm::vec3(0.0f))
            {
                // Wraps the shape made above (the result holds a reference to it)
                const JPH::Vec3 offset(collider.center.x, collider.center.y, collider.center.z);
                result = JPH::RotatedTranslatedShapeSettings(offset, JPH::Quat::sIdentity(), result.Get()).Create();
            }
            if (result.HasError())
            {
                LOG_ERROR(Physics, "Could not make a collider: {}", result.GetError());
                return nullptr;
            }
            return result.Get();
        }

        // A contact Jolt found between two bodies during a step. Jolt reports one per pair of touching sub shapes;
        // the shapes the engine makes have one each, so in practice one per pair of bodies.
        struct AddedContact
        {
            JPH::BodyID a; // Jolt orders the two by id
            JPH::BodyID b;
            std::uint64_t userDataA = 0;
            std::uint64_t userDataB = 0;
            glm::vec3 point{0.0f};
            glm::vec3 normal{0.0f};
            float impactSpeed = 0.0f;
        };

        // A contact from the step before that Jolt no longer finds. Jolt also sends one when a body goes to sleep.
        struct RemovedContact
        {
            JPH::BodyID a;
            JPH::BodyID b;
        };

        // Jolt calls these from its worker threads in the middle of a step, with every body locked: they only copy
        // what is needed into lists that PhysicsWorld goes through after the step, on the main thread.
        class ContactCollector final : public JPH::ContactListener
        {
        public:
            void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold,
                                JPH::ContactSettings& /*settings*/) override
            {
                AddedContact contact{.a = body1.GetID(),
                                     .b = body2.GetID(),
                                     .userDataA = body1.GetUserData(),
                                     .userDataB = body2.GetUserData()};

                // The middle of the contact area: the average of the points on both surfaces
                JPH::RVec3 sum = JPH::RVec3::sZero();
                const JPH::uint count = manifold.mRelativeContactPointsOn1.size();
                for (JPH::uint i = 0; i < count; ++i)
                {
                    sum += manifold.GetWorldSpaceContactPointOn1(i) + manifold.GetWorldSpaceContactPointOn2(i);
                }
                const JPH::RVec3 point = count > 0 ? sum / static_cast<float>(2 * count) : manifold.mBaseOffset;
                const JPH::Vec3 normal = manifold.mWorldSpaceNormal;

                // The normal points from body 1 to body 2, so body 1 moving along it closes the gap. Velocities are
                // the ones before the contact is resolved.
                const float closing = (body1.GetPointVelocity(point) - body2.GetPointVelocity(point)).Dot(normal);

                contact.point = {static_cast<float>(point.GetX()), static_cast<float>(point.GetY()),
                                 static_cast<float>(point.GetZ())};
                contact.normal = {normal.GetX(), normal.GetY(), normal.GetZ()};
                contact.impactSpeed = std::max(closing, 0.0f);

                const std::lock_guard lock(mutex_);
                added_.push_back(contact);
            }

            void OnContactRemoved(const JPH::SubShapeIDPair& pair) override
            {
                // The bodies cannot be looked at here (one may already be gone): only their ids are kept
                const std::lock_guard lock(mutex_);
                removed_.push_back({.a = pair.GetBody1ID(), .b = pair.GetBody2ID()});
            }

            // Hands over everything collected since the last call
            void Take(std::vector<AddedContact>& added, std::vector<RemovedContact>& removed)
            {
                const std::lock_guard lock(mutex_);
                added.swap(added_);
                removed.swap(removed_);
                added_.clear();
                removed_.clear();
            }

        private:
            std::mutex mutex_;
            std::vector<AddedContact> added_;
            std::vector<RemovedContact> removed_;
        };

        // Jolt takes any friction, but two touching bodies must never combine to below 0; bounciness above 1 would
        // add energy at every bounce
        Material Clamped(const Material& material)
        {
            return {.friction = std::max(material.friction, 0.0f),
                    .bounciness = std::clamp(material.bounciness, 0.0f, 1.0f)};
        }

        // The same key for a pair of bodies whichever order they come in
        std::uint64_t PairKey(std::uint32_t a, std::uint32_t b)
        {
            return (static_cast<std::uint64_t>(std::min(a, b)) << 32U) | std::max(a, b);
        }

        // One thread is the game's own; the rest of the CPU's cores (up to 8) run physics jobs
        int WorkerThreadCount()
        {
            const int cores = static_cast<int>(std::thread::hardware_concurrency());
            return std::clamp(cores - 1, 1, 8);
        }
    }

    // Members are destroyed in reverse order: the physics system goes first, while the layer tables and allocators
    // it points to still exist
    // A game that never asks for its impulses does not pile them up
    static constexpr std::size_t kMaxKeptImpulses = 64;

    struct PhysicsWorld::Impl
    {
        Impl() : jobSystem(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, WorkerThreadCount())
        {
            broadPhaseLayers.MapObjectToBroadPhaseLayer(Layers::NonMoving, BroadPhaseLayers::NonMoving);
            broadPhaseLayers.MapObjectToBroadPhaseLayer(Layers::Moving, BroadPhaseLayers::Moving);

            objectLayerPairs.EnableCollision(Layers::Moving, Layers::Moving);
            objectLayerPairs.EnableCollision(Layers::Moving, Layers::NonMoving);

            // Made only now: it reads the two tables above when it is constructed, so they have to be filled first
            objectVsBroadPhase.emplace(broadPhaseLayers, BroadPhaseLayers::Count, objectLayerPairs, Layers::Count);

            system.Init(MaxBodies, 0, kMaxBodyPairs, kMaxContactConstraints, broadPhaseLayers, *objectVsBroadPhase,
                        objectLayerPairs);
            system.SetContactListener(&contacts);

            // Of two touching bodies the slipperier and the bouncier one decide (Jolt's own default multiplies the
            // frictions, so an icy floor would still grip a rubber block)
            system.SetCombineFriction(
                [](const JPH::Body& body1, const JPH::SubShapeID&, const JPH::Body& body2, const JPH::SubShapeID&)
                { return std::min(body1.GetFriction(), body2.GetFriction()); });
            system.SetCombineRestitution(
                [](const JPH::Body& body1, const JPH::SubShapeID&, const JPH::Body& body2, const JPH::SubShapeID&)
                { return std::max(body1.GetRestitution(), body2.GetRestitution()); });
        }

        // Two bodies that touch
        struct TouchingPair
        {
            BodyId a;
            BodyId b;
            std::uint64_t userDataA = 0;
            std::uint64_t userDataB = 0;
            int contacts = 0; // Jolt's contacts between them; 0 while one of them sleeps on the other
        };

        // A body that is not moving and so not simulated. Jolt drops a sleeping body's contacts, but it still lies
        // where it did, on whatever it touched.
        bool Asleep(JPH::BodyID id) const
        {
            const JPH::BodyInterface& bodies = system.GetBodyInterface();
            return bodies.IsAdded(id) && bodies.GetMotionType(id) != JPH::EMotionType::Static && !bodies.IsActive(id);
        }

        void EndContact(const TouchingPair& pair)
        {
            events.push_back({.kind = ContactEvent::Kind::Ended,
                              .a = pair.a,
                              .b = pair.b,
                              .userDataA = pair.userDataA,
                              .userDataB = pair.userDataB});
        }

        // After a step: turns Jolt's contacts into pairs of bodies that started or stopped touching
        void UpdateContacts()
        {
            std::vector<AddedContact> added;
            std::vector<RemovedContact> removed;
            contacts.Take(added, removed);

            for (AddedContact contact : added)
            {
                // The one that moves first (dynamic, then kinematic, then static), so "the cube hit the floor"
                const JPH::BodyInterface& bodies = system.GetBodyInterface();
                if (bodies.GetMotionType(contact.b) > bodies.GetMotionType(contact.a))
                {
                    std::swap(contact.a, contact.b);
                    std::swap(contact.userDataA, contact.userDataB);
                    contact.normal = -contact.normal;
                }
                const BodyId a{contact.a.GetIndexAndSequenceNumber()};
                const BodyId b{contact.b.GetIndexAndSequenceNumber()};
                const auto [it, isNew] = touching.try_emplace(
                    PairKey(a.value, b.value),
                    TouchingPair{.a = a, .b = b, .userDataA = contact.userDataA, .userDataB = contact.userDataB});
                ++it->second.contacts;
                if (isNew)
                {
                    events.push_back({.kind = ContactEvent::Kind::Began,
                                      .a = a,
                                      .b = b,
                                      .userDataA = contact.userDataA,
                                      .userDataB = contact.userDataB,
                                      .point = contact.point,
                                      .normal = contact.normal,
                                      .impactSpeed = contact.impactSpeed});
                }
            }

            for (const RemovedContact& contact : removed)
            {
                const auto it = touching.find(
                    PairKey(contact.a.GetIndexAndSequenceNumber(), contact.b.GetIndexAndSequenceNumber()));
                if (it != touching.end() && it->second.contacts > 0)
                {
                    --it->second.contacts;
                }
            }

            // Pairs Jolt no longer has a contact for have come apart, unless one of them fell asleep on the other.
            // A sleeper that wakes up gets its contacts back from Jolt in the step it wakes in; if it does not, it
            // has moved away.
            for (auto it = touching.begin(); it != touching.end();)
            {
                const TouchingPair& pair = it->second;
                if (pair.contacts == 0 && !Asleep(JPH::BodyID(pair.a.value)) && !Asleep(JPH::BodyID(pair.b.value)))
                {
                    EndContact(pair);
                    it = touching.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        JPH::TempAllocatorImpl tempAllocator{kTempAllocatorBytes};
        JPH::JobSystemThreadPool jobSystem;
        JPH::BroadPhaseLayerInterfaceTable broadPhaseLayers{Layers::Count, BroadPhaseLayers::Count};
        JPH::ObjectLayerPairFilterTable objectLayerPairs{Layers::Count};
        std::optional<JPH::ObjectVsBroadPhaseLayerFilterTable> objectVsBroadPhase;
        ContactCollector contacts; // before the system, which points to it
        JPH::PhysicsSystem system;

        std::unordered_map<std::uint64_t, TouchingPair> touching; // by PairKey
        std::vector<ContactEvent> events;                         // not taken yet
        std::unordered_map<std::uint32_t, glm::vec3> forces;      // steady pushes, by BodyId::value
        std::unordered_map<std::uint32_t, std::uint32_t> layers;  // each body's layers, by BodyId::value
        std::vector<Push> impulses;                               // given since the last TakePushes

        // Jolt forgets forces after every step, so the steady ones are given again before each
        void ApplyConstantForces()
        {
            JPH::BodyInterface& bodies = system.GetBodyInterface();
            for (const auto& [id, force] : forces)
            {
                bodies.AddForce(JPH::BodyID(id), JPH::Vec3(force.x, force.y, force.z));
            }
        }

        // A body that can be pushed: one that exists and is dynamic
        bool IsDynamic(BodyId body) const
        {
            const JPH::BodyInterface& bodies = system.GetBodyInterface();
            return body.IsValid() && bodies.IsAdded(JPH::BodyID(body.value)) &&
                   bodies.GetMotionType(JPH::BodyID(body.value)) == JPH::EMotionType::Dynamic;
        }
    };

    PhysicsWorld::PhysicsWorld() = default;

    PhysicsWorld::~PhysicsWorld()
    {
        Shutdown();
    }

    bool PhysicsWorld::Init()
    {
        if (impl_)
        {
            return true;
        }
        if (!AcquireJolt())
        {
            return false;
        }

        impl_ = std::make_unique<Impl>();
        impl_->system.SetGravity(JPH::Vec3(gravity_.x, gravity_.y, gravity_.z));
        accumulator_ = 0.0;
        steps_ = 0;
        warnedUpdateError_ = false;
        warnedFull_ = false;

        const int workers = WorkerThreadCount();
        LOG_INFO(Physics, "{} ready ({} worker thread{}, {:.0f} steps per second)", LibraryVersion(), workers,
                 workers == 1 ? "" : "s", 1.0 / FixedStep);
        return true;
    }

    void PhysicsWorld::Shutdown()
    {
        if (!impl_)
        {
            return;
        }
        impl_.reset();
        ReleaseJolt();
        accumulator_ = 0.0;
    }

    bool PhysicsWorld::IsInitialized() const
    {
        return impl_ != nullptr;
    }

    // A fixed step keeps the simulation the same at any frame rate: the frame's time is added to the accumulator and
    // spent in whole steps; what is left (less than one step) waits for the next frame
    int PhysicsWorld::Update(double deltaSeconds)
    {
        if (!impl_)
        {
            return 0;
        }

        // Absorbs rounding, so that e.g. three frames of 1/180 s still make one step
        constexpr double kEpsilon = 1e-9;

        accumulator_ += std::max(deltaSeconds, 0.0);
        int steps = 0;
        while (accumulator_ + kEpsilon >= FixedStep && steps < MaxStepsPerUpdate)
        {
            impl_->ApplyConstantForces();

            // One collision step per update: enough at 60 steps per second
            const JPH::EPhysicsUpdateError errors =
                impl_->system.Update(static_cast<float>(FixedStep), 1, &impl_->tempAllocator, &impl_->jobSystem);
            if (errors != JPH::EPhysicsUpdateError::None && !warnedUpdateError_)
            {
                LOG_WARN(Physics, "Too many contacts this step (Jolt error flags {}); some collisions were skipped",
                         static_cast<unsigned>(errors));
                warnedUpdateError_ = true;
            }
            impl_->UpdateContacts();
            accumulator_ = std::max(accumulator_ - FixedStep, 0.0);
            ++steps;
        }

        // Hit the cap: drop the backlog instead of carrying it into the next frame
        if (accumulator_ + kEpsilon >= FixedStep)
        {
            accumulator_ = 0.0;
        }

        steps_ += static_cast<std::uint64_t>(steps);
        return steps;
    }

    void PhysicsWorld::SetGravity(const glm::vec3& gravity)
    {
        gravity_ = gravity;
        if (impl_)
        {
            impl_->system.SetGravity(JPH::Vec3(gravity.x, gravity.y, gravity.z));

            // A body that has been still for a moment goes to sleep and ignores gravity until something wakes it.
            // Wake them all, so e.g. a cube floating with no gravity starts to fall when gravity is turned back on.
            // Static bodies are skipped by Jolt.
            JPH::BodyIDVector bodies;
            impl_->system.GetBodies(bodies);
            impl_->system.GetBodyInterface().ActivateBodies(bodies.data(), static_cast<int>(bodies.size()));
        }
    }

    glm::vec3 PhysicsWorld::Gravity() const
    {
        return gravity_;
    }

    std::uint32_t PhysicsWorld::BodyCount() const
    {
        return impl_ ? impl_->system.GetNumBodies() : 0;
    }

    std::uint64_t PhysicsWorld::StepCount() const
    {
        return impl_ ? steps_ : 0;
    }

    BodyId PhysicsWorld::AddBody(const BodyDesc& desc)
    {
        if (!impl_)
        {
            return {};
        }
        const JPH::ShapeRefC shape = MakeShape(desc.collider, desc.motion == MotionType::Dynamic);
        if (shape == nullptr)
        {
            return {};
        }

        const JPH::EMotionType motion = desc.motion == MotionType::Static      ? JPH::EMotionType::Static
                                        : desc.motion == MotionType::Kinematic ? JPH::EMotionType::Kinematic
                                                                               : JPH::EMotionType::Dynamic;
        const JPH::ObjectLayer layer = desc.motion == MotionType::Static ? Layers::NonMoving : Layers::Moving;
        JPH::BodyCreationSettings settings(shape, JPH::RVec3(desc.position.x, desc.position.y, desc.position.z),
                                           ToJolt(desc.rotation), motion, layer);
        if (desc.motion == MotionType::Dynamic)
        {
            // The mass is set; how it is spread (the inertia) is worked out from the collider
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = std::max(desc.mass, 0.001f);
            settings.mGravityFactor = desc.gravityScale;
            // No air drag (Jolt slows moving bodies by 5% a second by default), so a fall follows the gravity
            // formula and a thrown object keeps its speed. Spinning still slows down.
            settings.mLinearDamping = 0.0f;
            // Swept between steps: a fast body stops at a wall or a thin floor, instead of landing on its far side
            settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
            const AxisLocks& locks = desc.locks;
            const auto allowed = static_cast<std::uint8_t>(
                (locks.moveX ? 0 : static_cast<std::uint8_t>(JPH::EAllowedDOFs::TranslationX)) |
                (locks.moveY ? 0 : static_cast<std::uint8_t>(JPH::EAllowedDOFs::TranslationY)) |
                (locks.moveZ ? 0 : static_cast<std::uint8_t>(JPH::EAllowedDOFs::TranslationZ)) |
                (locks.turnX ? 0 : static_cast<std::uint8_t>(JPH::EAllowedDOFs::RotationX)) |
                (locks.turnY ? 0 : static_cast<std::uint8_t>(JPH::EAllowedDOFs::RotationY)) |
                (locks.turnZ ? 0 : static_cast<std::uint8_t>(JPH::EAllowedDOFs::RotationZ)));
            if (allowed != 0)
            {
                settings.mAllowedDOFs = static_cast<JPH::EAllowedDOFs>(allowed);
            }
            else
            {
                LOG_WARN(Physics, "A dynamic body cannot be locked on every axis: make it static. Left unlocked.");
            }
        }
        else if (desc.motion == MotionType::Kinematic)
        {
            // Jolt leaves out contacts between a kinematic body and static or other kinematic ones (neither can push
            // the other). Kept, so a moving platform still reports touching a wall.
            settings.mCollideKinematicVsNonDynamic = true;
        }
        settings.mUserData = desc.userData;
        const Material material = Clamped(desc.material);
        settings.mFriction = material.friction;
        settings.mRestitution = material.bounciness;

        // A static body never needs waking: nothing moves it
        const JPH::EActivation activation =
            desc.motion == MotionType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate;
        const JPH::BodyID id = impl_->system.GetBodyInterface().CreateAndAddBody(settings, activation);
        if (id.IsInvalid())
        {
            if (!warnedFull_)
            {
                LOG_WARN(Physics, "Could not add a body: the world already has {} bodies", BodyCount());
                warnedFull_ = true;
            }
            return {};
        }
        impl_->layers[id.GetIndexAndSequenceNumber()] = desc.layers;
        return BodyId{id.GetIndexAndSequenceNumber()};
    }

    void PhysicsWorld::RemoveBody(BodyId body)
    {
        if (!HasBody(body))
        {
            return;
        }

        impl_->forces.erase(body.value);
        impl_->layers.erase(body.value);

        // Its contacts end now. Jolt reports them gone in the next step too; by then the pairs are forgotten.
        for (auto it = impl_->touching.begin(); it != impl_->touching.end();)
        {
            if (it->second.a == body || it->second.b == body)
            {
                impl_->EndContact(it->second);
                it = impl_->touching.erase(it);
            }
            else
            {
                ++it;
            }
        }

        JPH::BodyInterface& bodies = impl_->system.GetBodyInterface();
        bodies.RemoveBody(ToJolt(body));
        bodies.DestroyBody(ToJolt(body));
    }

    bool PhysicsWorld::HasBody(BodyId body) const
    {
        // IsAdded checks the id's sequence number too, so the id of a removed body is never mistaken for a new one
        return impl_ && body.IsValid() && impl_->system.GetBodyInterface().IsAdded(ToJolt(body));
    }

    std::optional<BodyPose> PhysicsWorld::Pose(BodyId body) const
    {
        if (!HasBody(body))
        {
            return std::nullopt;
        }
        JPH::RVec3 position;
        JPH::Quat rotation;
        impl_->system.GetBodyInterface().GetPositionAndRotation(ToJolt(body), position, rotation);
        return BodyPose{.position = {static_cast<float>(position.GetX()), static_cast<float>(position.GetY()),
                                     static_cast<float>(position.GetZ())},
                        .rotation = ToGlm(rotation)};
    }

    bool PhysicsWorld::IsAwake(BodyId body) const
    {
        return HasBody(body) && impl_->system.GetBodyInterface().IsActive(ToJolt(body));
    }

    void PhysicsWorld::SetPose(BodyId body, const BodyPose& pose)
    {
        if (!HasBody(body))
        {
            return;
        }
        JPH::BodyInterface& bodies = impl_->system.GetBodyInterface();
        const JPH::EActivation activation = bodies.GetMotionType(ToJolt(body)) == JPH::EMotionType::Static
                                                ? JPH::EActivation::DontActivate
                                                : JPH::EActivation::Activate;
        bodies.SetPositionAndRotation(ToJolt(body), JPH::RVec3(pose.position.x, pose.position.y, pose.position.z),
                                      ToJolt(pose.rotation), activation);
    }

    void PhysicsWorld::SetMaterial(BodyId body, const Material& material)
    {
        if (!HasBody(body))
        {
            return;
        }
        const Material clamped = Clamped(material);
        JPH::BodyInterface& bodies = impl_->system.GetBodyInterface();
        bodies.SetFriction(ToJolt(body), clamped.friction);
        bodies.SetRestitution(ToJolt(body), clamped.bounciness);
        if (bodies.GetMotionType(ToJolt(body)) != JPH::EMotionType::Static)
        {
            bodies.ActivateBody(ToJolt(body));
        }
    }

    std::optional<Material> PhysicsWorld::MaterialOf(BodyId body) const
    {
        if (!HasBody(body))
        {
            return std::nullopt;
        }
        const JPH::BodyInterface& bodies = impl_->system.GetBodyInterface();
        return Material{.friction = bodies.GetFriction(ToJolt(body)),
                        .bounciness = bodies.GetRestitution(ToJolt(body))};
    }

    void PhysicsWorld::AddImpulse(BodyId body, const glm::vec3& impulse)
    {
        if (impl_ && impl_->IsDynamic(body))
        {
            impl_->system.GetBodyInterface().AddImpulse(ToJolt(body), ToJolt(impulse));
            if (const auto pose = Pose(body); pose && impl_->impulses.size() < kMaxKeptImpulses)
            {
                impl_->impulses.push_back({pose->position, impulse});
            }
        }
    }

    void PhysicsWorld::AddImpulseAt(BodyId body, const glm::vec3& impulse, const glm::vec3& point)
    {
        if (impl_ && impl_->IsDynamic(body))
        {
            impl_->system.GetBodyInterface().AddImpulse(ToJolt(body), ToJolt(impulse),
                                                        JPH::RVec3(point.x, point.y, point.z));
            if (impl_->impulses.size() < kMaxKeptImpulses)
            {
                impl_->impulses.push_back({point, impulse});
            }
        }
    }

    std::vector<Push> PhysicsWorld::TakePushes()
    {
        if (!impl_)
        {
            return {};
        }
        std::vector<Push> pushes = std::exchange(impl_->impulses, {});
        for (const auto& [id, force] : impl_->forces)
        {
            if (const auto pose = Pose(BodyId{id}))
            {
                pushes.push_back({pose->position, force});
            }
        }
        return pushes;
    }

    void PhysicsWorld::SetConstantForce(BodyId body, const glm::vec3& force)
    {
        if (!impl_ || !impl_->IsDynamic(body))
        {
            return;
        }
        if (force == glm::vec3(0.0f))
        {
            impl_->forces.erase(body.value);
            return;
        }
        impl_->forces[body.value] = force;
        impl_->system.GetBodyInterface().ActivateBody(ToJolt(body));
    }

    glm::vec3 PhysicsWorld::ConstantForceOf(BodyId body) const
    {
        if (!impl_)
        {
            return glm::vec3(0.0f);
        }
        const auto it = impl_->forces.find(body.value);
        return it != impl_->forces.end() ? it->second : glm::vec3(0.0f);
    }

    void PhysicsWorld::SetVelocity(BodyId body, const glm::vec3& velocity)
    {
        if (impl_ && impl_->IsDynamic(body))
        {
            impl_->system.GetBodyInterface().SetLinearVelocity(ToJolt(body), ToJolt(velocity));
        }
    }

    std::optional<glm::vec3> PhysicsWorld::Velocity(BodyId body) const
    {
        if (!HasBody(body))
        {
            return std::nullopt;
        }
        return ToGlm(impl_->system.GetBodyInterface().GetLinearVelocity(ToJolt(body)));
    }

    void PhysicsWorld::SetAngularVelocity(BodyId body, const glm::vec3& velocity)
    {
        if (impl_ && impl_->IsDynamic(body))
        {
            impl_->system.GetBodyInterface().SetAngularVelocity(ToJolt(body), ToJolt(velocity));
        }
    }

    std::optional<glm::vec3> PhysicsWorld::AngularVelocity(BodyId body) const
    {
        if (!HasBody(body))
        {
            return std::nullopt;
        }
        return ToGlm(impl_->system.GetBodyInterface().GetAngularVelocity(ToJolt(body)));
    }

    std::vector<ContactEvent> PhysicsWorld::TakeContactEvents()
    {
        if (!impl_)
        {
            return {};
        }
        return std::exchange(impl_->events, {});
    }

    std::vector<BodyId> PhysicsWorld::Touching(BodyId body) const
    {
        std::vector<BodyId> bodies;
        if (!impl_ || !body.IsValid())
        {
            return bodies;
        }
        for (const auto& [key, pair] : impl_->touching)
        {
            if (pair.a == body)
            {
                bodies.push_back(pair.b);
            }
            else if (pair.b == body)
            {
                bodies.push_back(pair.a);
            }
        }
        return bodies;
    }

    bool PhysicsWorld::AreTouching(BodyId a, BodyId b) const
    {
        return impl_ && a.IsValid() && b.IsValid() && impl_->touching.contains(PairKey(a.value, b.value));
    }

    std::size_t PhysicsWorld::ContactCount() const
    {
        return impl_ ? impl_->touching.size() : 0;
    }

    glm::vec3 PhysicsWorld::ClipWalk(BodyId body, const glm::vec3& offset) const
    {
        if (!HasBody(body))
        {
            return offset;
        }
        JPH::RefConst<JPH::Shape> shape;
        JPH::RMat44 from;
        {
            const JPH::BodyLockRead lock(impl_->system.GetBodyLockInterface(), JPH::BodyID(body.value));
            if (!lock.Succeeded())
            {
                return offset;
            }
            shape = lock.GetBody().GetShape();
            from = lock.GetBody().GetCenterOfMassTransform();
        }
        // Swept from a little above where it is, clear of the floor it stands on (and resting a little into)
        constexpr float kAboveFloor = 0.05f;
        from.SetTranslation(from.GetTranslation() + JPH::RVec3(0.0, kAboveFloor, 0.0));

        constexpr int kMaxSlides = 3;        // along a wall, then along the next one in a corner
        constexpr float kWallNormalY = 0.5f; // a surface flatter than this is climbed, not a wall
        constexpr float kGap = 0.005f;       // stops this far short of a wall, so the next step starts clear of it
        const JPH::SpecifiedObjectLayerFilter onlyStatic(Layers::NonMoving);

        glm::vec3 left(offset.x, 0.0f, offset.z);
        glm::vec3 walked(0.0f);
        for (int slide = 0; slide < kMaxSlides; ++slide)
        {
            const float length = glm::length(left);
            if (length < 1e-5f)
            {
                break;
            }
            const JPH::RShapeCast cast(shape.GetPtr(), JPH::Vec3::sOne(), from, ToJolt(left));
            JPH::AllHitCollisionCollector<JPH::CastShapeCollector> hits;
            impl_->system.GetNarrowPhaseQuery().CastShape(cast, JPH::ShapeCastSettings(), JPH::RVec3::sZero(), hits,
                                                          JPH::BroadPhaseLayerFilter(), onlyStatic);

            // The nearest wall it is going into: the floor it stands on is touched all the way, but is no wall
            float nearest = 2.0f;
            glm::vec3 wall(0.0f);
            for (const JPH::ShapeCastResult& hit : hits.mHits)
            {
                // The surface's own normal: the sweep's axis at the moment of touching can point any way
                const JPH::BodyLockRead lock(impl_->system.GetBodyLockInterface(), hit.mBodyID2);
                if (!lock.Succeeded())
                {
                    continue;
                }
                const JPH::Vec3 surface =
                    lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hit.mContactPointOn2);
                const glm::vec3 normal(surface.GetX(), surface.GetY(), surface.GetZ());
                if (hit.mFraction < nearest && std::abs(normal.y) < kWallNormalY && glm::dot(normal, left) < 0.0f)
                {
                    nearest = hit.mFraction;
                    wall = normal;
                }
            }
            if (nearest > 1.0f)
            {
                walked += left;
                break;
            }

            const float reach = std::max(nearest - kGap / length, 0.0f);
            walked += left * reach;
            from.SetTranslation(from.GetTranslation() + JPH::RVec3(ToJolt(left * reach)));
            left *= 1.0f - reach;
            left -= wall * glm::dot(left, wall);
            left.y = 0.0f; // a slanted wall is not climbed by walking along it
        }
        return {walked.x, offset.y, walked.z};
    }

    namespace
    {
        // The bodies a ray sees: not the one it ignores, and only those on a layer it looks at
        class RayBodyFilter final : public JPH::BodyFilter
        {
        public:
            RayBodyFilter(const std::unordered_map<std::uint32_t, std::uint32_t>& layers, std::uint32_t mask,
                          JPH::BodyID ignore)
                : layers_(layers), mask_(mask), ignore_(ignore)
            {
            }

            bool ShouldCollide(const JPH::BodyID& body) const override
            {
                if (body == ignore_)
                {
                    return false;
                }
                const auto found = layers_.find(body.GetIndexAndSequenceNumber());
                return found == layers_.end() || (found->second & mask_) != 0;
            }

        private:
            const std::unordered_map<std::uint32_t, std::uint32_t>& layers_;
            std::uint32_t mask_;
            JPH::BodyID ignore_;
        };
    }

    std::optional<RayHit> PhysicsWorld::CastRay(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                                                BodyId ignore, std::uint32_t layerMask) const
    {
        const float length = glm::length(direction);
        const bool finite = std::isfinite(length) && std::isfinite(maxDistance) && std::isfinite(origin.x) &&
                            std::isfinite(origin.y) && std::isfinite(origin.z);
        if (!impl_ || !finite || length <= 0.0f || maxDistance <= 0.0f)
        {
            return std::nullopt;
        }

        // Jolt's ray is as long as it reaches: the hit comes back as the fraction of it travelled
        const glm::vec3 unit = direction / length;
        const JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z), ToJolt(unit * maxDistance));
        JPH::RayCastResult result;
        const RayBodyFilter skip(impl_->layers, layerMask, ToJolt(ignore)); // an invalid id skips nothing
        if (!impl_->system.GetNarrowPhaseQuery().CastRay(ray, result, JPH::BroadPhaseLayerFilter(),
                                                         JPH::ObjectLayerFilter(), skip))
        {
            return std::nullopt;
        }

        const JPH::BodyLockRead lock(impl_->system.GetBodyLockInterface(), result.mBodyID);
        if (!lock.Succeeded())
        {
            return std::nullopt;
        }
        const JPH::Body& body = lock.GetBody();
        const JPH::RVec3 point = ray.GetPointOnRay(result.mFraction);

        // Started inside the body: there is no surface where it was hit, so the normal points back along the ray
        const glm::vec3 normal =
            result.mFraction > 0.0f ? ToGlm(body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point)) : -unit;
        return RayHit{.body = BodyId{result.mBodyID.GetIndexAndSequenceNumber()},
                      .userData = body.GetUserData(),
                      .point = {static_cast<float>(point.GetX()), static_cast<float>(point.GetY()),
                                static_cast<float>(point.GetZ())},
                      .normal = normal,
                      .distance = result.mFraction * maxDistance};
    }

    void PhysicsWorld::AddLocalImpulse(BodyId body, const glm::vec3& impulse)
    {
        if (const auto pose = Pose(body))
        {
            AddImpulse(body, pose->rotation * impulse);
        }
    }

    void PhysicsWorld::SetLocalConstantForce(BodyId body, const glm::vec3& force)
    {
        if (const auto pose = Pose(body))
        {
            SetConstantForce(body, pose->rotation * force);
        }
    }

    std::optional<Gap> PhysicsWorld::DistanceBetween(BodyId a, BodyId b, float maxDistance) const
    {
        if (!HasBody(a) || !HasBody(b) || a == b || !std::isfinite(maxDistance) || maxDistance < 0.0f)
        {
            return std::nullopt;
        }

        const std::array<JPH::BodyID, 2> ids{ToJolt(a), ToJolt(b)};
        const JPH::BodyLockMultiRead lock(impl_->system.GetBodyLockInterface(), ids.data(),
                                          static_cast<int>(ids.size()));
        const JPH::Body* bodyA = lock.GetBody(0);
        const JPH::Body* bodyB = lock.GetBody(1);
        if (bodyA == nullptr || bodyB == nullptr)
        {
            return std::nullopt;
        }

        // Jolt's shape against shape test, allowed to report shapes up to maxDistance apart: then the penetration
        // depth is negative, minus the gap, and the contact points are the nearest points
        JPH::CollideShapeSettings settings;
        settings.mMaxSeparationDistance = maxDistance;
        JPH::ClosestHitCollisionCollector<JPH::CollideShapeCollector> collector;
        JPH::CollisionDispatch::sCollideShapeVsShape(bodyA->GetShape(), bodyB->GetShape(), JPH::Vec3::sOne(),
                                                     JPH::Vec3::sOne(), bodyA->GetCenterOfMassTransform(),
                                                     bodyB->GetCenterOfMassTransform(), JPH::SubShapeIDCreator(),
                                                     JPH::SubShapeIDCreator(), settings, collector);
        if (!collector.HadHit())
        {
            return std::nullopt;
        }
        const JPH::CollideShapeResult& hit = collector.mHit;
        return Gap{.distance = std::max(-hit.mPenetrationDepth, 0.0f),
                   .pointOnA = ToGlm(hit.mContactPointOn1),
                   .pointOnB = ToGlm(hit.mContactPointOn2)};
    }

    std::vector<NearbyBody> PhysicsWorld::BodiesWithin(const glm::vec3& point, float radius, BodyId ignore) const
    {
        std::vector<NearbyBody> nearby;
        const bool finite =
            std::isfinite(radius) && std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
        if (!impl_ || !finite || radius <= 0.0f)
        {
            return nearby;
        }

        // A sphere of the radius around the point, against every collider. Made on the stack: SetEmbedded keeps
        // Jolt's reference counting from deleting it.
        JPH::SphereShape sphere(radius);
        sphere.SetEmbedded();
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
        const JPH::IgnoreSingleBodyFilter skip(ToJolt(ignore));
        impl_->system.GetNarrowPhaseQuery().CollideShape(
            &sphere, JPH::Vec3::sOne(), JPH::RMat44::sTranslation(JPH::RVec3(point.x, point.y, point.z)),
            JPH::CollideShapeSettings(), JPH::RVec3::sZero(), collector, JPH::BroadPhaseLayerFilter(),
            JPH::ObjectLayerFilter(), skip);

        // How deep the sphere goes into a collider is the radius less the point's distance from it
        const JPH::BodyInterface& bodies = impl_->system.GetBodyInterface();
        for (const JPH::CollideShapeResult& hit : collector.mHits)
        {
            const BodyId body{hit.mBodyID2.GetIndexAndSequenceNumber()};
            const float distance = std::max(radius - hit.mPenetrationDepth, 0.0f);
            const auto same = std::ranges::find(nearby, body, &NearbyBody::body);
            if (same == nearby.end())
            {
                nearby.push_back({.body = body, .userData = bodies.GetUserData(hit.mBodyID2), .distance = distance});
            }
            else
            {
                same->distance = std::min(same->distance, distance);
            }
        }
        std::ranges::sort(nearby, [](const NearbyBody& x, const NearbyBody& y)
                          { return x.distance != y.distance ? x.distance < y.distance : x.body.value < y.body.value; });
        return nearby;
    }

    std::optional<Ground> PhysicsWorld::GroundUnder(BodyId body, float maxSlope) const
    {
        if (!HasBody(body) || !std::isfinite(maxSlope))
        {
            return std::nullopt;
        }

        // Up is against gravity; a surface counts as ground when its normal is within maxSlope of it
        const float gravityLength = glm::length(gravity_);
        const glm::vec3 up = gravityLength > 0.0f ? -gravity_ / gravityLength : glm::vec3(0.0f, 1.0f, 0.0f);
        const float minUpness = std::cos(glm::radians(std::clamp(maxSlope, 0.0f, 90.0f)));

        // The body's own collider where it is, against every other one, reporting those up to GroundTolerance away.
        // The shape and transform are copied out first: the query locks the bodies it tests itself.
        const JPH::BodyInterface& bodies = impl_->system.GetBodyInterface();
        const JPH::RefConst<JPH::Shape> shape = bodies.GetShape(ToJolt(body));
        const JPH::RMat44 transform = bodies.GetCenterOfMassTransform(ToJolt(body));
        JPH::CollideShapeSettings settings;
        settings.mMaxSeparationDistance = GroundTolerance;
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
        const JPH::IgnoreSingleBodyFilter skip(ToJolt(body));
        impl_->system.GetNarrowPhaseQuery().CollideShape(shape, JPH::Vec3::sOne(), transform, settings,
                                                         JPH::RVec3::sZero(), collector, JPH::BroadPhaseLayerFilter(),
                                                         JPH::ObjectLayerFilter(), skip);

        std::optional<Ground> ground;
        float bestUpness = -1.0f;
        for (const JPH::CollideShapeResult& hit : collector.mHits)
        {
            // The penetration axis points from the body into the other one: the other's surface faces the other way
            const float axisLength = hit.mPenetrationAxis.Length();
            if (axisLength <= 0.0f)
            {
                continue;
            }
            const glm::vec3 normal = -ToGlm(hit.mPenetrationAxis) / axisLength;
            const float upness = glm::dot(normal, up);
            const bool flatter = upness > bestUpness || (upness == bestUpness && ground &&
                                                         hit.mBodyID2.GetIndexAndSequenceNumber() < ground->body.value);
            if (upness >= minUpness && flatter)
            {
                bestUpness = upness;
                ground = Ground{.body = BodyId{hit.mBodyID2.GetIndexAndSequenceNumber()},
                                .userData = bodies.GetUserData(hit.mBodyID2),
                                .point = ToGlm(hit.mContactPointOn2),
                                .normal = normal};
            }
        }
        return ground;
    }

    bool PhysicsWorld::IsOnGround(BodyId body, float maxSlope) const
    {
        return GroundUnder(body, maxSlope).has_value();
    }

    std::string PhysicsWorld::LibraryVersion()
    {
        return std::format("Jolt Physics {}.{}.{}", JPH_VERSION_MAJOR, JPH_VERSION_MINOR, JPH_VERSION_PATCH);
    }
}
