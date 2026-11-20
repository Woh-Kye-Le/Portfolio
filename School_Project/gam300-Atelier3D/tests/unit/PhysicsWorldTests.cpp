/* !
@file    	    PhysicsWorldTests.cpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		26/10/2026
@brief
    Unit tests for PhysicsWorld: starting and stopping Jolt, the fixed-rate
    update, gravity, bodies, contacts, pushing bodies, ray casts, distance
    checks and the ground bodies stand on. They run Jolt for real on every
    platform, CI included.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/physics/ColliderLines.hpp"
#include "engine/physics/PhysicsWorld.hpp"

#include <gtest/gtest.h>

#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

using Engine::Physics::BodyId;
using Engine::Physics::Collider;
using Engine::Physics::ContactEvent;
using Engine::Physics::MotionType;
using Engine::Physics::PhysicsWorld;
using Engine::Physics::ShapeType;

namespace
{
    // A static 10 x 10 floor whose top is at y = 0
    void AddFloor(PhysicsWorld& world)
    {
        world.AddBody({.motion = MotionType::Static,
                       .collider = {.halfExtents = {5.0f, 0.05f, 5.0f}, .center = {0.0f, -0.05f, 0.0f}}});
    }

    // The same floor, with its id and a user data value
    BodyId AddFloorBody(PhysicsWorld& world, std::uint64_t userData = 0)
    {
        return world.AddBody({.motion = MotionType::Static,
                              .collider = {.halfExtents = {5.0f, 0.05f, 5.0f}, .center = {0.0f, -0.05f, 0.0f}},
                              .userData = userData});
    }

    // The events of one kind between two bodies, in either order
    int CountEvents(const std::vector<ContactEvent>& events, ContactEvent::Kind kind, BodyId x, BodyId y)
    {
        int count = 0;
        for (const ContactEvent& event : events)
        {
            if (event.kind == kind && ((event.a == x && event.b == y) || (event.a == y && event.b == x)))
            {
                ++count;
            }
        }
        return count;
    }

    void RunSeconds(PhysicsWorld& world, double seconds)
    {
        for (double time = 0.0; time < seconds; time += PhysicsWorld::FixedStep)
        {
            world.Update(PhysicsWorld::FixedStep);
        }
    }

    using Kind = ContactEvent::Kind;
}

// Before Init() nothing runs, but every call is safe.
TEST(PhysicsWorld, NothingRunsBeforeInit)
{
    PhysicsWorld world;
    EXPECT_FALSE(world.IsInitialized());
    EXPECT_EQ(world.Update(1.0), 0);
    EXPECT_EQ(world.BodyCount(), 0u);
    EXPECT_EQ(world.StepCount(), 0u);
    world.Shutdown(); // safe without Init
}

// Init() starts Jolt, a second Init() is a no-op, and the world can be stopped and started again.
TEST(PhysicsWorld, StartsStopsAndRestarts)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    EXPECT_TRUE(world.IsInitialized());
    EXPECT_TRUE(world.Init()); // already running
    EXPECT_EQ(world.BodyCount(), 0u);

    world.Shutdown();
    EXPECT_FALSE(world.IsInitialized());
    world.Shutdown(); // twice is fine

    ASSERT_TRUE(world.Init());
    EXPECT_EQ(world.Update(PhysicsWorld::FixedStep), 1);
}

// The world steps in fixed steps: time shorter than a step is carried over to the next Update().
TEST(PhysicsWorld, StepsAtAFixedRate)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());

    EXPECT_EQ(world.Update(PhysicsWorld::FixedStep), 1);
    EXPECT_EQ(world.Update(PhysicsWorld::FixedStep * 0.5), 0); // half a step: nothing yet
    EXPECT_EQ(world.Update(PhysicsWorld::FixedStep * 0.5), 1); // the other half completes it
    EXPECT_EQ(world.Update(PhysicsWorld::FixedStep * 2.0), 2);

    // Three frames at 180 FPS make one step at 60, despite rounding
    EXPECT_EQ(world.Update(1.0 / 180.0), 0);
    EXPECT_EQ(world.Update(1.0 / 180.0), 0);
    EXPECT_EQ(world.Update(1.0 / 180.0), 1);

    EXPECT_EQ(world.StepCount(), 5u);
    EXPECT_EQ(world.Update(-1.0), 0); // a negative time is ignored
}

// The debugger's frame step passes exactly one fixed step of time, so it must move the bodies exactly one step, however
// much time was left over before it.
TEST(PhysicsWorld, OneStepOfTimeIsAlwaysOneStep)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());

    EXPECT_EQ(world.Update(PhysicsWorld::FixedStep), 1);
    EXPECT_EQ(world.Update(PhysicsWorld::FixedStep * 0.7), 0); // 0.7 of a step left over
    EXPECT_EQ(world.Update(PhysicsWorld::FixedStep), 1);       // 1.7 of a step: one step, 0.7 left
    EXPECT_EQ(world.Update(PhysicsWorld::FixedStep), 1);
    EXPECT_EQ(world.StepCount(), 3u);
}

// A body too fast to touch a thin wall in any one step still stops at it, rather than landing on its far side
TEST(PhysicsWorld, AFastBodyDoesNotPassThroughAThinWall)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    world.SetGravity({0.0f, 0.0f, 0.0f});
    world.AddBody({.motion = MotionType::Static,
                   .collider = {.halfExtents = {0.05f, 5.0f, 5.0f}}});
    const BodyId ball = world.AddBody({.motion = MotionType::Dynamic,
                                       .position = {-5.0f, 0.0f, 0.0f},
                                       .collider = {.shape = ShapeType::Sphere, .radius = 0.25f}});
    world.SetVelocity(ball, {300.0f, 0.0f, 0.0f}); // 5 m a step

    for (int step = 0; step < 10; ++step)
    {
        world.Update(PhysicsWorld::FixedStep);
    }
    ASSERT_TRUE(world.Pose(ball).has_value());
    EXPECT_LT(world.Pose(ball)->position.x, 0.0f);
}

// A walk stops at a wall and slides along it; the floor it stands on and moving bodies do not stop it
TEST(PhysicsWorld, AWalkStopsAtAWallAndSlidesAlongIt)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    AddFloor(world);
    world.AddBody({.motion = MotionType::Static,
                   .position = {3.0f, 1.0f, 0.0f},
                   .collider = {.halfExtents = {0.5f, 1.0f, 5.0f}}}); // its near face at x = 2.5
    const BodyId walker = world.AddBody({.motion = MotionType::Dynamic,
                                         .position = {0.0f, 0.5f, 0.0f},
                                         .collider = {.halfExtents = {0.5f, 0.5f, 0.5f}}}); // its front at x = 0.5

    const glm::vec3 open = world.ClipWalk(walker, {1.0f, 0.0f, 0.0f});
    EXPECT_NEAR(open.x, 1.0f, 1e-3f) << "the floor under it does not stop it";

    const glm::vec3 far = world.ClipWalk(walker, {10.0f, 0.0f, 0.0f});
    EXPECT_NEAR(far.x, 2.0f, 0.02f) << "up to the wall";

    const glm::vec3 slid = world.ClipWalk(walker, {10.0f, 0.0f, 3.0f});
    EXPECT_LE(slid.x, 2.01f);
    EXPECT_GT(slid.z, 2.0f) << "and on along it";

    const glm::vec3 away = world.ClipWalk(walker, {-1.0f, 0.5f, 0.0f});
    EXPECT_NEAR(away.x, -1.0f, 1e-3f) << "away from it is clear";
    EXPECT_FLOAT_EQ(away.y, 0.5f) << "up and down is left alone";

    const BodyId crate = world.AddBody({.motion = MotionType::Dynamic,
                                        .position = {1.5f, 0.5f, 0.0f},
                                        .collider = {.halfExtents = {0.2f, 0.2f, 0.2f}}});
    ASSERT_TRUE(crate.IsValid());
    EXPECT_NEAR(world.ClipWalk(walker, {1.5f, 0.0f, 0.0f}).x, 1.5f, 1e-3f) << "it can push a moving body";
}

// A very long frame runs at most MaxStepsPerUpdate steps, and the backlog is dropped rather than carried over.
TEST(PhysicsWorld, LongFramesAreCapped)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());

    EXPECT_EQ(world.Update(2.0), PhysicsWorld::MaxStepsPerUpdate);
    EXPECT_EQ(world.Update(0.0), 0);
}

// Gravity defaults to Earth's, pulling down Y, and a value set before Init() is kept.
TEST(PhysicsWorld, GravityCanBeChanged)
{
    PhysicsWorld world;
    EXPECT_FLOAT_EQ(world.Gravity().y, -9.81f);

    world.SetGravity({0.0f, -1.62f, 0.0f}); // the Moon, before Init
    ASSERT_TRUE(world.Init());
    EXPECT_FLOAT_EQ(world.Gravity().y, -1.62f);

    world.SetGravity({1.0f, 0.0f, 0.0f});
    EXPECT_FLOAT_EQ(world.Gravity().x, 1.0f);
    EXPECT_FLOAT_EQ(world.Gravity().y, 0.0f);
}

// Jolt is set up once per process: stopping one world must not break another that is still running.
TEST(PhysicsWorld, TwoWorldsRunSideBySide)
{
    PhysicsWorld first;
    PhysicsWorld second;
    ASSERT_TRUE(first.Init());
    ASSERT_TRUE(second.Init());

    first.Shutdown();
    EXPECT_EQ(second.Update(PhysicsWorld::FixedStep), 1);
    EXPECT_TRUE(second.IsInitialized());
}

// The version string names the library, for the log and the editor.
TEST(PhysicsWorld, ReportsTheJoltVersion)
{
    EXPECT_EQ(PhysicsWorld::LibraryVersion().rfind("Jolt Physics ", 0), 0u);
}

// Bodies need a running world; before Init() every body call is safe and does nothing.
TEST(PhysicsWorld, BodiesNeedARunningWorld)
{
    PhysicsWorld world;
    EXPECT_FALSE(world.AddBody({}).IsValid());
    world.RemoveBody({});
    EXPECT_FALSE(world.HasBody({}));
    EXPECT_FALSE(world.Pose({}).has_value());
    world.SetPose({}, {});
}

// A dynamic body falls under gravity; a static one stays where it was put.
TEST(PhysicsWorld, DynamicBodiesFallAndStaticOnesStay)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId falling = world.AddBody({.motion = MotionType::Dynamic, .position = {0.0f, 10.0f, 0.0f}});
    const BodyId fixed = world.AddBody({.motion = MotionType::Static, .position = {5.0f, 10.0f, 0.0f}});
    ASSERT_TRUE(falling.IsValid());
    ASSERT_TRUE(fixed.IsValid());
    EXPECT_EQ(world.BodyCount(), 2u);

    RunSeconds(world, 1.0);

    // One second of free fall is about 4.9 units
    EXPECT_NEAR(world.Pose(falling)->position.y, 10.0f - 4.9f, 0.3f);
    EXPECT_FLOAT_EQ(world.Pose(fixed)->position.y, 10.0f);
}

// A box dropped on a thin static floor (moved down so its top is at 0) comes to rest on top of it.
TEST(PhysicsWorld, ABoxComesToRestOnTheFloor)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    world.AddBody({.motion = MotionType::Static,
                   .collider = {.halfExtents = {5.0f, 0.05f, 5.0f}, .center = {0.0f, -0.05f, 0.0f}}});
    const BodyId box = world.AddBody({.motion = MotionType::Dynamic, .position = {0.0f, 3.0f, 0.0f}});

    RunSeconds(world, 3.0);

    EXPECT_NEAR(world.Pose(box)->position.y, 0.5f, 0.05f); // half its height above the floor
}

// A removed body is gone, and its id stays invalid even when a new body takes its place.
TEST(PhysicsWorld, RemovedBodiesStayGone)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId first = world.AddBody({});
    world.RemoveBody(first);
    EXPECT_FALSE(world.HasBody(first));
    EXPECT_FALSE(world.Pose(first).has_value());
    EXPECT_EQ(world.BodyCount(), 0u);

    const BodyId second = world.AddBody({});
    EXPECT_FALSE(world.HasBody(first));
    world.RemoveBody(first); // must not remove the new body
    EXPECT_TRUE(world.HasBody(second));
    EXPECT_EQ(world.BodyCount(), 1u);
}

// SetPose puts a body somewhere else straight away, turned as asked.
TEST(PhysicsWorld, SetPoseMovesABodyStraightAway)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId body = world.AddBody({.motion = MotionType::Static});
    const glm::quat turned = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    world.SetPose(body, {.position = {3.0f, 4.0f, 5.0f}, .rotation = turned});

    const auto pose = world.Pose(body);
    ASSERT_TRUE(pose.has_value());
    EXPECT_NEAR(pose->position.x, 3.0f, 1e-5f);
    EXPECT_NEAR(pose->position.y, 4.0f, 1e-5f);
    EXPECT_NEAR(pose->position.z, 5.0f, 1e-5f);
    EXPECT_NEAR(std::abs(glm::dot(pose->rotation, turned)), 1.0f, 1e-5f);
}

// Each collider shape rests on the floor at its own height: a sphere at its radius, a standing capsule at half its
// full height, a capsule with no straight middle like a sphere.
TEST(PhysicsWorld, EachShapeRestsAtItsOwnHeight)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    AddFloor(world);
    const BodyId sphere =
        world.AddBody({.position = {-3.0f, 2.0f, 0.0f}, .collider = {.shape = ShapeType::Sphere, .radius = 0.75f}});
    const BodyId capsule =
        world.AddBody({.position = {0.0f, 3.0f, 0.0f},
                       .collider = {.shape = ShapeType::Capsule, .radius = 0.5f, .halfHeight = 0.5f}});
    const BodyId round = world.AddBody({.position = {3.0f, 2.0f, 0.0f},
                                        .collider = {.shape = ShapeType::Capsule, .radius = 0.5f, .halfHeight = 0.0f}});
    ASSERT_TRUE(sphere.IsValid());
    ASSERT_TRUE(capsule.IsValid());
    ASSERT_TRUE(round.IsValid());

    RunSeconds(world, 3.0);

    EXPECT_NEAR(world.Pose(sphere)->position.y, 0.75f, 0.05f);
    EXPECT_NEAR(world.Pose(capsule)->position.y, 1.0f, 0.05f); // 0.5 straight + 0.5 rounded end below the centre
    EXPECT_NEAR(world.Pose(round)->position.y, 0.5f, 0.05f);
}

// A collider moved off the body's position: the body rests higher by as much as its collider was moved down.
TEST(PhysicsWorld, ACentredOffColliderMovesTheRestingPlace)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    AddFloor(world);
    const BodyId body =
        world.AddBody({.position = {0.0f, 3.0f, 0.0f},
                       .collider = {.shape = ShapeType::Sphere, .radius = 0.5f, .center = {0.0f, -1.0f, 0.0f}}});

    RunSeconds(world, 3.0);

    EXPECT_NEAR(world.Pose(body)->position.y, 1.5f, 0.05f);
}

// Sizes of 0 or below are raised to a small minimum instead of failing.
TEST(PhysicsWorld, ZeroSizedCollidersStillMakeABody)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    EXPECT_TRUE(world.AddBody({.collider = {.shape = ShapeType::Box, .halfExtents = glm::vec3(0.0f)}}).IsValid());
    EXPECT_TRUE(world.AddBody({.collider = {.shape = ShapeType::Sphere, .radius = -1.0f}}).IsValid());
    EXPECT_TRUE(
        world.AddBody({.collider = {.shape = ShapeType::Capsule, .radius = 0.0f, .halfHeight = 1.0f}}).IsValid());
}

// A body's gravity scale multiplies the world's gravity: 0 floats, 2 falls twice as far in the same time.
TEST(PhysicsWorld, GravityScaleMultipliesGravity)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId normal = world.AddBody({.position = {0.0f, 10.0f, 0.0f}});
    const BodyId floating = world.AddBody({.position = {3.0f, 10.0f, 0.0f}, .gravityScale = 0.0f});
    const BodyId heavy = world.AddBody({.position = {6.0f, 10.0f, 0.0f}, .gravityScale = 2.0f});

    RunSeconds(world, 1.0);

    const float normalDrop = 10.0f - world.Pose(normal)->position.y;
    EXPECT_NEAR(world.Pose(floating)->position.y, 10.0f, 1e-4f);
    EXPECT_NEAR(10.0f - world.Pose(heavy)->position.y, 2.0f * normalDrop, 0.05f);
}

// The world's gravity can point any way, and changes reach bodies that are already falling.
TEST(PhysicsWorld, GravityCanPointAnyWay)
{
    PhysicsWorld world;
    world.SetGravity({5.0f, 0.0f, 0.0f}); // sideways
    ASSERT_TRUE(world.Init());
    const BodyId body = world.AddBody({});

    RunSeconds(world, 1.0);
    EXPECT_GT(world.Pose(body)->position.x, 2.0f);
    EXPECT_NEAR(world.Pose(body)->position.y, 0.0f, 1e-4f);

    world.SetGravity({0.0f, 0.0f, 0.0f});
    const float x = world.Pose(body)->position.x;
    RunSeconds(world, 0.5);
    EXPECT_GT(world.Pose(body)->position.x, x) << "keeps the speed it had";
    EXPECT_NEAR(world.Pose(body)->position.y, 0.0f, 1e-4f);
}

// A body that sat still with no gravity has gone to sleep; turning gravity back on wakes it so it falls.
TEST(PhysicsWorld, TurningGravityOnWakesSleepingBodies)
{
    PhysicsWorld world;
    world.SetGravity({0.0f, 0.0f, 0.0f});
    ASSERT_TRUE(world.Init());
    const BodyId body = world.AddBody({.position = {0.0f, 5.0f, 0.0f}});

    RunSeconds(world, 2.0); // Jolt puts bodies that barely move to sleep after half a second
    EXPECT_NEAR(world.Pose(body)->position.y, 5.0f, 1e-4f);

    world.SetGravity({0.0f, -9.81f, 0.0f});
    RunSeconds(world, 0.5);
    EXPECT_LT(world.Pose(body)->position.y, 4.0f);
}

// Before Init() there are no contacts, and asking is safe.
TEST(PhysicsWorld, NoContactsBeforeInit)
{
    PhysicsWorld world;
    EXPECT_TRUE(world.TakeContactEvents().empty());
    EXPECT_TRUE(world.Touching({}).empty());
    EXPECT_FALSE(world.AreTouching({}, {}));
    EXPECT_EQ(world.ContactCount(), 0u);
}

// A box landing on the floor begins one contact, with where, which way and how hard it hit, and each body's user
// data. The events are handed over once.
TEST(PhysicsWorld, LandingBeginsAContact)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId floor = AddFloorBody(world, 7);
    const BodyId box = world.AddBody({.position = {0.0f, 2.0f, 0.0f}, .userData = 42});

    RunSeconds(world, 1.0);

    const std::vector<ContactEvent> events = world.TakeContactEvents();
    ASSERT_EQ(events.size(), 1u);
    const ContactEvent& hit = events[0];
    EXPECT_EQ(hit.kind, Kind::Began);
    // The moving body comes first
    EXPECT_EQ(hit.a, box);
    EXPECT_EQ(hit.b, floor);
    EXPECT_EQ(hit.userDataA, 42u);
    EXPECT_EQ(hit.userDataB, 7u);

    // The normal points from a to b: down, from the box into the floor
    EXPECT_NEAR(hit.normal.y, -1.0f, 1e-3f);
    EXPECT_NEAR(hit.point.y, 0.0f, 0.05f);
    // Falling 1.5 units (its bottom starts 1.5 above the floor) is about 5.4 units per second
    EXPECT_NEAR(hit.impactSpeed, 5.4f, 0.4f);

    EXPECT_TRUE(world.AreTouching(box, floor));
    EXPECT_TRUE(world.AreTouching(floor, box));
    EXPECT_EQ(world.Touching(box), std::vector<BodyId>{floor});
    EXPECT_EQ(world.ContactCount(), 1u);
    EXPECT_TRUE(world.TakeContactEvents().empty()) << "taken already";
}

// A box resting on the floor goes to sleep; it still lies on the floor, so the contact does not end.
TEST(PhysicsWorld, ASleepingBodyKeepsTouching)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId floor = AddFloorBody(world);
    const BodyId box = world.AddBody({.position = {0.0f, 0.6f, 0.0f}});

    RunSeconds(world, 4.0); // long enough to settle and sleep

    const std::vector<ContactEvent> events = world.TakeContactEvents();
    EXPECT_EQ(CountEvents(events, Kind::Began, floor, box), 1);
    EXPECT_EQ(CountEvents(events, Kind::Ended, floor, box), 0);
    EXPECT_TRUE(world.AreTouching(box, floor));

    // Waking it (gravity changes) does not begin the contact again
    world.SetGravity({0.0f, -9.81f, 0.0f});
    RunSeconds(world, 0.5);
    EXPECT_TRUE(world.TakeContactEvents().empty());
    EXPECT_TRUE(world.AreTouching(box, floor));
}

// Lifting a box off the floor ends the contact, also when it was asleep there; putting it back begins it again.
TEST(PhysicsWorld, LiftingABodyEndsTheContact)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId floor = AddFloorBody(world);
    const BodyId box = world.AddBody({.position = {0.0f, 0.6f, 0.0f}});
    RunSeconds(world, 4.0);
    world.TakeContactEvents();

    world.SetPose(box, {.position = {0.0f, 5.0f, 0.0f}});
    world.Update(PhysicsWorld::FixedStep);

    std::vector<ContactEvent> events = world.TakeContactEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(CountEvents(events, Kind::Ended, floor, box), 1);
    EXPECT_FALSE(world.AreTouching(box, floor));
    EXPECT_EQ(world.ContactCount(), 0u);

    RunSeconds(world, 2.0);
    events = world.TakeContactEvents();
    EXPECT_EQ(CountEvents(events, Kind::Began, floor, box), 1);
    EXPECT_TRUE(world.AreTouching(box, floor));
}

// Removing a body ends its contacts straight away, with the user data it was made with.
TEST(PhysicsWorld, RemovingABodyEndsItsContacts)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId floor = AddFloorBody(world, 7);
    const BodyId box = world.AddBody({.position = {0.0f, 0.6f, 0.0f}, .userData = 42});
    RunSeconds(world, 1.0);
    world.TakeContactEvents();

    world.RemoveBody(floor);

    const std::vector<ContactEvent> events = world.TakeContactEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].kind, Kind::Ended);
    EXPECT_EQ(CountEvents(events, Kind::Ended, floor, box), 1);
    EXPECT_EQ(events[0].userDataA + events[0].userDataB, 7u + 42u);
    EXPECT_TRUE(world.Touching(box).empty());

    // Jolt's own report of the lost contact in the next step changes nothing
    world.Update(PhysicsWorld::FixedStep);
    EXPECT_TRUE(world.TakeContactEvents().empty());
}

// A body pulled sideways into a wall reports the hit along the X axis, with the speed it had reached.
TEST(PhysicsWorld, ABodyHitsAWallSideways)
{
    PhysicsWorld world;
    world.SetGravity({4.0f, 0.0f, 0.0f});
    ASSERT_TRUE(world.Init());
    const BodyId box = world.AddBody({.position = {-3.0f, 0.0f, 0.0f}});
    const BodyId wall = world.AddBody({.motion = MotionType::Static, .position = {3.0f, 0.0f, 0.0f}});

    RunSeconds(world, 2.0);

    // 5 units between them at 4 units per second squared: about 6.3 units per second at the wall
    const std::vector<ContactEvent> events = world.TakeContactEvents();
    ASSERT_EQ(events.size(), 1u);
    ASSERT_EQ(CountEvents(events, Kind::Began, box, wall), 1);
    EXPECT_NEAR(events[0].impactSpeed, 6.3f, 0.4f);
    EXPECT_NEAR(std::abs(events[0].normal.x), 1.0f, 1e-3f);
    EXPECT_NEAR(events[0].point.x, 2.5f, 0.05f);
}

// A kinematic body reports touching a static one, though neither pushes the other.
TEST(PhysicsWorld, KinematicBodiesReportTouchingStaticOnes)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId floor = AddFloorBody(world);
    const BodyId platform = world.AddBody({.motion = MotionType::Kinematic, .position = {0.0f, 0.4f, 0.0f}});

    RunSeconds(world, 0.1);

    EXPECT_TRUE(world.AreTouching(platform, floor));
    EXPECT_NEAR(world.Pose(platform)->position.y, 0.4f, 1e-4f) << "the floor does not push it";
}

// Stopping the world forgets every contact.
TEST(PhysicsWorld, ShutdownForgetsContacts)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    AddFloorBody(world);
    world.AddBody({.position = {0.0f, 0.6f, 0.0f}});
    RunSeconds(world, 1.0);
    ASSERT_EQ(world.ContactCount(), 1u);

    world.Shutdown();
    EXPECT_EQ(world.ContactCount(), 0u);
    EXPECT_TRUE(world.TakeContactEvents().empty());
    ASSERT_TRUE(world.Init());
    EXPECT_EQ(world.ContactCount(), 0u);
}

// An impulse changes a dynamic body's velocity by impulse / mass straight away; static and kinematic bodies ignore it.
TEST(PhysicsWorld, ImpulsesKickDynamicBodies)
{
    PhysicsWorld world;
    world.SetGravity({0.0f, 0.0f, 0.0f});
    ASSERT_TRUE(world.Init());
    const BodyId ball = world.AddBody({.mass = 2.0f});
    const BodyId wall = world.AddBody({.motion = MotionType::Static, .position = {5.0f, 0.0f, 0.0f}});

    world.AddImpulse(ball, {4.0f, 0.0f, 0.0f});
    world.AddImpulse(wall, {4.0f, 0.0f, 0.0f});

    EXPECT_NEAR(world.Velocity(ball)->x, 2.0f, 1e-4f);
    EXPECT_EQ(*world.Velocity(wall), glm::vec3(0.0f));
    EXPECT_FALSE(world.Velocity({}).has_value());
}

// A constant force keeps pushing every step until it is set back to zero.
TEST(PhysicsWorld, ConstantForcesPushUntilStopped)
{
    PhysicsWorld world;
    world.SetGravity({0.0f, 0.0f, 0.0f});
    ASSERT_TRUE(world.Init());
    const BodyId body = world.AddBody({.mass = 2.0f});

    world.SetConstantForce(body, {0.0f, 0.0f, 6.0f}); // 3 units per second squared
    EXPECT_EQ(world.ConstantForceOf(body), glm::vec3(0.0f, 0.0f, 6.0f));
    RunSeconds(world, 1.0);
    EXPECT_NEAR(world.Velocity(body)->z, 3.0f, 0.05f);

    world.SetConstantForce(body, glm::vec3(0.0f));
    RunSeconds(world, 1.0);
    EXPECT_NEAR(world.Velocity(body)->z, 3.0f, 0.05f) << "keeps the speed it had";
}

// A ray hits the nearest body in its way: which body, how far, where on its surface and which way that faces. Bodies
// can be hit as soon as they are added, before any step.
TEST(PhysicsWorld, RaysHitTheNearestBody)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId floor = AddFloorBody(world, 7);
    const BodyId box = world.AddBody({.motion = MotionType::Static, .position = {0.0f, 1.0f, 0.0f}, .userData = 8});

    // Straight down onto the box's top (the direction can be any length)
    const auto top = world.CastRay({0.0f, 10.0f, 0.0f}, {0.0f, -2.0f, 0.0f}, 20.0f);
    ASSERT_TRUE(top.has_value());
    EXPECT_EQ(top->body, box);
    EXPECT_EQ(top->userData, 8u);
    EXPECT_NEAR(top->distance, 8.5f, 1e-3f);
    EXPECT_NEAR(top->point.y, 1.5f, 1e-3f);
    EXPECT_NEAR(top->normal.y, 1.0f, 1e-3f);

    // Passing through the box, the floor under it
    const auto below = world.CastRay({0.0f, 10.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 20.0f, box);
    ASSERT_TRUE(below.has_value());
    EXPECT_EQ(below->body, floor);
    EXPECT_EQ(below->userData, 7u);
    EXPECT_NEAR(below->distance, 10.0f, 1e-3f);

    // From the side, onto the box's left face
    const auto side = world.CastRay({-4.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 20.0f);
    ASSERT_TRUE(side.has_value());
    EXPECT_EQ(side->body, box);
    EXPECT_NEAR(side->distance, 3.5f, 1e-3f);
    EXPECT_NEAR(side->point.x, -0.5f, 1e-3f);
    EXPECT_NEAR(side->normal.x, -1.0f, 1e-3f);

    // Too short to reach anything, or aimed away from everything
    EXPECT_FALSE(world.CastRay({0.0f, 10.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 8.0f).has_value());
    EXPECT_FALSE(world.CastRay({0.0f, 10.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 100.0f).has_value());
}

// The normal is the surface's own: on a sphere it points out from the centre, whichever way the ray came.
TEST(PhysicsWorld, RayNormalsPointOutOfTheSurface)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    world.AddBody({.motion = MotionType::Static, .collider = {.shape = ShapeType::Sphere, .radius = 1.0f}});

    const auto hit = world.CastRay({0.6f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 10.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(hit->point.y, 0.8f, 1e-3f);
    EXPECT_NEAR(hit->normal.x, 0.6f, 1e-3f);
    EXPECT_NEAR(hit->normal.y, 0.8f, 1e-3f);
    EXPECT_NEAR(glm::length(hit->normal), 1.0f, 1e-4f);
}

// Rays see bodies where they are now: after falling, after SetPose; a removed body is gone.
TEST(PhysicsWorld, RaysSeeBodiesWhereTheyAre)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId floor = AddFloorBody(world);
    const BodyId box = world.AddBody({.position = {0.0f, 5.0f, 0.0f}});
    const glm::vec3 down(0.0f, -1.0f, 0.0f);

    EXPECT_NEAR(world.CastRay({0.0f, 10.0f, 0.0f}, down, 20.0f)->distance, 4.5f, 1e-3f);
    RunSeconds(world, 3.0); // landed: its top at 1
    EXPECT_NEAR(world.CastRay({0.0f, 10.0f, 0.0f}, down, 20.0f)->distance, 9.0f, 0.02f);

    world.SetPose(box, {.position = {3.0f, 2.0f, 0.0f}});
    EXPECT_EQ(world.CastRay({0.0f, 10.0f, 0.0f}, down, 20.0f)->body, floor);
    EXPECT_NEAR(world.CastRay({3.0f, 10.0f, 0.0f}, down, 20.0f)->distance, 7.5f, 1e-3f);

    world.RemoveBody(box);
    EXPECT_EQ(world.CastRay({3.0f, 10.0f, 0.0f}, down, 20.0f)->body, floor);
}

// A ray that starts inside a body hits it at once, its normal pointing back along the ray. Rays that cannot be cast
// hit nothing.
TEST(PhysicsWorld, RaysFromInsideAndRaysThatCannotBeCast)
{
    PhysicsWorld world;
    const glm::vec3 down(0.0f, -1.0f, 0.0f);
    EXPECT_FALSE(world.CastRay({0.0f, 5.0f, 0.0f}, down, 10.0f).has_value()) << "before Init";

    ASSERT_TRUE(world.Init());
    const BodyId box = world.AddBody({.motion = MotionType::Static});
    const auto inside = world.CastRay({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 10.0f);
    ASSERT_TRUE(inside.has_value());
    EXPECT_EQ(inside->body, box);
    EXPECT_EQ(inside->distance, 0.0f);
    EXPECT_EQ(inside->normal, glm::vec3(-1.0f, 0.0f, 0.0f));

    constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
    constexpr float kInfinity = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(world.CastRay({0.0f, 5.0f, 0.0f}, glm::vec3(0.0f), 10.0f).has_value()) << "no direction";
    EXPECT_FALSE(world.CastRay({0.0f, 5.0f, 0.0f}, down, 0.0f).has_value()) << "no length";
    EXPECT_FALSE(world.CastRay({0.0f, 5.0f, 0.0f}, down, -10.0f).has_value());
    EXPECT_FALSE(world.CastRay({0.0f, 5.0f, 0.0f}, down, kInfinity).has_value());
    EXPECT_FALSE(world.CastRay({kNaN, 5.0f, 0.0f}, down, 10.0f).has_value());
    EXPECT_FALSE(world.CastRay({0.0f, 5.0f, 0.0f}, {0.0f, kNaN, 0.0f}, 10.0f).has_value());
}

// The distance between two bodies is the gap between their surfaces, not their centres: a big wall is near as soon as
// its face is. It follows the bodies when they move.
TEST(PhysicsWorld, DistancesAreBetweenSurfaces)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId left = world.AddBody({.motion = MotionType::Static});
    const BodyId right = world.AddBody({.motion = MotionType::Static, .position = {3.0f, 0.0f, 0.0f}});

    const auto gap = world.DistanceBetween(left, right);
    ASSERT_TRUE(gap.has_value());
    EXPECT_NEAR(gap->distance, 2.0f, 1e-3f);
    EXPECT_NEAR(gap->pointOnA.x, 0.5f, 1e-3f);
    EXPECT_NEAR(gap->pointOnB.x, 2.5f, 1e-3f);
    EXPECT_NEAR(world.DistanceBetween(right, left)->pointOnA.x, 2.5f, 1e-3f) << "the order says which point is which";

    // Spheres: along the line between their centres, 5 apart
    const Collider ball{.shape = ShapeType::Sphere, .radius = 1.0f};
    const BodyId a = world.AddBody({.motion = MotionType::Static, .position = {0.0f, 10.0f, 0.0f}, .collider = ball});
    const BodyId b = world.AddBody({.motion = MotionType::Static, .position = {3.0f, 14.0f, 0.0f}, .collider = ball});
    const auto balls = world.DistanceBetween(a, b);
    ASSERT_TRUE(balls.has_value());
    EXPECT_NEAR(balls->distance, 3.0f, 1e-3f);
    EXPECT_NEAR(balls->pointOnA.x, 0.6f, 1e-3f);
    EXPECT_NEAR(balls->pointOnA.y, 10.8f, 1e-3f);

    // A wall 10 high: its face is 3 from the small ball, though its centre is 4 away
    const BodyId wall = world.AddBody({.motion = MotionType::Static,
                                       .position = {-4.0f, 10.0f, 0.0f},
                                       .collider = {.halfExtents = {0.5f, 5.0f, 5.0f}}});
    const BodyId pebble = world.AddBody({.motion = MotionType::Static,
                                         .position = {0.0f, 12.0f, 0.0f},
                                         .collider = {.shape = ShapeType::Sphere, .radius = 0.5f}});
    EXPECT_NEAR(world.DistanceBetween(pebble, wall)->distance, 3.0f, 1e-3f);

    // Moved next to it, then into it
    world.SetPose(right, {.position = {1.0f, 0.0f, 0.0f}});
    EXPECT_NEAR(world.DistanceBetween(left, right)->distance, 0.0f, 1e-3f) << "touching";
    world.SetPose(right, {.position = {0.6f, 0.0f, 0.0f}});
    EXPECT_EQ(world.DistanceBetween(left, right)->distance, 0.0f) << "overlapping";
}

// Bodies farther apart than the limit are out of range. Bodies that do not exist, or one body with itself, have no
// distance.
TEST(PhysicsWorld, DistancesOutOfRangeOrWithoutBodies)
{
    PhysicsWorld world;
    EXPECT_FALSE(world.DistanceBetween({}, {}).has_value()) << "before Init";
    ASSERT_TRUE(world.Init());
    const BodyId a = world.AddBody({.motion = MotionType::Static});
    const BodyId b = world.AddBody({.motion = MotionType::Static, .position = {3.0f, 0.0f, 0.0f}});

    EXPECT_FALSE(world.DistanceBetween(a, b, 1.0f).has_value());
    EXPECT_TRUE(world.DistanceBetween(a, b, 2.5f).has_value());
    EXPECT_FALSE(world.DistanceBetween(a, a).has_value());
    EXPECT_FALSE(world.DistanceBetween(a, {}).has_value());
    EXPECT_FALSE(world.DistanceBetween(a, b, std::numeric_limits<float>::quiet_NaN()).has_value());
    world.RemoveBody(b);
    EXPECT_FALSE(world.DistanceBetween(a, b).has_value());
}

// The bodies within a distance of a point, nearest first, each as far as its nearest surface: 0 for a body the point
// is inside. One body can be left out.
TEST(PhysicsWorld, BodiesWithinADistanceOfAPoint)
{
    PhysicsWorld world;
    EXPECT_TRUE(world.BodiesWithin({}, 1.0f).empty()) << "before Init";
    ASSERT_TRUE(world.Init());
    const BodyId floor = AddFloorBody(world, 7);
    const BodyId box = world.AddBody({.motion = MotionType::Static, .position = {2.0f, 0.5f, 0.0f}, .userData = 8});
    const glm::vec3 point(0.0f, 0.5f, 0.0f);

    const auto both = world.BodiesWithin(point, 3.0f);
    ASSERT_EQ(both.size(), 2u);
    EXPECT_EQ(both[0].body, floor);
    EXPECT_EQ(both[0].userData, 7u);
    EXPECT_NEAR(both[0].distance, 0.5f, 1e-3f);
    EXPECT_EQ(both[1].body, box);
    EXPECT_EQ(both[1].userData, 8u);
    EXPECT_NEAR(both[1].distance, 1.5f, 1e-3f);

    const auto close = world.BodiesWithin(point, 1.0f);
    ASSERT_EQ(close.size(), 1u);
    EXPECT_EQ(close[0].body, floor);

    const auto others = world.BodiesWithin(point, 3.0f, floor);
    ASSERT_EQ(others.size(), 1u);
    EXPECT_EQ(others[0].body, box);

    const auto inside = world.BodiesWithin({2.0f, 0.5f, 0.0f}, 0.1f);
    ASSERT_EQ(inside.size(), 1u);
    EXPECT_EQ(inside[0].body, box);
    EXPECT_EQ(inside[0].distance, 0.0f);

    EXPECT_TRUE(world.BodiesWithin(point, 0.0f).empty());
    EXPECT_TRUE(world.BodiesWithin(point, -1.0f).empty());
    EXPECT_TRUE(world.BodiesWithin(point, std::numeric_limits<float>::infinity()).empty());
    EXPECT_TRUE(world.BodiesWithin({0.0f, 50.0f, 0.0f}, 3.0f).empty()) << "nothing up there";
}

// A body is on the ground once it lands, on what it landed on with the surface facing up; while it falls it is not.
TEST(PhysicsWorld, ABodyThatLandsIsOnTheGround)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId floor = AddFloorBody(world, 7);
    const BodyId box = world.AddBody({.position = {0.0f, 3.0f, 0.0f}});
    EXPECT_FALSE(world.IsOnGround(box)) << "held up in the air";

    RunSeconds(world, 0.3);
    EXPECT_FALSE(world.IsOnGround(box)) << "falling";

    RunSeconds(world, 2.0);
    const auto ground = world.GroundUnder(box);
    ASSERT_TRUE(ground.has_value());
    EXPECT_EQ(ground->body, floor);
    EXPECT_EQ(ground->userData, 7u);
    EXPECT_NEAR(ground->normal.y, 1.0f, 1e-3f);
    EXPECT_NEAR(ground->point.y, 0.0f, 0.02f);
    EXPECT_TRUE(world.IsOnGround(box));
    EXPECT_FALSE(world.IsOnGround(floor)) << "nothing is under the floor";

    world.SetVelocity(box, {0.0f, 5.0f, 0.0f}); // a jump
    RunSeconds(world, 0.1);
    EXPECT_FALSE(world.IsOnGround(box));
}

// Leaning on a wall is not standing on it; a slope is ground up to the steepest slope allowed.
TEST(PhysicsWorld, WallsAreNotGroundAndSlopesAreUpToALimit)
{
    PhysicsWorld world;
    world.SetGravity({0.0f, 0.0f, 0.0f}); // nothing falls: bodies are placed where they touch
    ASSERT_TRUE(world.Init());
    world.AddBody({.motion = MotionType::Static,
                   .position = {1.0f, 0.0f, 0.0f},
                   .collider = {.halfExtents = {0.5f, 5.0f, 5.0f}}}); // a wall whose face is at x = 0.5
    const BodyId leaning = world.AddBody({.position = {0.0f, 0.0f, 0.0f}});
    EXPECT_FALSE(world.IsOnGround(leaning));

    // A ramp tilted 30 degrees, and a box resting on its top face
    const glm::quat tilt = glm::angleAxis(glm::radians(30.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    const BodyId ramp = world.AddBody({.motion = MotionType::Static,
                                       .position = {0.0f, -20.0f, 0.0f},
                                       .rotation = tilt,
                                       .collider = {.halfExtents = {5.0f, 0.5f, 5.0f}}});
    const glm::vec3 onTop = glm::vec3(0.0f, -20.0f, 0.0f) + tilt * glm::vec3(0.0f, 1.0f, 0.0f);
    const BodyId resting = world.AddBody({.position = onTop, .rotation = tilt});

    const auto ground = world.GroundUnder(resting);
    ASSERT_TRUE(ground.has_value());
    EXPECT_EQ(ground->body, ramp);
    EXPECT_NEAR(ground->normal.x, -0.5f, 1e-3f);
    EXPECT_NEAR(ground->normal.y, std::sqrt(3.0f) / 2.0f, 1e-3f);
    EXPECT_TRUE(world.IsOnGround(resting, 35.0f));
    EXPECT_FALSE(world.IsOnGround(resting, 25.0f)) << "too steep";
}

// Up is against gravity: with the gravity upside down a body stands on the ceiling. With no gravity, up is +Y.
TEST(PhysicsWorld, GroundIsWhereGravityPulls)
{
    PhysicsWorld world;
    world.SetGravity({0.0f, 9.81f, 0.0f});
    ASSERT_TRUE(world.Init());
    const BodyId ceiling = world.AddBody({.motion = MotionType::Static,
                                          .position = {0.0f, 5.0f, 0.0f},
                                          .collider = {.halfExtents = {5.0f, 0.5f, 5.0f}}});
    const BodyId floor = AddFloorBody(world);
    const BodyId box = world.AddBody({.position = {0.0f, 3.0f, 0.0f}});

    RunSeconds(world, 2.0); // falls up
    const auto ground = world.GroundUnder(box);
    ASSERT_TRUE(ground.has_value());
    EXPECT_EQ(ground->body, ceiling);
    EXPECT_NEAR(ground->normal.y, -1.0f, 1e-3f);

    world.SetGravity({0.0f, 0.0f, 0.0f});
    world.SetPose(box, {.position = {0.0f, 0.5f, 0.0f}}); // on the floor
    const auto onFloor = world.GroundUnder(box);
    ASSERT_TRUE(onFloor.has_value());
    EXPECT_EQ(onFloor->body, floor);
}

// Only bodies that exist have ground under them.
TEST(PhysicsWorld, GroundNeedsABody)
{
    PhysicsWorld world;
    EXPECT_FALSE(world.IsOnGround({})) << "before Init";
    ASSERT_TRUE(world.Init());
    EXPECT_FALSE(world.IsOnGround({}));
    const BodyId box = world.AddBody({.position = {0.0f, 0.5f, 0.0f}});
    AddFloor(world);
    EXPECT_TRUE(world.IsOnGround(box));
    world.RemoveBody(box);
    EXPECT_FALSE(world.IsOnGround(box));
}

// ---------------------------------------------------------------- Shapes made of parts

namespace
{
    // A floor of two triangles, 10 x 10, at height 0
    std::vector<glm::vec3> FloorTriangles()
    {
        return {{-5.0f, 0.0f, -5.0f}, {-5.0f, 0.0f, 5.0f}, {5.0f, 0.0f, 5.0f},
                {-5.0f, 0.0f, -5.0f}, {5.0f, 0.0f, 5.0f},  {5.0f, 0.0f, -5.0f}};
    }

    Engine::Physics::BodyDesc Ball(const glm::vec3& at)
    {
        Engine::Physics::BodyDesc ball;
        ball.position = at;
        ball.collider.shape = Engine::Physics::ShapeType::Sphere;
        ball.collider.radius = 0.5f;
        return ball;
    }
}

TEST(PhysicsShapes, ABallLandsOnAStaticMesh)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    Engine::Physics::BodyDesc floor;
    floor.motion = Engine::Physics::MotionType::Static;
    floor.collider.shape = Engine::Physics::ShapeType::Mesh;
    floor.collider.triangles = FloorTriangles();
    ASSERT_TRUE(world.AddBody(floor).IsValid());
    const auto ball = world.AddBody(Ball({0.0f, 2.0f, 0.0f}));

    for (int i = 0; i < 180; ++i)
    {
        world.Update(PhysicsWorld::FixedStep);
    }

    EXPECT_NEAR(world.Pose(ball)->position.y, 0.5f, 0.05f) << "resting on the triangles";
}

TEST(PhysicsShapes, ADynamicMeshBecomesItsHullAndStillFalls)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    Engine::Physics::BodyDesc rock;
    rock.position = {0.0f, 10.0f, 0.0f};
    rock.collider.shape = Engine::Physics::ShapeType::Mesh;
    rock.collider.triangles = {{-1, -1, -1}, {1, -1, -1}, {0, 1, 0}, {-1, -1, 1}, {1, -1, 1}, {0, 1, 0}};
    const auto body = world.AddBody(rock);
    ASSERT_TRUE(body.IsValid());
    for (int i = 0; i < 30; ++i)
    {
        world.Update(PhysicsWorld::FixedStep);
    }
    EXPECT_LT(world.Pose(body)->position.y, 10.0f);
}

TEST(PhysicsShapes, ACompoundIsHitOnEachOfItsParts)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    Engine::Physics::Collider left;
    left.center = {-2.0f, 0.0f, 0.0f};
    Engine::Physics::Collider right;
    right.center = {2.0f, 0.0f, 0.0f};
    Engine::Physics::BodyDesc pair;
    pair.motion = Engine::Physics::MotionType::Static;
    pair.collider.shape = Engine::Physics::ShapeType::Compound;
    pair.collider.parts = {left, right};
    ASSERT_TRUE(world.AddBody(pair).IsValid());

    EXPECT_TRUE(world.CastRay({-2.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 10.0f).has_value());
    EXPECT_TRUE(world.CastRay({2.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 10.0f).has_value());
    EXPECT_FALSE(world.CastRay({0.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 10.0f).has_value()) << "nothing between them";
}

TEST(PhysicsShapes, OutlinesCoverMeshesAndCompounds)
{
    Engine::Physics::Collider mesh;
    mesh.shape = Engine::Physics::ShapeType::Mesh;
    mesh.triangles = FloorTriangles();
    EXPECT_EQ(Engine::Physics::OutlineOf(mesh, {}).size(), 6u) << "three edges each";

    Engine::Physics::Collider compound;
    compound.shape = Engine::Physics::ShapeType::Compound;
    compound.parts = {Engine::Physics::Collider{}, Engine::Physics::Collider{}};
    EXPECT_EQ(Engine::Physics::OutlineOf(compound, {}).size(), 24u) << "twelve edges for each box";
}

// ---------------------------------------------------------------- Layers, locks and local pushes

TEST(PhysicsLayers, ARayOnlySeesTheLayersItLooksAt)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    Engine::Physics::BodyDesc nearDesc;
    nearDesc.motion = Engine::Physics::MotionType::Static;
    nearDesc.position = {0.0f, 0.0f, -2.0f};
    nearDesc.layers = 2u;
    Engine::Physics::BodyDesc farDesc = nearDesc;
    farDesc.position = {0.0f, 0.0f, -6.0f};
    farDesc.layers = 4u;
    const auto nearBody = world.AddBody(nearDesc);
    const auto farBody = world.AddBody(farDesc);

    const glm::vec3 forward(0.0f, 0.0f, -1.0f);
    const glm::vec3 from(0.0f);
    EXPECT_EQ(world.CastRay(from, forward, 20.0f)->body, nearBody) << "every layer by default";
    EXPECT_EQ(world.CastRay(from, forward, 20.0f, {}, 4u)->body, farBody) << "the nearer one is on another layer";
    EXPECT_FALSE(world.CastRay(from, forward, 20.0f, {}, 8u).has_value());
    EXPECT_EQ(world.CastRay(from, forward, 20.0f, nearBody, 6u)->body, farBody) << "ignore still works with a mask";
}

TEST(PhysicsLocks, ALockedAxisDoesNotMove)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    Engine::Physics::BodyDesc desc;
    desc.position = {0.0f, 5.0f, 0.0f};
    desc.locks.moveY = true;
    const auto body = world.AddBody(desc);
    world.SetVelocity(body, {1.0f, 0.0f, 0.0f});

    for (int i = 0; i < 60; ++i)
    {
        world.Update(PhysicsWorld::FixedStep);
    }

    EXPECT_NEAR(world.Pose(body)->position.y, 5.0f, 0.001f) << "gravity cannot pull it down";
    EXPECT_GT(world.Pose(body)->position.x, 0.5f) << "the other axes are free";
}

TEST(PhysicsLocks, LockingEveryAxisIsRefusedAndLeftUnlocked)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    Engine::Physics::BodyDesc desc;
    desc.position = {0.0f, 5.0f, 0.0f};
    desc.locks = {true, true, true, true, true, true};
    const auto body = world.AddBody(desc);
    ASSERT_TRUE(body.IsValid());
    for (int i = 0; i < 30; ++i)
    {
        world.Update(PhysicsWorld::FixedStep);
    }
    EXPECT_LT(world.Pose(body)->position.y, 5.0f);
}

TEST(PhysicsForces, LocalPushesFollowTheTurnOfTheBody)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    world.SetGravity(glm::vec3(0.0f));
    Engine::Physics::BodyDesc desc;
    desc.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // its +X is the world -Z
    const auto body = world.AddBody(desc);

    world.AddLocalImpulse(body, {1.0f, 0.0f, 0.0f});

    const glm::vec3 velocity = *world.Velocity(body);
    EXPECT_NEAR(velocity.x, 0.0f, 0.001f);
    EXPECT_NEAR(velocity.z, -1.0f, 0.001f);
}

TEST(PhysicsPushes, ForcesAndImpulsesAreReported)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    Engine::Physics::BodyDesc desc;
    desc.position = {1.0f, 2.0f, 3.0f};
    const auto body = world.AddBody(desc);

    world.AddImpulse(body, {0.0f, 4.0f, 0.0f});
    world.SetConstantForce(body, {5.0f, 0.0f, 0.0f});

    const auto pushes = world.TakePushes();
    ASSERT_EQ(pushes.size(), 2u);
    EXPECT_EQ(pushes[0].amount, glm::vec3(0.0f, 4.0f, 0.0f));
    EXPECT_NEAR(pushes[0].point.y, 2.0f, 0.001f);
    EXPECT_EQ(pushes[1].amount, glm::vec3(5.0f, 0.0f, 0.0f));

    const auto next = world.TakePushes();
    ASSERT_EQ(next.size(), 1u) << "an impulse is told once, a steady force each time";
    EXPECT_EQ(next[0].amount, glm::vec3(5.0f, 0.0f, 0.0f));
}

TEST(PhysicsForces, ALocalForcePushesAlongTheAxesOfTheBody)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    Engine::Physics::BodyDesc desc;
    desc.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const auto body = world.AddBody(desc);

    world.SetLocalConstantForce(body, {5.0f, 0.0f, 0.0f});

    const auto pushes = world.TakePushes();
    ASSERT_EQ(pushes.size(), 1u);
    EXPECT_NEAR(pushes[0].amount.x, 0.0f, 0.001f);
    EXPECT_NEAR(pushes[0].amount.z, -5.0f, 0.001f);
}
