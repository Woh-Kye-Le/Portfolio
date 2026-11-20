/* !
@file    	    ColliderLines.cpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		28/10/2026
@brief
    Implements OutlineOf: the line segments of a box, sphere or capsule
    collider's wireframe, in the body's frame and then moved to its pose.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/physics/ColliderLines.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace Engine::Physics
{
    namespace
    {
        // Below this a capsule's straight middle is too short to matter, and the physics makes a sphere instead (see
        // MakeShape in PhysicsWorld.cpp)
        constexpr float kMinHalfHeight = 0.001f;

        // Collects segments in the body's frame and moves them to the pose
        class Outline
        {
        public:
            Outline(const BodyPose& pose, const glm::vec3& center) : pose_(pose), center_(center) {}

            void Add(const glm::vec3& from, const glm::vec3& to)
            {
                lines_.push_back({.from = ToWorld(from), .to = ToWorld(to)});
            }

            // An arc around centre in the plane of the unit axes u and v, from angle start through sweep (radians)
            void Arc(const glm::vec3& centre, const glm::vec3& u, const glm::vec3& v, float radius, float start,
                     float sweep, int segments)
            {
                const auto at = [&](int i)
                {
                    const float angle = start + sweep * static_cast<float>(i) / static_cast<float>(segments);
                    return centre + radius * (std::cos(angle) * u + std::sin(angle) * v);
                };
                for (int i = 0; i < segments; ++i)
                {
                    Add(at(i), at(i + 1));
                }
            }

            std::vector<Line> Take() { return std::move(lines_); }

        private:
            glm::vec3 ToWorld(const glm::vec3& local) const
            {
                return pose_.position + pose_.rotation * (center_ + local);
            }

            BodyPose pose_;
            glm::vec3 center_;
            std::vector<Line> lines_;
        };

        constexpr glm::vec3 kX{1.0f, 0.0f, 0.0f};
        constexpr glm::vec3 kY{0.0f, 1.0f, 0.0f};
        constexpr glm::vec3 kZ{0.0f, 0.0f, 1.0f};

        void AddSphere(Outline& outline, float radius, int segments)
        {
            constexpr float kTurn = glm::two_pi<float>();
            outline.Arc(glm::vec3(0.0f), kX, kY, radius, 0.0f, kTurn, segments);
            outline.Arc(glm::vec3(0.0f), kY, kZ, radius, 0.0f, kTurn, segments);
            outline.Arc(glm::vec3(0.0f), kZ, kX, radius, 0.0f, kTurn, segments);
        }

        void AddBox(Outline& outline, const glm::vec3& half)
        {
            // Corner i has bit 0 for +X, bit 1 for +Y, bit 2 for +Z; an edge joins two corners one bit apart
            const auto corner = [&half](int i)
            { return glm::vec3((i & 1) ? half.x : -half.x, (i & 2) ? half.y : -half.y, (i & 4) ? half.z : -half.z); };
            for (int a = 0; a < 8; ++a)
            {
                for (const int bit : {1, 2, 4})
                {
                    if ((a & bit) == 0)
                    {
                        outline.Add(corner(a), corner(a | bit));
                    }
                }
            }
        }

        void AddCapsule(Outline& outline, float radius, float halfHeight, int segments)
        {
            constexpr float kTurn = glm::two_pi<float>();
            constexpr float kHalfTurn = glm::pi<float>();
            const glm::vec3 top(0.0f, halfHeight, 0.0f);
            const glm::vec3 bottom(0.0f, -halfHeight, 0.0f);

            // A ring where each rounded end meets the middle, and the sides between them
            outline.Arc(top, kZ, kX, radius, 0.0f, kTurn, segments);
            outline.Arc(bottom, kZ, kX, radius, 0.0f, kTurn, segments);
            for (const glm::vec3& side : {radius * kX, -radius * kX, radius * kZ, -radius * kZ})
            {
                outline.Add(bottom + side, top + side);
            }

            // Half circles over each end, seen from the front (X) and the side (Z)
            const int half = std::max(segments / 2, 4);
            for (const glm::vec3& across : {kX, kZ})
            {
                outline.Arc(top, across, kY, radius, 0.0f, kHalfTurn, half);
                outline.Arc(bottom, across, -kY, radius, 0.0f, kHalfTurn, half);
            }
        }
    }

    std::vector<Line> OutlineOf(const Collider& collider, const BodyPose& pose, int segments)
    {
        segments = std::max(segments, 8);
        Outline outline(pose, collider.center);
        switch (collider.shape)
        {
        case ShapeType::Box: AddBox(outline, collider.halfExtents); break;
        case ShapeType::Sphere: AddSphere(outline, collider.radius, segments); break;
        case ShapeType::Capsule:
            if (collider.halfHeight < kMinHalfHeight)
            {
                AddSphere(outline, collider.radius, segments);
            }
            else
            {
                AddCapsule(outline, collider.radius, collider.halfHeight, segments);
            }
            break;
        case ShapeType::Mesh:
            for (std::size_t i = 0; i + 2 < collider.triangles.size(); i += 3)
            {
                outline.Add(collider.triangles[i], collider.triangles[i + 1]);
                outline.Add(collider.triangles[i + 1], collider.triangles[i + 2]);
                outline.Add(collider.triangles[i + 2], collider.triangles[i]);
            }
            break;
        case ShapeType::Compound:
        {
            std::vector<Line> lines = outline.Take();
            for (const Collider& part : collider.parts)
            {
                const std::vector<Line> partLines = OutlineOf(part, pose, segments);
                lines.insert(lines.end(), partLines.begin(), partLines.end());
            }
            return lines;
        }
        }
        return outline.Take();
    }
}
