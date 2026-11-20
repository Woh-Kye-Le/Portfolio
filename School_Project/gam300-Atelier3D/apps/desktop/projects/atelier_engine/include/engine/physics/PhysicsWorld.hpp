/* !
@file    	    PhysicsWorld.hpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		26/10/2026
@brief
    Declares PhysicsWorld, the engine's wrapper around Jolt Physics. It owns a
    Jolt physics system with its worker threads, adds and removes bodies,
    steps the simulation at a fixed rate, whatever the frame rate is, and
    reports which bodies start and stop touching. Game code can push bodies
    with impulses, steady forces and velocities, cast rays to find what is in
    a direction, measure how far apart bodies are, and tell whether a body
    stands on the ground.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/physics/Body.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Engine::Physics
{
    // One physics simulation: the bodies of one running scene, stepped by Update() at a fixed rate.
    //
    // Pimpl'd so Jolt's headers stay inside PhysicsWorld.cpp: nothing that includes this header needs Jolt's include
    // paths, compile definitions or instruction-set flags.
    class PhysicsWorld
    {
    public:
        // Length of one simulation step in seconds. Physics always steps by this amount, so a fast or slow frame rate
        // does not change how objects move.
        static constexpr double FixedStep = 1.0 / 60.0;

        // Most steps one Update() runs. A very long frame (a breakpoint, loading a scene) would otherwise make the
        // next frame run hundreds of steps and take even longer.
        static constexpr int MaxStepsPerUpdate = 4;

        // Most bodies the simulation can hold
        static constexpr std::uint32_t MaxBodies = 10240;

        PhysicsWorld();
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;

        /*!
        \brief   Creates the Jolt physics system and its worker threads. Does nothing if the world is already running.
        \return  True if the world is running. False (logged) if Jolt could not start.
        */
        bool Init();

        /*!
        \brief   Destroys every body and stops the worker threads. Safe to call twice, or without Init().
        */
        void Shutdown();

        /*!
        \brief   Whether Init() succeeded and Shutdown() has not been called since.
        \return  True if the world is running.
        */
        bool IsInitialized() const;

        /*!
        \brief   Advances the simulation by the frame's time, in steps of FixedStep. Time left over that is shorter than
                 one step is carried to the next call. Does nothing before Init().
        \param   deltaSeconds The time since the last call, in seconds.
        \return  The number of steps run: 0 when less than one step of time has built up, at most MaxStepsPerUpdate.
        */
        int Update(double deltaSeconds);

        /*!
        \brief   Sets the acceleration applied to every moving body. Can be set before Init(); it is kept across
                 Shutdown() and Init().
        \param   gravity The acceleration in units per second squared. The default is (0, -9.81, 0): Earth's gravity,
                 pulling down the Y axis.
        */
        void SetGravity(const glm::vec3& gravity);

        /*!
        \brief   The acceleration applied to every moving body.
        \return  The gravity in units per second squared.
        */
        glm::vec3 Gravity() const;

        /*!
        \brief   The number of bodies in the simulation.
        \return  The body count; 0 before Init().
        */
        std::uint32_t BodyCount() const;

        /*!
        \brief   The number of steps run since Init().
        \return  The step count; 0 before Init().
        */
        std::uint64_t StepCount() const;

        /*!
        \brief   Makes a body and adds it to the simulation. A dynamic body starts moving on the next step.
        \param   desc How it moves, where it is, its collider and its mass.
        \return  The new body, or an invalid id (logged) before Init() or when MaxBodies is reached.
        */
        BodyId AddBody(const BodyDesc& desc);

        /*!
        \brief   Takes a body out of the simulation and destroys it. Does nothing for an invalid or removed id.
        \param   body The body to remove.
        */
        void RemoveBody(BodyId body);

        /*!
        \brief   Whether the body exists in this world.
        \param   body The body to look for.
        \return  False for an invalid id, a removed body, or before Init().
        */
        bool HasBody(BodyId body) const;

        /*!
        \brief   Where a body is and how it is turned.
        \param   body The body to look at.
        \return  Its pose, or nullopt if the body does not exist.
        */
        std::optional<BodyPose> Pose(BodyId body) const;

        /*!
        \brief   Whether a body is being simulated. A body that has been still for a moment goes to sleep until
                 something touches or moves it, and static bodies never are.
        \param   body The body to look at.
        \return  True for a moving or kinematic body that is awake; false if asleep, static or not there.
        */
        bool IsAwake(BodyId body) const;

        /*!
        \brief   Puts a body somewhere else straight away (not by moving it there), keeping its velocity. Does nothing
                 for a body that does not exist.
        \param   body The body to move.
        \param   pose Where to put it.
        */
        void SetPose(BodyId body, const BodyPose& pose);

        /*!
        \brief   Changes how a body's surface behaves, straight away (a sleeping body is woken so it reacts). Does
                 nothing for a body that does not exist.
        \param   body The body to change.
        \param   material Its friction (raised to 0 if below) and bounciness (kept within 0 to 1).
        */
        void SetMaterial(BodyId body, const Material& material);

        /*!
        \brief   A body's surface.
        \param   body The body to look at.
        \return  Its friction and bounciness, or nullopt if the body does not exist.
        */
        std::optional<Material> MaterialOf(BodyId body) const;

        // ---- Pushing bodies. Only dynamic bodies are pushed: static ones never move, and kinematic ones go where
        //      their Transform is. A sleeping body is woken.

        /*!
        \brief   Gives a body a one-off kick through its centre (a hit, a jump): its velocity changes straight away by
                 impulse / mass, so a heavy body moves less from the same kick.
        \param   body The body to kick.
        \param   impulse The kick in kilogram units per second, pointing the way it pushes.
        */
        void AddImpulse(BodyId body, const glm::vec3& impulse);

        /*!
        \brief   AddImpulse, with the impulse given in the body's own axes: "forward" stays forward as it turns.
        */
        void AddLocalImpulse(BodyId body, const glm::vec3& impulse);

        /*!
        \brief   Gives a body a one-off kick at a point: off its centre it also starts the body spinning.
        \param   body The body to kick.
        \param   impulse The kick in kilogram units per second.
        \param   point Where it is kicked, in world space.
        */
        void AddImpulseAt(BodyId body, const glm::vec3& impulse, const glm::vec3& point);

        /*!
        \brief   Pushes a body steadily through its centre (wind, a thruster) every step until changed; zero stops it.
                 It accelerates by force / mass, like gravity does. Removing the body forgets it.
        \param   body The body to push.
        \param   force The push in newtons (kilogram units per second squared).
        */
        void SetConstantForce(BodyId body, const glm::vec3& force);

        /*!
        \brief   SetConstantForce, with the force given in the body's own axes as it is now.
        */
        void SetLocalConstantForce(BodyId body, const glm::vec3& force);

        /*!
        \brief   For the debug view: every steady force as it is now, and the impulses given since the last call.
        */
        std::vector<Push> TakePushes();

        /*!
        \brief   The steady push set with SetConstantForce.
        \param   body The body to look at.
        \return  The force, zero if there is none or the body does not exist.
        */
        glm::vec3 ConstantForceOf(BodyId body) const;

        /*!
        \brief   Sets how fast and which way a body moves, straight away.
        \param   body The body to set.
        \param   velocity In units per second.
        */
        void SetVelocity(BodyId body, const glm::vec3& velocity);

        /*!
        \brief   How fast and which way a body moves.
        \param   body The body to look at.
        \return  Its velocity in units per second (zero for a static body), or nullopt if it does not exist.
        */
        std::optional<glm::vec3> Velocity(BodyId body) const;

        /*!
        \brief   Sets how fast a body spins, straight away.
        \param   body The body to set.
        \param   velocity The axis it spins around, as long as its speed in radians per second.
        */
        void SetAngularVelocity(BodyId body, const glm::vec3& velocity);

        /*!
        \brief   How fast a body spins.
        \param   body The body to look at.
        \return  The axis it spins around, as long as its speed in radians per second; nullopt if it does not exist.
        */
        std::optional<glm::vec3> AngularVelocity(BodyId body) const;

        /*!
        \brief   The contacts that began or ended since the last call, oldest first, and forgets them. Bodies touching
                 at the end of a step count: a body that only grazes another between two steps is not reported. A
                 resting body that goes to sleep keeps touching what it lies on. Removing a body ends its contacts
                 straight away. Empty before Init().
        \return  The events, each pair of bodies at most once per kind and step.
        */
        std::vector<ContactEvent> TakeContactEvents();

        /*!
        \brief   The bodies touching a body now.
        \param   body The body to look at.
        \return  The bodies it touches, empty if none or if it does not exist.
        */
        std::vector<BodyId> Touching(BodyId body) const;

        /*!
        \brief   Whether two bodies touch now.
        \param   a One body.
        \param   b The other body; the order does not matter.
        \return  True if they touch.
        */
        bool AreTouching(BodyId a, BodyId b) const;

        /*!
        \brief   The number of pairs of bodies touching now.
        \return  The pair count; 0 before Init().
        */
        std::size_t ContactCount() const;

        /*!
        \brief   Casts a ray and finds the first body it hits: what is in front of an enemy, whether there is ground
                 under a player, what the mouse points at. Bodies are where the last step left them, or where SetPose
                 put them since. A ray that starts inside a body hits it at distance 0.
        \param   origin Where the ray starts, in world space.
        \param   direction Which way it goes, of any length but zero.
        \param   maxDistance How far it reaches, in units.
        \param   ignore A body the ray passes through, usually the caster's own; none by default.
        \return  The nearest hit, or nullopt if nothing is hit within maxDistance. Also nullopt before Init(), for a
                 zero direction, a distance not above 0, or values that are not finite.
        */
        /*!
        \brief   How far a body can walk along the ground: its shape is swept along the move, stops at a static wall and
                 slides along it. Pushed into a wall step after step, a fast body would otherwise be forced through.
                 Moving bodies and what it stands on or can climb (a slope) do not stop it: the simulation deals with
                 those. The up and down part of the move is left as it is.
        \param   body   The body that walks.
        \param   offset How far it wants to go this step, in units.
        \return  How far it can go; offset itself for a body that does not exist.
        */
        glm::vec3 ClipWalk(BodyId body, const glm::vec3& offset) const;

        std::optional<RayHit> CastRay(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                                      BodyId ignore = {}, std::uint32_t layerMask = kAllLayers) const;

        /*!
        \brief   How far apart two bodies are: the gap between the nearest points of their colliders, not their
                 centres, so a big wall is close as soon as its surface is. Bodies are where the last step left them,
                 or where SetPose put them since.
        \param   a One body.
        \param   b The other.
        \param   maxDistance Bodies farther apart than this are out of range.
        \return  The gap, 0 when they touch or overlap; nullopt when they are out of range, either body does not
                 exist, a and b are the same body, maxDistance is not finite, or before Init().
        */
        std::optional<Gap> DistanceBetween(BodyId a, BodyId b, float maxDistance = DefaultMaxGap) const;

        /*!
        \brief   The bodies whose colliders reach within a distance of a point: what is in range of an explosion, an
                 enemy's sight, a pickup's magnet.
        \param   point The point, in world space.
        \param   radius How far from it, in units; above 0.
        \param   ignore A body to leave out, usually the one asking; none by default.
        \return  The bodies in range, nearest first (ties by id); empty when there are none, radius is not above 0,
                 a value is not finite, or before Init().
        */
        std::vector<NearbyBody> BodiesWithin(const glm::vec3& point, float radius, BodyId ignore = {}) const;

        /*!
        \brief   What a body stands on: another body its collider rests on (within GroundTolerance) whose surface
                 there faces up, or is no steeper than maxSlope. Up is against gravity (+Y when there is none), so
                 with the gravity turned upside down a body stands on the ceiling. A wall it leans on is not ground.
                 Bodies are where the last step left them, or where SetPose put them since.
        \param   body The body to look under, usually a dynamic one (a player, a crate).
        \param   maxSlope The steepest surface that counts as ground, in degrees from flat (0 to 90).
        \return  The ground, the flattest of them when it stands on several; nullopt when it stands on nothing
                 (falling, jumping, or only touching walls), the body does not exist, or before Init().
        */
        std::optional<Ground> GroundUnder(BodyId body, float maxSlope = DefaultMaxSlope) const;

        /*!
        \brief   Whether a body stands on the ground: see GroundUnder.
        \param   body The body to look under.
        \param   maxSlope The steepest surface that counts as ground, in degrees from flat.
        \return  True if it stands on something.
        */
        bool IsOnGround(BodyId body, float maxSlope = DefaultMaxSlope) const;

        /*!
        \brief   The Jolt version the engine was built with, for the log and the editor.
        \return  For example "Jolt Physics 5.6.0".
        */
        static std::string LibraryVersion();

    private:
        struct Impl; // the Jolt objects, defined in PhysicsWorld.cpp
        std::unique_ptr<Impl> impl_;

        glm::vec3 gravity_{0.0f, -9.81f, 0.0f};
        double accumulator_ = 0.0; // time not simulated yet, always under one step after Update()
        std::uint64_t steps_ = 0;
        bool warnedUpdateError_ = false; // a full cache is logged once, not every step
        bool warnedFull_ = false;        // so is running out of bodies
    };
}
