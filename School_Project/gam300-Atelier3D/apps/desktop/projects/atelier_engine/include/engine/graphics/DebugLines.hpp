/* !
@file    	    DebugLines.hpp
@author  	    zhihenghenrey.hu@digipen.edu [100%]
@date    		27/10/2026
@brief
    Declares DebugLines, a batch of coloured lines (boxes, spheres, capsules,
    rays) built over a frame and drawn in one go over the scene, and
    DebugLineSystem, which fills it from the scene (boxes around objects,
    colliders, rays).

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/game/Shape.hpp"
#include "engine/graphics/Mesh/Vertex.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Engine::Scene
{
    class Scene;
}

namespace Engine::Assets
{
    class ResourceManager;
}

namespace Engine::Graphics
{
    class Shader;

    // Lines collected over a frame and drawn in one go over the scene (they show through objects): the debugger's
    // bounds, colliders and rays. Building them needs no OpenGL, so they can be tested.
    class DebugLines
    {
    public:
        void Line(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color);
        // The 12 edges of a box with this centre and half size in its own space, placed by world
        void Box(const glm::mat4& world, const glm::vec3& center, const glm::vec3& halfSize, const glm::vec4& color);
        // A circle around each axis
        void Sphere(const glm::vec3& center, float radius, const glm::vec4& color);
        // A capsule standing along up: height from end to end, rounded ends included
        void Capsule(const glm::vec3& center, const glm::vec3& up, float radius, float height, const glm::vec4& color);
        // A line with a small head at its end
        void Arrow(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color);
        // A ray: its whole length in color when it hit nothing; otherwise color up to the hit point, then afterHit for
        // the rest of its length, with a cross in afterHit at the hit point
        void Ray(const glm::vec3& origin, const glm::vec3& direction, float length, const std::optional<glm::vec3>& hit,
                 const glm::vec4& color, const glm::vec4& afterHit);

        void Clear() { vertices_.clear(); }
        bool Empty() const { return vertices_.empty(); }
        // Two per line
        const std::vector<Vertex>& Vertices() const { return vertices_; }

        // Draws the lines with the standard shader's unlit path (their own colours), without the depth test
        void Draw(const Shader& standardShader, const glm::mat4& viewProjection) const;

    private:
        // Part of a circle in the plane of u and v (unit, at right angles), from one angle to another (radians)
        void Arc(const glm::vec3& center, const glm::vec3& u, const glm::vec3& v, float radius, float from, float to,
                 const glm::vec4& color);

        std::vector<Vertex> vertices_;
    };
}

namespace Engine::Systems
{
    struct ColliderOutline;

    // Which debug lines to draw (the editor's Debug menu switches them)
    struct DebugLineSettings
    {
        bool bounds = false;      // a box around everything that draws (white on dark objects, dark on light ones)
        bool colliders = false;   // each rigid body's collider: green static, yellow dynamic, blue kinematic
        bool rays = false;        // the rays the game cast this frame (and the frame before, while paused)
        bool contacts = false;    // where bodies just started touching, with the direction they push apart
        bool forces = false;      // the pushes on bodies, as arrows
        bool layerColors = false; // colliders coloured by collision layer instead of body type

        bool Any() const { return bounds || colliders || rays || contacts || forces; }
        bool operator==(const DebugLineSettings&) const = default;
    };

    // A ray the game cast, kept for the Raycasts debug lines (GameRuntime::CastRays)
    struct CastRay
    {
        glm::vec3 origin{0.0f};
        glm::vec3 direction{0.0f, 0.0f, -1.0f}; // unit length
        float length = 0.0f;                    // how far it looked
        std::optional<glm::vec3> hit;           // where it hit something, if it did
        std::string hitName;                    // the object it hit, if it is known
    };

    // Where two bodies started touching, kept for the Contacts debug lines
    struct ContactMark
    {
        glm::vec3 point{0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f}; // unit length
    };

    // A push on a body, kept for the Forces debug lines
    struct ForceMark
    {
        glm::vec3 point{0.0f};
        glm::vec3 amount{0.0f};
    };

    // Turns the scene into debug lines each frame (GameRuntime::Tick draws them after the scene)
    class DebugLineSystem
    {
    public:
        static inline const glm::vec4 kBoundsLightColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.9f);   // on dark objects
        static inline const glm::vec4 kBoundsDarkColor = glm::vec4(0.05f, 0.05f, 0.12f, 0.9f); // on light objects
        static inline const glm::vec4 kStaticColor = glm::vec4(0.35f, 0.82f, 0.47f, 1.0f);     // green
        static inline const glm::vec4 kDynamicColor = glm::vec4(0.98f, 0.82f, 0.25f, 1.0f);    // yellow
        static inline const glm::vec4 kKinematicColor = glm::vec4(0.35f, 0.63f, 1.0f, 1.0f);   // blue
        static constexpr float kAsleepAlpha = 0.43f;    // a resting body is faded
        static constexpr float kApartBrightness = 0.8f; // a body touching nothing is a little dimmer

        // The colour of the bounds box around an object tinted r, g, b (0 to 1): white on a dark object, near black on
        // a light one, so it shows up either way
        static glm::vec4 BoundsColorFor(float r, float g, float b);

        // The colour of a collider's lines: by body type, faded when asleep, a little dimmer when it touches nothing
        // (a line is one width, so touching cannot be thicker as it is in the Physics window)
        static glm::vec4 ColliderColorFor(const ColliderOutline& outline, bool byLayer = false);
        // A colour of its own for the lowest layer of layers
        static glm::vec4 LayerColor(std::uint32_t layers);

        static inline const glm::vec4 kRayColor = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f);    // red: a ray that hit nothing
        static inline const glm::vec4 kRayHitColor = glm::vec4(0.2f, 0.9f, 0.3f, 1.0f); // green: a ray that hit
        static inline const glm::vec4 kTouchingBoundsColor = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f); // red: touching
        static inline const glm::vec4 kContactColor = glm::vec4(1.0f, 0.6f, 0.1f, 1.0f);        // orange
        static constexpr float kContactMarkRadius = 0.06f;
        static constexpr float kContactNormalLength = 0.5f;
        static inline const glm::vec4 kForceColor = glm::vec4(0.9f, 0.3f, 1.0f, 1.0f); // violet
        static constexpr float kForceScale = 0.1f;                                     // units of arrow per newton
        static constexpr float kMinForceLength = 0.3f;
        static constexpr float kMaxForceLength = 3.0f;
        static constexpr std::size_t kMaxCastRays = 100;  // more in one frame are not kept
        static constexpr float kGroundProbeLength = 1.0f; // how long a ground check that found nothing is drawn

        // Adds a ray to a list of them, unless the list is already at kMaxCastRays
        static void AddRay(std::vector<CastRay>& rays, const CastRay& ray);
        // A ground check as a ray: straight along gravity (down if there is none) from origin, to the height of the
        // ground point it found, or kGroundProbeLength long if it found none
        static CastRay GroundRay(const glm::vec3& origin, const glm::vec3& gravity,
                                 const std::optional<glm::vec3>& groundPoint);

        // Adds the lines settings asks for, for every entity. colliders are the outlines to draw when
        // settings.colliders is on (GameRuntime::ColliderOutlines()), rays the ones to draw when settings.rays is on
        // (GameRuntime::CastRays()).
        static void Collect(Scene::Scene& scene, Assets::ResourceManager& resources, const DebugLineSettings& settings,
                            const std::vector<ColliderOutline>& colliders, const std::vector<CastRay>& rays,
                            Graphics::DebugLines& lines, const std::vector<ContactMark>& contacts = {},
                            const std::vector<ForceMark>& forces = {});
        // The same without colliders or rays
        static void Collect(Scene::Scene& scene, Assets::ResourceManager& resources, const DebugLineSettings& settings,
                            Graphics::DebugLines& lines);
        // A built-in shape's box at scale 1, as the renderer draws it: lowest and highest corner
        static std::pair<glm::vec3, glm::vec3> ShapeBounds(Game::Shape shape);
    };
}
