/* !
@file               LodTests.cpp
@author             w.kyele@digipen.edu [100%]
@date               03/11/2026
@brief
    Tests of level of detail: the choice of level (the coarsest one whose
    threshold is passed, a margin before a finer one comes back, missing levels
    skipped, by size on screen), and the simplifier that makes the simpler
    versions of a model.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/assets/MeshSimplifier.hpp"
#include "engine/scene/Lod.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>

using Engine::Scene::ChooseLodLevel;

namespace
{
    const std::array<float, 2> kStarts{10.0f, 25.0f};
    const std::array<bool, 2> kBoth{true, true};
}

TEST(LodTest, TheCameraGoingAwaySwitchesToSimplerModels)
{
    EXPECT_EQ(ChooseLodLevel(2.0f, 0, kStarts, kBoth), 0);
    EXPECT_EQ(ChooseLodLevel(10.0f, 0, kStarts, kBoth), 1); // from its distance on
    EXPECT_EQ(ChooseLodLevel(24.9f, 1, kStarts, kBoth), 1);
    EXPECT_EQ(ChooseLodLevel(25.0f, 1, kStarts, kBoth), 2);
    EXPECT_EQ(ChooseLodLevel(500.0f, 0, kStarts, kBoth), 2); // a jump skips the level between
}

TEST(LodTest, TheFinerModelComesBackOnlyClearlyInsideItsDistance)
{
    // 10% margin: LOD 1 from 10 m stays down to 9 m, LOD 2 from 25 m down to 22.5 m
    EXPECT_EQ(ChooseLodLevel(9.5f, 1, kStarts, kBoth), 1);
    EXPECT_EQ(ChooseLodLevel(8.9f, 1, kStarts, kBoth), 0);
    EXPECT_EQ(ChooseLodLevel(23.0f, 2, kStarts, kBoth), 2);
    EXPECT_EQ(ChooseLodLevel(22.0f, 2, kStarts, kBoth), 1);
    EXPECT_EQ(ChooseLodLevel(3.0f, 2, kStarts, kBoth), 0);       // straight back in close
    EXPECT_EQ(ChooseLodLevel(9.5f, 1, kStarts, kBoth, 0.0f), 0); // no margin
}

TEST(LodTest, LevelsTheObjectDoesNotHaveAreSkipped)
{
    EXPECT_EQ(ChooseLodLevel(15.0f, 0, kStarts, {false, true}), 0); // no LOD 1: the full model until LOD 2
    EXPECT_EQ(ChooseLodLevel(30.0f, 0, kStarts, {false, true}), 2);
    EXPECT_EQ(ChooseLodLevel(30.0f, 0, kStarts, {true, false}), 1); // no LOD 2: LOD 1 however far
    EXPECT_EQ(ChooseLodLevel(30.0f, 0, kStarts, {false, false}), 0);
    // A level drawn last frame that has been taken away is not kept
    EXPECT_EQ(ChooseLodLevel(30.0f, 2, kStarts, {true, false}), 1);
    EXPECT_EQ(ChooseLodLevel(5.0f, 7, kStarts, kBoth), 0);
}

TEST(LodTest, TheSmallerOnScreenTheSimplerTheModel)
{
    using Engine::Scene::ChooseLodLevelForScreenSize;
    using Engine::Scene::ScreenSize;
    // A 2-unit model (radius 1) with a 90 degree view (projection [1][1] = 1): fills the view at 1 unit away
    EXPECT_FLOAT_EQ(ScreenSize(1.0f, 4.0f, 1.0f), 0.25f);
    EXPECT_GT(ScreenSize(1.0f, 0.5f, 1.0f), 1.0f); // the camera inside it

    EXPECT_EQ(ChooseLodLevelForScreenSize(0.9f, 0, 3), 0);
    EXPECT_EQ(ChooseLodLevelForScreenSize(0.39f, 0, 3), 1); // under 40% of the view
    EXPECT_EQ(ChooseLodLevelForScreenSize(0.14f, 1, 3), 2); // under 15%
    EXPECT_EQ(ChooseLodLevelForScreenSize(0.41f, 1, 3), 1); // the margin: back to LOD 0 only above about 44%
    EXPECT_EQ(ChooseLodLevelForScreenSize(0.5f, 1, 3), 0);
    EXPECT_EQ(ChooseLodLevelForScreenSize(0.01f, 0, 1), 0); // a model without simpler versions
    EXPECT_EQ(ChooseLodLevelForScreenSize(0.01f, 0, 2), 1); // only LOD 1
}

namespace
{
    using Engine::Assets::MeshData;
    using Engine::Assets::ModelData;

    // A flat n x n grid of quads in the XZ plane, 2 triangles each, with uvs, normals and two material runs
    MeshData Grid(int n)
    {
        MeshData mesh;
        for (int z = 0; z <= n; ++z)
        {
            for (int x = 0; x <= n; ++x)
            {
                mesh.positions.insert(mesh.positions.end(), {float(x), 0.0f, float(z)});
                mesh.uvs.insert(mesh.uvs.end(), {float(x) / n, float(z) / n});
                mesh.normals.insert(mesh.normals.end(), {0.0f, 1.0f, 0.0f});
            }
        }
        const auto at = [n](int x, int z) { return static_cast<std::uint32_t>(z * (n + 1) + x); };
        for (int z = 0; z < n; ++z)
        {
            for (int x = 0; x < n; ++x)
            {
                mesh.indices.insert(mesh.indices.end(), {at(x, z), at(x, z + 1), at(x + 1, z), at(x + 1, z),
                                                         at(x, z + 1), at(x + 1, z + 1)});
            }
        }
        const auto half = static_cast<std::uint32_t>(mesh.indices.size() / 2);
        mesh.subMeshes = {{0, half, 0, 0}, {half, half, 0, 1}};
        return mesh;
    }

    // A sphere made of latitude and longitude bands (closed, no border), welded but with a texture seam
    MeshData Sphere(int bands)
    {
        MeshData mesh;
        for (int i = 0; i <= bands; ++i)
        {
            const float theta = 3.14159265f * float(i) / float(bands);
            for (int j = 0; j <= bands; ++j)
            {
                const float phi = 2.0f * 3.14159265f * float(j) / float(bands);
                mesh.positions.insert(mesh.positions.end(), {std::sin(theta) * std::cos(phi), std::cos(theta),
                                                             std::sin(theta) * std::sin(phi)});
                mesh.uvs.insert(mesh.uvs.end(), {float(j) / bands, float(i) / bands});
            }
        }
        for (int i = 0; i < bands; ++i)
        {
            for (int j = 0; j < bands; ++j)
            {
                const auto a = static_cast<std::uint32_t>(i * (bands + 1) + j);
                const auto b = a + static_cast<std::uint32_t>(bands + 1);
                mesh.indices.insert(mesh.indices.end(), {a, b, a + 1, a + 1, b, b + 1});
            }
        }
        return mesh;
    }
}

TEST(MeshSimplifierTest, AFlatGridLosesMostOfItsTrianglesButKeepsItsOutlineAndRuns)
{
    const MeshData grid = Grid(20); // 800 triangles
    const MeshData simple = Engine::Assets::SimplifyMesh(grid, 0.2f);

    EXPECT_LE(Engine::Assets::TriangleCount(simple), 200u);
    EXPECT_GT(Engine::Assets::TriangleCount(simple), 0u);
    // Every vertex is one of the grid's, with its own uv and normal
    ASSERT_EQ(simple.uvs.size() / 2, simple.positions.size() / 3);
    ASSERT_EQ(simple.normals.size(), simple.positions.size());
    // The corners stay, so the plane still covers 0..20 in x and z
    float minX = 1e9f, maxX = -1e9f, minZ = 1e9f, maxZ = -1e9f;
    for (std::size_t i = 0; i < simple.positions.size(); i += 3)
    {
        minX = std::min(minX, simple.positions[i]);
        maxX = std::max(maxX, simple.positions[i]);
        minZ = std::min(minZ, simple.positions[i + 2]);
        maxZ = std::max(maxZ, simple.positions[i + 2]);
        EXPECT_FLOAT_EQ(simple.positions[i + 1], 0.0f); // still flat
    }
    EXPECT_FLOAT_EQ(minX, 0.0f);
    EXPECT_FLOAT_EQ(maxX, 20.0f);
    EXPECT_FLOAT_EQ(minZ, 0.0f);
    EXPECT_FLOAT_EQ(maxZ, 20.0f);
    // Both material runs are still there, in order, and every index is in range
    ASSERT_EQ(simple.subMeshes.size(), 2u);
    EXPECT_EQ(simple.subMeshes[0].materialIndex, 0u);
    EXPECT_EQ(simple.subMeshes[1].materialIndex, 1u);
    for (const std::uint32_t index : simple.indices)
    {
        EXPECT_LT(index, simple.positions.size() / 3);
    }
}

TEST(MeshSimplifierTest, ASphereKeepsItsShapeAndItsSeam)
{
    const MeshData sphere = Sphere(32); // 2048 triangles
    const MeshData simple = Engine::Assets::SimplifyMesh(sphere, 0.25f);
    EXPECT_LE(Engine::Assets::TriangleCount(simple), 700u); // close to the 512 asked for; seams may keep a few more
    EXPECT_LT(Engine::Assets::TriangleCount(simple), Engine::Assets::TriangleCount(sphere) / 2);
    // Every vertex left is still on the sphere (a vertex keeps its own position)
    for (std::size_t i = 0; i < simple.positions.size(); i += 3)
    {
        const float r =
            std::sqrt(simple.positions[i] * simple.positions[i] + simple.positions[i + 1] * simple.positions[i + 1] +
                      simple.positions[i + 2] * simple.positions[i + 2]);
        EXPECT_NEAR(r, 1.0f, 1e-4f);
    }
    // The seam's vertices (u = 0 and u = 1 at the same places) all stay, so it cannot open
    std::set<std::pair<long, long>> seamLeft; // positions to a ten-thousandth
    std::set<std::pair<long, long>> seamRight;
    for (std::size_t v = 0; v < simple.positions.size() / 3; ++v)
    {
        const float u = simple.uvs[2 * v];
        if (u == 0.0f || u == 1.0f)
        {
            (u == 0.0f ? seamLeft : seamRight)
                .insert({std::lround(simple.positions[3 * v] * 1e4f), std::lround(simple.positions[3 * v + 1] * 1e4f)});
        }
    }
    EXPECT_EQ(seamLeft, seamRight);
}

TEST(MeshSimplifierTest, AModelGetsSimplerVersionsOnlyWhenItIsWorthIt)
{
    ModelData big;
    big.meshes.push_back(Sphere(48)); // 4608 triangles
    big.meshes.push_back(Grid(3));    // 18: a small part, kept as it is
    Engine::Assets::GenerateLods(big);
    ASSERT_EQ(big.lods.size(), 2u);
    ASSERT_EQ(big.lods[0].size(), big.meshes.size()); // a full set of meshes per level, in the same order
    const std::size_t full = Engine::Assets::TriangleCount(big, 0);
    const std::size_t half = Engine::Assets::TriangleCount(big, 1);
    const std::size_t fifth = Engine::Assets::TriangleCount(big, 2);
    EXPECT_LT(half, full * 6 / 10);
    EXPECT_LT(fifth, half);
    EXPECT_EQ(Engine::Assets::TriangleCount(big.lods[0][1]), 18u);

    ModelData small;
    small.meshes.push_back(Grid(10)); // 200 triangles: cheap at any distance
    Engine::Assets::GenerateLods(small);
    EXPECT_TRUE(small.lods.empty());
}

TEST(MeshSimplifierTest, TheSimplerVersionsAreMadeOnceAndKeptUntilTheFileChanges)
{
    const auto source = std::filesystem::temp_directory_path() / "atelier_tests" / "lod_cache_source.obj";
    std::filesystem::create_directories(source.parent_path());
    std::ofstream(source, std::ios::binary) << "stands for the model's file";

    ModelData first;
    first.meshes.push_back(Sphere(48));
    Engine::Assets::LoadOrGenerateLods(first, source);
    ASSERT_EQ(first.lods.size(), 2u);

    // Loaded again: the same versions, read from the cache
    ModelData again;
    again.meshes.push_back(Sphere(48));
    Engine::Assets::LoadOrGenerateLods(again, source);
    ASSERT_EQ(again.lods.size(), 2u);
    EXPECT_EQ(again.lods[0][0].indices, first.lods[0][0].indices);
    EXPECT_EQ(again.lods[1][0].positions, first.lods[1][0].positions);

    // The file changed (another model): made again for it, not taken from the cache
    std::ofstream(source, std::ios::binary) << "another model, saved over the first one";
    ModelData changed;
    changed.meshes.push_back(Sphere(40));
    Engine::Assets::LoadOrGenerateLods(changed, source);
    ASSERT_EQ(changed.lods.size(), 2u);
    EXPECT_LT(Engine::Assets::TriangleCount(changed, 1), Engine::Assets::TriangleCount(changed, 0));
    EXPECT_NE(changed.lods[0][0].positions.size(), first.lods[0][0].positions.size());
    std::filesystem::remove(source);
}
