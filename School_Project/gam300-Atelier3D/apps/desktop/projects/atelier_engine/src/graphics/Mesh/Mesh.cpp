/* !
@file    	    Mesh.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Contains the implementation of the Mesh class methods and primitive
    factories. Implements vertex/index constructors, static geometry generators
    for common 2D and 3D primitives, and lazy GPU resource creation via the
    VertexArray interface.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Graphics Headers
#include "engine/graphics/Mesh/Mesh.hpp"
#include "engine/graphics/OpenGL/VertexArray.hpp"

// Standard Library Headers
#include <cmath>
#include <memory>
#include <utility>

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          Constructs a mesh with explicit vertex, index, and name data.
    @param          vertices      Vector of vertex attributes for the mesh.
    @param          indices       Vector of indices defining triangle or line topology.
    @param          name          Optional name string for mesh identification.
    *************************************************************************/
    Mesh::Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices, std::string name)
        : vertices_(std::move(vertices)), indices_(std::move(indices)), name_(std::move(name))
    {
    }

    /*!***********************************************************************
    @brief          Converts a CPU-side MeshData flat blob into a GPU-ready Graphics::Mesh. Normals the file
                    authored are used as they are; a vertex without one gets a normal derived from its faces.
    @param          cpuMesh       CPU mesh (flat positions, uvs, normals, indices).
    @param          name          Optional name for the resulting mesh.
    @return         Graphics::Mesh populated with per-vertex attributes.
    *************************************************************************/
    Mesh Mesh::FromAssetData(const Assets::MeshData& cpuMesh, std::string name)
    {
        const std::size_t vertexCount = cpuMesh.positions.size() / 3;
        std::vector<Vertex> vertices(vertexCount);

        const glm::vec3 defaultNormal(0.0f, 0.0f, 1.0f);
        const glm::vec4 defaultColor(1.0f, 1.0f, 1.0f, 1.0f);

        // Normals from the file count only when there is one per vertex; a zero normal means "not authored"
        const bool hasNormals = cpuMesh.normals.size() == cpuMesh.positions.size();
        const bool skinned = cpuMesh.joints.size() == vertexCount * 4 && cpuMesh.weights.size() == vertexCount * 4;
        std::vector<char> authored(vertexCount, 0);

        for (std::size_t i = 0; i < vertexCount; ++i)
        {
            Vertex& v = vertices[i];
            v.position = {cpuMesh.positions[i * 3 + 0], cpuMesh.positions[i * 3 + 1], cpuMesh.positions[i * 3 + 2]};
            v.normal = glm::vec3(0.0f);
            if (hasNormals)
            {
                const glm::vec3 n(cpuMesh.normals[i * 3 + 0], cpuMesh.normals[i * 3 + 1], cpuMesh.normals[i * 3 + 2]);
                if (glm::dot(n, n) > 1e-8f)
                {
                    v.normal = glm::normalize(n);
                    authored[i] = 1;
                }
            }
            v.color = defaultColor;
            if (i * 2 + 1 < cpuMesh.uvs.size())
            {
                v.texCoord = {cpuMesh.uvs[i * 2 + 0], cpuMesh.uvs[i * 2 + 1]};
            }
            if (skinned)
            {
                v.joints = {cpuMesh.joints[i * 4 + 0], cpuMesh.joints[i * 4 + 1], cpuMesh.joints[i * 4 + 2],
                            cpuMesh.joints[i * 4 + 3]};
                v.weights = {cpuMesh.weights[i * 4 + 0], cpuMesh.weights[i * 4 + 1], cpuMesh.weights[i * 4 + 2],
                             cpuMesh.weights[i * 4 + 3]};
            }
        }

        auto faceAccumulate = [&](std::size_t i0, std::size_t i1, std::size_t i2)
        {
            if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount)
            {
                return;
            }
            const glm::vec3& p0 = vertices[i0].position;
            const glm::vec3& p1 = vertices[i1].position;
            const glm::vec3& p2 = vertices[i2].position;
            const glm::vec3 e1 = p1 - p0;
            const glm::vec3 e2 = p2 - p0;
            const glm::vec3 faceNormal = glm::cross(e1, e2);
            if (glm::dot(faceNormal, faceNormal) > 1e-8f)
            {
                for (const std::size_t i : {i0, i1, i2})
                {
                    if (!authored[i])
                    {
                        vertices[i].normal += faceNormal;
                    }
                }
            }
        };

        if (!cpuMesh.indices.empty())
        {
            for (std::size_t t = 0; t + 2 < cpuMesh.indices.size(); t += 3)
            {
                faceAccumulate(cpuMesh.indices[t + 0], cpuMesh.indices[t + 1], cpuMesh.indices[t + 2]);
            }
        }
        else
        {
            for (std::size_t t = 0; t + 2 < vertexCount; t += 3)
            {
                faceAccumulate(t + 0, t + 1, t + 2);
            }
        }

        for (std::size_t i = 0; i < vertexCount; ++i)
        {
            if (authored[i])
            {
                continue;
            }
            const glm::vec3 n = vertices[i].normal;
            const float len2 = glm::dot(n, n);
            if (len2 > 1e-8f)
            {
                vertices[i].normal = glm::normalize(n);
            }
            else
            {
                vertices[i].normal = defaultNormal;
            }
        }

        if (name.empty())
        {
            name = "AssetMesh";
        }
        return Mesh(std::move(vertices), cpuMesh.indices, std::move(name));
    }

    // ============================================================
    // 2D MESH FACTORIES   (XY plane, Z = 0, normal +Z)
    // ============================================================

    /*!***********************************************************************
    @brief          Generates a 2D axis-aligned quad centered at the origin on the XY plane.
    @param          width         Horizontal extent of the quad.
    @param          height        Vertical extent of the quad.
    @return         Mesh instance populated with quad geometry data.
    *************************************************************************/
    Mesh Mesh::CreateQuad(float width, float height)
    {
        const float hw = width * 0.5f;
        const float hh = height * 0.5f;

        const glm::vec3 normal(0.0f, 0.0f, 1.0f);
        const glm::vec4 white(1.0f);

        const std::vector<Vertex> verts = {
            {glm::vec3(-hw, -hh, 0.0f), normal, glm::vec2(0.0f, 0.0f), white},
            {glm::vec3(hw, -hh, 0.0f), normal, glm::vec2(1.0f, 0.0f), white},
            {glm::vec3(hw, hh, 0.0f), normal, glm::vec2(1.0f, 1.0f), white},
            {glm::vec3(-hw, hh, 0.0f), normal, glm::vec2(0.0f, 1.0f), white},
        };
        const std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};

        return Mesh(verts, indices, "Quad");
    }

    /*!***********************************************************************
    @brief          Generates a 2D triangulated disc centered at the origin on the XY plane.
    @param          diameter      Overall diameter of the circle.
    @param          segments      Number of radial segments defining the perimeter.
    @return         Mesh instance populated with circle geometry data.
    *************************************************************************/
    Mesh Mesh::CreateCircle(float diameter, uint32_t segments)
    {
        if (segments < 3)
        {
            segments = 3;
        }

        constexpr float kTwoPi = 2.0f * 3.14159265358979323846f;
        const float radius = diameter * 0.5f;

        const glm::vec3 normal(0.0f, 0.0f, 1.0f);
        const glm::vec4 white(1.0f);

        std::vector<Vertex> verts;
        std::vector<uint32_t> indices;
        verts.reserve(static_cast<size_t>(segments) + 1);
        indices.reserve(static_cast<size_t>(segments) * 3);

        verts.push_back(Vertex{glm::vec3(0.0f), normal, glm::vec2(0.5f), white});
        for (uint32_t i = 0; i < segments; ++i)
        {
            const float a = static_cast<float>(i) / static_cast<float>(segments) * kTwoPi;
            const float cx = std::cos(a);
            const float cy = std::sin(a);
            verts.push_back(Vertex{glm::vec3(cx * radius, cy * radius, 0.0f), normal,
                                   glm::vec2(cx * 0.5f + 0.5f, cy * 0.5f + 0.5f), white});
        }
        for (uint32_t i = 0; i < segments; ++i)
        {
            const uint32_t a = i + 1;
            const uint32_t b = (i + 1) % segments + 1;
            indices.push_back(0);
            indices.push_back(a);
            indices.push_back(b);
        }

        return Mesh(std::move(verts), std::move(indices), "Circle");
    }

    // ============================================================
    // 3D MESH FACTORIES
    // ============================================================

    /*!***********************************************************************
    @brief          Generates a 3D axis-aligned cube centered at the origin.
    @param          size          Edge length of the cube along each axis.
    @return         Mesh instance populated with cube geometry data.
    *************************************************************************/
    Mesh Mesh::CreateCube(float size)
    {
        const float h = size * 0.5f;

        const glm::vec4 white(1.0f); // plain faces: the renderer tints every shape the same grey

        const glm::vec3 normalFront(0.0f, 0.0f, 1.0f);
        const glm::vec3 normalBack(0.0f, 0.0f, -1.0f);
        const glm::vec3 normalLeft(-1.0f, 0.0f, 0.0f);
        const glm::vec3 normalRight(1.0f, 0.0f, 0.0f);
        const glm::vec3 normalTop(0.0f, 1.0f, 0.0f);
        const glm::vec3 normalBottom(0.0f, -1.0f, 0.0f);

        std::vector<Vertex> verts;
        verts.reserve(24);

        verts.push_back(Vertex{glm::vec3(-h, -h, h), normalFront, glm::vec2(0.0f, 0.0f), white});
        verts.push_back(Vertex{glm::vec3(h, -h, h), normalFront, glm::vec2(1.0f, 0.0f), white});
        verts.push_back(Vertex{glm::vec3(h, h, h), normalFront, glm::vec2(1.0f, 1.0f), white});
        verts.push_back(Vertex{glm::vec3(-h, h, h), normalFront, glm::vec2(0.0f, 1.0f), white});

        verts.push_back(Vertex{glm::vec3(h, -h, -h), normalBack, glm::vec2(0.0f, 0.0f), white});
        verts.push_back(Vertex{glm::vec3(-h, -h, -h), normalBack, glm::vec2(1.0f, 0.0f), white});
        verts.push_back(Vertex{glm::vec3(-h, h, -h), normalBack, glm::vec2(1.0f, 1.0f), white});
        verts.push_back(Vertex{glm::vec3(h, h, -h), normalBack, glm::vec2(0.0f, 1.0f), white});

        verts.push_back(Vertex{glm::vec3(-h, -h, -h), normalLeft, glm::vec2(0.0f, 0.0f), white});
        verts.push_back(Vertex{glm::vec3(-h, -h, h), normalLeft, glm::vec2(1.0f, 0.0f), white});
        verts.push_back(Vertex{glm::vec3(-h, h, h), normalLeft, glm::vec2(1.0f, 1.0f), white});
        verts.push_back(Vertex{glm::vec3(-h, h, -h), normalLeft, glm::vec2(0.0f, 1.0f), white});

        verts.push_back(Vertex{glm::vec3(h, -h, h), normalRight, glm::vec2(0.0f, 0.0f), white});
        verts.push_back(Vertex{glm::vec3(h, -h, -h), normalRight, glm::vec2(1.0f, 0.0f), white});
        verts.push_back(Vertex{glm::vec3(h, h, -h), normalRight, glm::vec2(1.0f, 1.0f), white});
        verts.push_back(Vertex{glm::vec3(h, h, h), normalRight, glm::vec2(0.0f, 1.0f), white});

        verts.push_back(Vertex{glm::vec3(-h, h, -h), normalTop, glm::vec2(0.0f, 0.0f), white});
        verts.push_back(Vertex{glm::vec3(-h, h, h), normalTop, glm::vec2(1.0f, 0.0f), white});
        verts.push_back(Vertex{glm::vec3(h, h, h), normalTop, glm::vec2(1.0f, 1.0f), white});
        verts.push_back(Vertex{glm::vec3(h, h, -h), normalTop, glm::vec2(0.0f, 1.0f), white});

        // Counter-clockwise seen from below, like every face from outside (back faces are culled)
        verts.push_back(Vertex{glm::vec3(-h, -h, -h), normalBottom, glm::vec2(0.0f, 0.0f), white});
        verts.push_back(Vertex{glm::vec3(h, -h, -h), normalBottom, glm::vec2(1.0f, 0.0f), white});
        verts.push_back(Vertex{glm::vec3(h, -h, h), normalBottom, glm::vec2(1.0f, 1.0f), white});
        verts.push_back(Vertex{glm::vec3(-h, -h, h), normalBottom, glm::vec2(0.0f, 1.0f), white});

        std::vector<uint32_t> indices;
        indices.reserve(36);
        for (uint32_t face = 0; face < 6; ++face)
        {
            const uint32_t base = face * 4;
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        }

        return Mesh(std::move(verts), std::move(indices), "Cube");
    }

    /*!***********************************************************************
    @brief          Generates a 3D square-based pyramid with apex along the positive Y axis.
    @param          baseHalf      Half-extent of the square base along X and Z axes.
    @param          baseY         Y-coordinate of the base vertices.
    @param          apexY         Y-coordinate of the pyramid apex.
    @return         Mesh instance populated with pyramid geometry data.
    *************************************************************************/
    Mesh Mesh::CreatePyramid(float baseHalf, float baseY, float apexY)
    {
        const glm::vec3 apex(0.0f, apexY, 0.0f);
        const glm::vec3 baseFL(-baseHalf, baseY, baseHalf);
        const glm::vec3 baseFR(baseHalf, baseY, baseHalf);
        const glm::vec3 baseBR(baseHalf, baseY, -baseHalf);
        const glm::vec3 baseBL(-baseHalf, baseY, -baseHalf);

        const glm::vec4 white(1.0f); // plain faces: the renderer tints every shape the same grey

        const glm::vec2 uvZero(0.0f);
        const glm::vec2 uvA(0.5f, 1.0f);
        const glm::vec2 uvBL(0.0f, 0.0f);
        const glm::vec2 uvBR(1.0f, 0.0f);
        const glm::vec2 uvTR(1.0f, 1.0f);
        const glm::vec2 uvTL(0.0f, 1.0f);

        const auto FaceNormal = [](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
        { return glm::normalize(glm::cross(b - a, c - a)); };
        const glm::vec3 nFront = FaceNormal(baseFL, baseFR, apex);
        const glm::vec3 nRight = FaceNormal(baseFR, baseBR, apex);
        const glm::vec3 nBack = FaceNormal(baseBR, baseBL, apex);
        const glm::vec3 nLeft = FaceNormal(baseBL, baseFL, apex);
        const glm::vec3 nDown(0.0f, -1.0f, 0.0f);

        std::vector<Vertex> verts;
        verts.reserve(19);

        // Front triangle: apex -> FL -> FR (CCW seen from +Z)
        verts.push_back(Vertex{apex, nFront, uvA, white});
        verts.push_back(Vertex{baseFL, nFront, uvBL, white});
        verts.push_back(Vertex{baseFR, nFront, uvBR, white});

        // Right triangle: apex -> FR -> BR (CCW seen from +X)
        verts.push_back(Vertex{apex, nRight, uvA, white});
        verts.push_back(Vertex{baseFR, nRight, uvBL, white});
        verts.push_back(Vertex{baseBR, nRight, uvBR, white});

        // Back triangle: apex -> BR -> BL (CCW seen from -Z)
        verts.push_back(Vertex{apex, nBack, uvA, white});
        verts.push_back(Vertex{baseBR, nBack, uvBL, white});
        verts.push_back(Vertex{baseBL, nBack, uvBR, white});

        // Left triangle: apex -> BL -> FL (CCW seen from -X)
        verts.push_back(Vertex{apex, nLeft, uvA, white});
        verts.push_back(Vertex{baseBL, nLeft, uvBL, white});
        verts.push_back(Vertex{baseFL, nLeft, uvBR, white});

        // Base quad (seen from below = -Y, looking up): FL -> BL -> BR -> FR (CCW)
        verts.push_back(Vertex{baseFL, nDown, uvBL, white});
        verts.push_back(Vertex{baseBL, nDown, uvTL, white});
        verts.push_back(Vertex{baseBR, nDown, uvTR, white});
        verts.push_back(Vertex{baseFR, nDown, uvBR, white});

        // 4 side-triangles (3 indices) + base (2 tris x 3 = 6) = 18 total
        const std::vector<uint32_t> indices = {
            0,  1,  2,  // front
            3,  4,  5,  // right
            6,  7,  8,  // back
            9,  10, 11, // left
            12, 13, 14, // base tri 1
            12, 14, 15, // base tri 2
        };

        return Mesh(std::move(verts), indices, "Pyramid");
    }

    /*!***********************************************************************
    @brief          Generates a 3D subdivided XZ-plane with upward (+Y) facing normals.
    @param          size          Overall edge length of the plane along X and Z.
    @param          subdivisions  Number of grid subdivisions along each axis.
    @return         Mesh instance populated with plane geometry data.
    *************************************************************************/
    Mesh Mesh::CreatePlane(float size, int subdivisions)
    {
        if (subdivisions < 1)
        {
            subdivisions = 1;
        }

        const float half = size * 0.5f;
        const float step = size / static_cast<float>(subdivisions);

        const int vertsPerSide = subdivisions + 1;
        const int totalVerts = vertsPerSide * vertsPerSide;
        const int totalIndices = subdivisions * subdivisions * 6;

        std::vector<Vertex> verts;
        verts.reserve(static_cast<size_t>(totalVerts));

        const glm::vec3 normal(0.0f, 1.0f, 0.0f);
        const glm::vec4 white(1.0f);

        for (int z = 0; z < vertsPerSide; ++z)
        {
            for (int x = 0; x < vertsPerSide; ++x)
            {
                const float fx = static_cast<float>(x) * step - half;
                const float fz = static_cast<float>(z) * step - half;
                const float u = static_cast<float>(x) / static_cast<float>(subdivisions);
                const float v = static_cast<float>(z) / static_cast<float>(subdivisions);
                verts.push_back(Vertex{glm::vec3(fx, 0.0f, fz), normal, glm::vec2(u, v), white});
            }
        }

        std::vector<uint32_t> indices;
        indices.reserve(static_cast<size_t>(totalIndices));
        const uint32_t vps = static_cast<uint32_t>(vertsPerSide);

        for (int z = 0; z < subdivisions; ++z)
        {
            for (int x = 0; x < subdivisions; ++x)
            {
                const uint32_t i00 = static_cast<uint32_t>(z) * vps + static_cast<uint32_t>(x);
                const uint32_t i10 = i00 + 1;
                const uint32_t i01 = i00 + vps;
                const uint32_t i11 = i01 + 1;

                indices.push_back(i00);
                indices.push_back(i01);
                indices.push_back(i11);

                indices.push_back(i00);
                indices.push_back(i11);
                indices.push_back(i10);
            }
        }

        return Mesh(std::move(verts), std::move(indices), "Plane");
    }

    /*!***********************************************************************
    @brief          Generates a 3D axis-aligned cube outline using line segment topology.
    @param          size          Edge length of the wirebox along each axis.
    @return         Mesh instance populated with wirebox line geometry.
    *************************************************************************/
    Mesh Mesh::CreateWirebox(float size)
    {
        const float h = size * 0.5f;

        const glm::vec3 normal(0.0f, 1.0f, 0.0f);
        const glm::vec2 uv(0.0f);
        const glm::vec4 white(1.0f);

        const std::vector<Vertex> verts = {
            {glm::vec3(-h, -h, -h), normal, uv, white}, {glm::vec3(h, -h, -h), normal, uv, white},
            {glm::vec3(h, h, -h), normal, uv, white},   {glm::vec3(-h, h, -h), normal, uv, white},
            {glm::vec3(-h, -h, h), normal, uv, white},  {glm::vec3(h, -h, h), normal, uv, white},
            {glm::vec3(h, h, h), normal, uv, white},    {glm::vec3(-h, h, h), normal, uv, white},
        };

        const std::vector<uint32_t> indices = {
            0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7,
        };

        return Mesh(verts, indices, "Wirebox");
    }

    /*!***********************************************************************
    @brief          Generates a 3D UV-sphere centered at the origin.
    @param          radius          Radius of the sphere from center to surface.
    @param          latitudeBands   Number of horizontal latitude subdivisions.
    @param          longitudeBands  Number of vertical longitude subdivisions.
    @return         Mesh instance populated with sphere geometry data.
    *************************************************************************/
    Mesh Mesh::CreateSphere(float radius, int latitudeBands, int longitudeBands)
    {
        if (latitudeBands < 2)
        {
            latitudeBands = 2;
        }
        if (longitudeBands < 3)
        {
            longitudeBands = 3;
        }

        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kTwoPi = kPi * 2.0f;

        const glm::vec4 white(1.0f);

        std::vector<Vertex> verts;
        verts.reserve(static_cast<size_t>(latitudeBands + 1) * static_cast<size_t>(longitudeBands + 1));

        for (int lat = 0; lat <= latitudeBands; ++lat)
        {
            const float theta = static_cast<float>(lat) * kPi / static_cast<float>(latitudeBands);
            const float sinTheta = std::sin(theta);
            const float cosTheta = std::cos(theta);

            for (int lon = 0; lon <= longitudeBands; ++lon)
            {
                const float phi = static_cast<float>(lon) * kTwoPi / static_cast<float>(longitudeBands);
                const float sinPhi = std::sin(phi);
                const float cosPhi = std::cos(phi);

                const glm::vec3 normal(cosPhi * sinTheta, cosTheta, sinPhi * sinTheta);
                const glm::vec3 position = normal * radius;
                const glm::vec2 uv(1.0f - (static_cast<float>(lon) / static_cast<float>(longitudeBands)),
                                   1.0f - (static_cast<float>(lat) / static_cast<float>(latitudeBands)));

                verts.push_back(Vertex{position, normal, uv, white});
            }
        }

        std::vector<uint32_t> indices;
        indices.reserve(static_cast<size_t>(latitudeBands) * static_cast<size_t>(longitudeBands) * 6);
        const uint32_t lons = static_cast<uint32_t>(longitudeBands) + 1;

        for (int lat = 0; lat < latitudeBands; ++lat)
        {
            for (int lon = 0; lon < longitudeBands; ++lon)
            {
                const uint32_t i0 = static_cast<uint32_t>(lat) * lons + static_cast<uint32_t>(lon);
                const uint32_t i1 = i0 + lons;
                const uint32_t i2 = i0 + 1;
                const uint32_t i3 = i1 + 1;

                // Counter-clockwise seen from outside, so culling drops the inside and not the outside
                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i1);

                indices.push_back(i1);
                indices.push_back(i2);
                indices.push_back(i3);
            }
        }

        return Mesh(std::move(verts), std::move(indices), "Sphere");
    }

    /*!***********************************************************************
    @brief          Generates a straight 3D line segment between two world-space points.
    @param          from          Start point of the line segment.
    @param          to            End point of the line segment.
    @param          color         Per-vertex RGBA color applied to both endpoints.
    @return         Mesh instance populated with line segment geometry.
    *************************************************************************/
    Mesh Mesh::CreateLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color)
    {
        const glm::vec3 normal(0.0f, 1.0f, 0.0f);
        const glm::vec2 uv(0.0f);

        const std::vector<Vertex> verts = {
            {from, normal, uv, color},
            {to, normal, uv, color},
        };
        const std::vector<uint32_t> indices = {0, 1};

        return Mesh(verts, indices, "Line");
    }

    // ============================================================
    // EDITOR / DEBUG LINE MESH FACTORIES
    // ============================================================

    /*!***********************************************************************
    @brief          Generates an XZ-plane ground grid on y = 0 with accent major lines.
    @param          halfExtent       Distance from origin to edge along X and Z axes.
    @param          majorEveryUnits  Step size at which lines use the brighter major color.
    @return         Mesh instance populated with 2-point line segments and per-vertex colors.
    *************************************************************************/
    Mesh Mesh::CreateGrid(int halfExtent, int majorEveryUnits)
    {
        const glm::vec4 minor(0.30f, 0.32f, 0.36f, 1.0f);
        const glm::vec4 major(0.46f, 0.48f, 0.53f, 1.0f);
        const glm::vec3 up(0.0f, 1.0f, 0.0f);
        const glm::vec2 uv(0.0f);

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        const auto line = [&](glm::vec3 a, glm::vec3 b, glm::vec4 color)
        {
            const auto first = static_cast<uint32_t>(vertices.size());
            vertices.push_back({.position = a, .normal = up, .texCoord = uv, .color = color});
            vertices.push_back({.position = b, .normal = up, .texCoord = uv, .color = color});
            indices.insert(indices.end(), {first, first + 1});
        };

        if (halfExtent < 1)
        {
            halfExtent = 1;
        }
        if (majorEveryUnits < 1)
        {
            majorEveryUnits = 1;
        }

        const float extent = static_cast<float>(halfExtent);
        for (int i = -halfExtent; i <= halfExtent; ++i)
        {
            const float at = static_cast<float>(i);
            const glm::vec4 color = i % majorEveryUnits == 0 ? major : minor;
            line({at, 0.0f, -extent}, {at, 0.0f, extent}, color);
            line({-extent, 0.0f, at}, {extent, 0.0f, at}, color);
        }
        return Mesh(std::move(vertices), std::move(indices), "EditorGrid");
    }

    /*!***********************************************************************
    @brief          Generates X + Z axis indicator lines on y = 0, each +/-axisLength.
    @param          axisLength    Half-length of each axis in world units.
    @return         Mesh instance populated with 2-point colored line segments (X = red, Z = blue).
    *************************************************************************/
    Mesh Mesh::CreateGridAxes(float axisLength)
    {
        const glm::vec3 up(0.0f, 1.0f, 0.0f);
        const glm::vec2 uv(0.0f);
        if (axisLength <= 0.0f)
        {
            axisLength = 1.0f;
        }

        std::vector<Vertex> vertices{
            {.position = {-axisLength, 0.0f, 0.0f}, .normal = up, .texCoord = uv, .color = {0.85f, 0.28f, 0.28f, 1.0f}},
            {.position = {axisLength, 0.0f, 0.0f}, .normal = up, .texCoord = uv, .color = {0.85f, 0.28f, 0.28f, 1.0f}},
            {.position = {0.0f, 0.0f, -axisLength}, .normal = up, .texCoord = uv, .color = {0.28f, 0.45f, 0.90f, 1.0f}},
            {.position = {0.0f, 0.0f, axisLength}, .normal = up, .texCoord = uv, .color = {0.28f, 0.45f, 0.90f, 1.0f}},
        };
        return Mesh(std::move(vertices), {0, 1, 2, 3}, "EditorGridAxes");
    }

    /*!***********************************************************************
    @brief          Lazily creates and binds the GPU vertex array for rendering.
    *************************************************************************/
    void Mesh::Bind() const
    {
        if (!vertexArray_)
        {
            vertexArray_ = std::make_shared<OpenGL::VertexArray>();
            vertexArray_->Create(vertices_, indices_);
        }
        vertexArray_->Bind();
    }

    /*!***********************************************************************
    @brief          Unbinds the underlying GPU vertex array if one has been created.
    *************************************************************************/
    void Mesh::Unbind() const
    {
        if (vertexArray_)
        {
            vertexArray_->Unbind();
        }
    }
}
