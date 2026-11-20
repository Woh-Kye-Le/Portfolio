#include "engine/graphics/Mesh/Mesh.hpp"
#include "engine/platform/Window.hpp"

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>

// The built-in shapes the editor's Create menu adds, and the editor's window

using Engine::Graphics::Mesh;

namespace
{
    void ExpectPlain(const std::string& name, const Mesh& mesh)
    {
        ASSERT_FALSE(mesh.GetVertices().empty()) << name;
        for (const auto& vertex : mesh.GetVertices())
        {
            EXPECT_EQ(vertex.color, glm::vec4(1.0f)) << name;
        }
    }

    // Every triangle is counter-clockwise seen from the side its normals point to: the renderer culls back faces, so
    // a triangle wound the other way would hide the shape's outside and show its inside
    void ExpectFacingOut(const std::string& name, const Mesh& mesh)
    {
        const auto& v = mesh.GetVertices();
        const auto& indices = mesh.GetIndices();
        ASSERT_EQ(indices.size() % 3, 0u) << name;
        for (std::size_t t = 0; t < indices.size(); t += 3)
        {
            const glm::vec3 a = v[indices[t]].position;
            const glm::vec3 b = v[indices[t + 1]].position;
            const glm::vec3 c = v[indices[t + 2]].position;
            const glm::vec3 face = glm::cross(b - a, c - a);
            if (glm::dot(face, face) < 1e-12f)
            {
                continue; // the sphere's poles have triangles with no area
            }
            const glm::vec3 normal = v[indices[t]].normal + v[indices[t + 1]].normal + v[indices[t + 2]].normal;
            EXPECT_GT(glm::dot(face, normal), 0.0f) << name << " triangle " << t / 3 << " faces inwards";
        }
    }
}

TEST(BuiltInShapes, EveryTriangleFacesOutwards)
{
    ExpectFacingOut("cube", Mesh::CreateCube());
    ExpectFacingOut("pyramid", Mesh::CreatePyramid());
    ExpectFacingOut("sphere", Mesh::CreateSphere());
    ExpectFacingOut("plane", Mesh::CreatePlane());
    ExpectFacingOut("quad", Mesh::CreateQuad());
    ExpectFacingOut("circle", Mesh::CreateCircle());
}

// Every solid shape is plain white, so the renderer's one neutral grey (times the object's own tint) is all that shows:
// no face of a cube or a pyramid has a colour of its own, as in other editors
TEST(BuiltInShapes, EveryFaceIsPlainSoOnlyTheTintShows)
{
    ExpectPlain("cube", Mesh::CreateCube());
    ExpectPlain("pyramid", Mesh::CreatePyramid());
    ExpectPlain("sphere", Mesh::CreateSphere());
    ExpectPlain("plane", Mesh::CreatePlane());
    ExpectPlain("quad", Mesh::CreateQuad());
    ExpectPlain("circle", Mesh::CreateCircle());
}

// The cube still has its 6 faces of 4 corners each (one normal per face, for lighting)
TEST(BuiltInShapes, TheCubeKeepsItsFaces)
{
    const Mesh cube = Mesh::CreateCube(2.0f);
    ASSERT_EQ(cube.GetVertices().size(), 24u);
    for (const auto& vertex : cube.GetVertices())
    {
        EXPECT_FLOAT_EQ(std::max({std::abs(vertex.position.x), std::abs(vertex.position.y),
                                  std::abs(vertex.position.z)}),
                        1.0f);
    }
}

// Windows open at their size unless asked to fill the screen (the editor is; games use their game.cfg size)
TEST(WindowSpec, OpensAtItsSizeUnlessMaximized)
{
    const Engine::Platform::WindowSpec spec;
    EXPECT_FALSE(spec.maximized);
    EXPECT_EQ(spec.width, 1280);
    EXPECT_EQ(spec.height, 720);
}
