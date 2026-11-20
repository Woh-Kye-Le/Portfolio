/* !
@file    	    DebugLines.cpp
@author  	    zhihenghenrey.hu@digipen.edu [100%]
@date    		27/10/2026
@brief
    Implements DebugLines: the line shapes, and drawing the batch with the
    standard shader's unlit path; and DebugLineSystem: a box that turns and
    scales with each object that draws, fitted to its built-in shape or
    imported mesh.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/graphics/DebugLines.hpp"

#include "engine/assets/ResourceManager.hpp"
#include "engine/graphics/Mesh/Mesh.hpp"
#include "engine/graphics/Renderer/RenderAPI.hpp"
#include "engine/graphics/Shader/Shader.hpp"
#include "engine/scene/Components.hpp"
#include "engine/scene/Scene.hpp"
#include "engine/scene/systems/PhysicsSystem.hpp"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>

namespace Engine::Graphics
{
    namespace
    {
        constexpr int kCircleSegments = 32;
        constexpr float kCrossSize = 0.1f; // half the width of the cross on a ray's hit point
    }

    void DebugLines::Line(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color)
    {
        vertices_.push_back({.position = from, .color = color});
        vertices_.push_back({.position = to, .color = color});
    }

    void DebugLines::Box(const glm::mat4& world, const glm::vec3& center, const glm::vec3& halfSize,
                         const glm::vec4& color)
    {
        // Corner i is on the + side of x, y, z where bit 1, 2, 4 of i is set
        std::array<glm::vec3, 8> corners;
        for (std::size_t i = 0; i < corners.size(); ++i)
        {
            const glm::vec3 side((i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f);
            corners[i] = glm::vec3(world * glm::vec4(center + halfSize * side, 1.0f));
        }
        // An edge joins two corners that differ on one axis
        for (std::size_t i = 0; i < corners.size(); ++i)
        {
            for (const std::size_t axis : {1u, 2u, 4u})
            {
                if ((i & axis) == 0)
                {
                    Line(corners[i], corners[i | axis], color);
                }
            }
        }
    }

    void DebugLines::Sphere(const glm::vec3& center, float radius, const glm::vec4& color)
    {
        const glm::vec3 x(1.0f, 0.0f, 0.0f);
        const glm::vec3 y(0.0f, 1.0f, 0.0f);
        const glm::vec3 z(0.0f, 0.0f, 1.0f);
        Arc(center, x, y, radius, 0.0f, glm::two_pi<float>(), color);
        Arc(center, y, z, radius, 0.0f, glm::two_pi<float>(), color);
        Arc(center, z, x, radius, 0.0f, glm::two_pi<float>(), color);
    }

    void DebugLines::Capsule(const glm::vec3& center, const glm::vec3& up, float radius, float height,
                             const glm::vec4& color)
    {
        const glm::vec3 y = glm::normalize(up);
        const glm::vec3 other = std::abs(y.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 x = glm::normalize(glm::cross(y, other));
        const glm::vec3 z = glm::cross(x, y);
        const float straight = std::max(height * 0.5f - radius, 0.0f); // from the centre to each end's centre
        const glm::vec3 top = center + y * straight;
        const glm::vec3 bottom = center - y * straight;
        Arc(top, x, z, radius, 0.0f, glm::two_pi<float>(), color);
        Arc(bottom, x, z, radius, 0.0f, glm::two_pi<float>(), color);
        for (const glm::vec3& side : {x, -x, z, -z})
        {
            Line(bottom + side * radius, top + side * radius, color);
        }
        // The rounded ends: half circles over the top and under the bottom
        Arc(top, x, y, radius, 0.0f, glm::pi<float>(), color);
        Arc(top, z, y, radius, 0.0f, glm::pi<float>(), color);
        Arc(bottom, x, y, radius, glm::pi<float>(), glm::two_pi<float>(), color);
        Arc(bottom, z, y, radius, glm::pi<float>(), glm::two_pi<float>(), color);
    }

    void DebugLines::Arrow(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color)
    {
        Line(from, to, color);
        const glm::vec3 along = to - from;
        const float length = glm::length(along);
        if (length <= 0.0f)
        {
            return;
        }
        constexpr float kHeadShare = 0.25f; // of its length
        const glm::vec3 unit = along / length;
        const glm::vec3 other = std::abs(unit.y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 side = glm::normalize(glm::cross(unit, other)) * (length * kHeadShare * 0.5f);
        const glm::vec3 base = to - unit * (length * kHeadShare);
        Line(to, base + side, color);
        Line(to, base - side, color);
    }

    void DebugLines::Ray(const glm::vec3& origin, const glm::vec3& direction, float length,
                         const std::optional<glm::vec3>& hit, const glm::vec4& color, const glm::vec4& afterHit)
    {
        const bool aimed = glm::length(direction) > 0.0f;
        const glm::vec3 end = aimed ? origin + glm::normalize(direction) * length : origin;
        if (!hit)
        {
            if (aimed)
            {
                Line(origin, end, color);
            }
            return;
        }
        Line(origin, *hit, color);
        if (aimed && glm::distance(origin, *hit) + kCrossSize < length)
        {
            Line(*hit, end, afterHit); // nothing past a hit at the very end of the ray
        }
        for (const glm::vec3& axis :
             {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)})
        {
            Line(*hit - axis * kCrossSize, *hit + axis * kCrossSize, afterHit);
        }
    }

    void DebugLines::Arc(const glm::vec3& center, const glm::vec3& u, const glm::vec3& v, float radius, float from,
                         float to, const glm::vec4& color)
    {
        const int segments = std::max(
            1, static_cast<int>(std::ceil(static_cast<float>(kCircleSegments) * (to - from) / glm::two_pi<float>())));
        glm::vec3 previous = center + radius * (std::cos(from) * u + std::sin(from) * v);
        for (int i = 1; i <= segments; ++i)
        {
            const float angle = from + (to - from) * static_cast<float>(i) / static_cast<float>(segments);
            const glm::vec3 point = center + radius * (std::cos(angle) * u + std::sin(angle) * v);
            Line(previous, point, color);
            previous = point;
        }
    }

    void DebugLines::Draw(const Shader& standardShader, const glm::mat4& viewProjection) const
    {
        if (vertices_.empty())
        {
            return;
        }
        std::vector<std::uint32_t> indices(vertices_.size());
        std::iota(indices.begin(), indices.end(), 0u);
        const Mesh mesh(vertices_, std::move(indices), "DebugLines"); // uploaded on Bind, freed at the end of Draw

        standardShader.Bind();
        standardShader.SetInt("u_Lit", 0); // the grid's path: vertex colours, no lighting
        standardShader.SetInt("u_Textured", 0);
        standardShader.SetMat4("u_ViewProjection", viewProjection);
        standardShader.SetMat4("u_Model", glm::mat4(1.0f));
        standardShader.SetMat3("u_NormalMatrix", glm::mat3(1.0f));
        standardShader.SetFloat4("u_AlbedoTint", glm::vec4(1.0f));
        RenderAPI::EnableDepthTest(false); // seen through the objects they belong to
        RenderAPI::EnableBlend(true);      // the 3D pass draws with blending off, and a resting body's lines are faded
        mesh.Bind();
        RenderAPI::DrawIndexed(mesh.GetIndexCount(), PrimitiveType::Lines);
        mesh.Unbind();
        RenderAPI::EnableBlend(false);
        RenderAPI::EnableDepthTest(true);
        standardShader.Unbind();
    }
}

namespace Engine::Systems
{
    namespace
    {
        using Bounds = std::pair<glm::vec3, glm::vec3>;

        Bounds Nothing()
        {
            return {glm::vec3(std::numeric_limits<float>::max()), glm::vec3(std::numeric_limits<float>::lowest())};
        }

        Bounds Around(const std::vector<Graphics::Vertex>& vertices)
        {
            Bounds bounds = Nothing();
            for (const Graphics::Vertex& vertex : vertices)
            {
                bounds.first = glm::min(bounds.first, vertex.position);
                bounds.second = glm::max(bounds.second, vertex.position);
            }
            return bounds;
        }

        // An imported mesh's points: x, y, z per vertex
        Bounds Around(const std::vector<float>& positions)
        {
            Bounds bounds = Nothing();
            for (std::size_t i = 0; i + 2 < positions.size(); i += 3)
            {
                const glm::vec3 point(positions[i], positions[i + 1], positions[i + 2]);
                bounds.first = glm::min(bounds.first, point);
                bounds.second = glm::max(bounds.second, point);
            }
            return bounds;
        }
    }

    std::pair<glm::vec3, glm::vec3> DebugLineSystem::ShapeBounds(Game::Shape shape)
    {
        // The meshes Renderer::Init makes, measured once (keep these sizes the same as there)
        static const std::array<Bounds, 8> kShapes = []
        {
            Bounds plane = Around(Graphics::Mesh::CreatePlane(10.0f, 1).GetVertices());
            const glm::vec3 tenth(0.1f, 1.0f, 0.1f); // DrawPrimitive draws the plane at a tenth: 1 x 1
            plane = {plane.first * tenth, plane.second * tenth};
            // In Game::Shape's order
            return std::array<Bounds, 8>{
                Around(Graphics::Mesh::CreateQuad(1.0f, 1.0f).GetVertices()),
                Around(Graphics::Mesh::CreateCircle(1.0f, 48).GetVertices()),
                Around(Graphics::Mesh::CreateCube(1.0f).GetVertices()),
                Around(Graphics::Mesh::CreatePyramid(0.7f, -0.6f, 0.6f).GetVertices()),
                plane,
                Around(Graphics::Mesh::CreateWirebox(1.0f).GetVertices()),
                Around(Graphics::Mesh::CreateSphere(1.0f, 16, 32).GetVertices()),
                Around(Graphics::Mesh::CreateLine(glm::vec3(-0.5f, 0.0f, 0.0f), glm::vec3(0.5f, 0.0f, 0.0f))
                           .GetVertices())};
        }();
        return kShapes[static_cast<std::size_t>(shape)];
    }

    glm::vec4 DebugLineSystem::BoundsColorFor(float r, float g, float b)
    {
        constexpr float kLightObject = 0.5f;
        const float brightness = 0.2126f * r + 0.7152f * g + 0.0722f * b; // how bright the eye sees the tint
        return brightness >= kLightObject ? kBoundsDarkColor : kBoundsLightColor;
    }

    glm::vec4 DebugLineSystem::LayerColor(std::uint32_t layers)
    {
        static constexpr std::array<glm::vec4, 6> kColors{
            glm::vec4(0.35f, 0.82f, 0.47f, 1.0f), glm::vec4(0.35f, 0.63f, 1.0f, 1.0f),
            glm::vec4(0.98f, 0.82f, 0.25f, 1.0f), glm::vec4(0.9f, 0.3f, 1.0f, 1.0f),
            glm::vec4(1.0f, 0.5f, 0.2f, 1.0f),    glm::vec4(0.3f, 0.9f, 0.9f, 1.0f)};
        std::size_t lowest = 0;
        while (lowest < 31 && (layers & (1u << lowest)) == 0)
        {
            ++lowest;
        }
        return kColors[lowest % kColors.size()];
    }

    glm::vec4 DebugLineSystem::ColliderColorFor(const ColliderOutline& outline, bool byLayer)
    {
        glm::vec4 color = kDynamicColor;
        switch (outline.type)
        {
        case Scene::BodyType::Static: color = kStaticColor; break;
        case Scene::BodyType::Kinematic: color = kKinematicColor; break;
        case Scene::BodyType::Dynamic: break;
        }
        if (byLayer)
        {
            color = LayerColor(outline.layers);
        }
        if (!outline.touching)
        {
            color = glm::vec4(glm::vec3(color) * kApartBrightness, color.a);
        }
        if (outline.asleep)
        {
            color.a = kAsleepAlpha;
        }
        return color;
    }

    void DebugLineSystem::AddRay(std::vector<CastRay>& rays, const CastRay& ray)
    {
        if (rays.size() < kMaxCastRays)
        {
            rays.push_back(ray);
        }
    }

    CastRay DebugLineSystem::GroundRay(const glm::vec3& origin, const glm::vec3& gravity,
                                       const std::optional<glm::vec3>& groundPoint)
    {
        const glm::vec3 down = glm::length(gravity) > 0.0f ? glm::normalize(gravity) : glm::vec3(0.0f, -1.0f, 0.0f);
        if (!groundPoint)
        {
            return {origin, down, kGroundProbeLength, std::nullopt};
        }
        // Where the object touches may be off to one side of its centre: only how far down it is counts
        const float length = std::max(glm::dot(*groundPoint - origin, down), 0.0f);
        return {origin, down, length, origin + down * length};
    }

    void DebugLineSystem::Collect(Scene::Scene& scene, Assets::ResourceManager& resources,
                                  const DebugLineSettings& settings, Graphics::DebugLines& lines)
    {
        Collect(scene, resources, settings, {}, {}, lines);
    }

    void DebugLineSystem::Collect(Scene::Scene& scene, Assets::ResourceManager& resources,
                                  const DebugLineSettings& settings, const std::vector<ColliderOutline>& colliders,
                                  const std::vector<CastRay>& rays, Graphics::DebugLines& lines,
                                  const std::vector<ContactMark>& contacts, const std::vector<ForceMark>& forces)
    {
        if (settings.forces)
        {
            for (const ForceMark& force : forces)
            {
                const float size = glm::length(force.amount);
                if (size > 0.0f)
                {
                    const float length = std::clamp(size * kForceScale, kMinForceLength, kMaxForceLength);
                    lines.Arrow(force.point, force.point + force.amount / size * length, kForceColor);
                }
            }
        }
        if (settings.contacts)
        {
            for (const ContactMark& contact : contacts)
            {
                lines.Sphere(contact.point, kContactMarkRadius, kContactColor);
                lines.Arrow(contact.point, contact.point + contact.normal * kContactNormalLength, kContactColor);
            }
        }
        if (settings.bounds)
        {
            scene.ForEach<const Scene::TransformComponent, const Scene::MeshRendererComponent>(
                [&](Scene::Entity entity, const Scene::TransformComponent& transform,
                    const Scene::MeshRendererComponent& renderer)
                {
                    if (!renderer.visible)
                    {
                        return;
                    }
                    const Assets::Mesh* mesh = renderer.mesh.IsValid() ? resources.Get(renderer.mesh) : nullptr;
                    const Bounds bounds =
                        mesh && !mesh->positions.empty() ? Around(mesh->positions) : ShapeBounds(renderer.shape);
                    // A box that turns and scales with the object, so it hugs what it draws
                    const bool touching =
                        std::any_of(colliders.begin(), colliders.end(), [entity](const ColliderOutline& each)
                                    { return each.touching && each.entity == entity; });
                    lines.Box(Scene::ToGraphicsTransform(transform).ComposeModelMatrix(),
                              (bounds.first + bounds.second) * 0.5f, (bounds.second - bounds.first) * 0.5f,
                              touching ? kTouchingBoundsColor
                                       : BoundsColorFor(renderer.albedoR, renderer.albedoG, renderer.albedoB));
                });
        }
        if (settings.colliders)
        {
            // The outlines are already in world space (where the physics has each body in Play mode)
            for (const ColliderOutline& outline : colliders)
            {
                const glm::vec4 color = ColliderColorFor(outline, settings.layerColors);
                for (const Physics::Line& line : outline.lines)
                {
                    lines.Line(line.from, line.to, color);
                }
            }
        }
        if (settings.rays)
        {
            // Green with a cross at the hit; all red when it hit nothing
            for (const CastRay& ray : rays)
            {
                lines.Ray(ray.origin, ray.direction, ray.length, ray.hit, ray.hit ? kRayHitColor : kRayColor,
                          kRayHitColor);
            }
        }
    }
}
