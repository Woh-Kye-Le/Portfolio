/* !
@file    	    Mesh.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Declares the Mesh class for managing renderable geometry. Stores vertex and
    index data, provides GPU binding via an OpenGL vertex array, and includes
    static factory methods for generating common primitive shapes.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Assets Headers
#include "engine/assets/data/MeshData.hpp"

// Engine Graphics Headers
#include "engine/graphics/Mesh/Vertex.hpp"

// Standard Library Headers
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Engine::Graphics
{
    namespace OpenGL
    {
        class VertexArray;
    }

    /*!***********************************************************************
    @brief          Renderable mesh holding vertex/index data with primitive shape factories.
    *************************************************************************/
    class Mesh
    {
    public:
        /*!***********************************************************************
        @brief          Default constructor creating an empty uninitialized mesh.
        *************************************************************************/
        Mesh() = default;

        /*!***********************************************************************
        @brief          Constructor initializing mesh with explicit geometry data.
        @param          vertices      Vector of vertex attributes for the mesh.
        @param          indices       Vector of indices defining triangle or line topology.
        @param          name          Optional name string for mesh identification.
        *************************************************************************/
        Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices, std::string name = "");

        /*!***********************************************************************
        @brief          Builds a renderable Mesh from a CPU-side MeshData blob.
                        Populates positions directly, uses asset UVs and normals when present
                        (a vertex without a normal gets one derived from its faces), fills
                        sensible defaults (opaque white color) for attributes the asset source omits.
        @param          cpuMesh       CPU-side mesh (positions/uvs/normals/indices).
        @param          name          Optional name string for mesh identification.
        @return         Graphics::Mesh instance ready for lazy GPU upload.
        *************************************************************************/
        static Mesh FromAssetData(const Assets::MeshData& cpuMesh, std::string name = "");

        /*!***********************************************************************
        @brief          Creates a 2D axis-aligned quad centered on the origin.
        @param          width         Horizontal extent of the quad.
        @param          height        Vertical extent of the quad.
        @return         Mesh instance populated with quad geometry.
        *************************************************************************/
        static Mesh CreateQuad(float width = 1.0f, float height = 1.0f);

        /*!***********************************************************************
        @brief          Creates a 2D triangulated disc (circle) centered on the origin.
        @param          diameter      Overall diameter of the circle.
        @param          segments      Number of radial segments defining the perimeter.
        @return         Mesh instance populated with circle geometry.
        *************************************************************************/
        static Mesh CreateCircle(float diameter = 1.0f, uint32_t segments = 48);

        /*!***********************************************************************
        @brief          Creates a 3D axis-aligned cube centered on the origin.
        @param          size          Edge length of the cube along each axis.
        @return         Mesh instance populated with cube geometry.
        *************************************************************************/
        static Mesh CreateCube(float size = 1.0f);

        /*!***********************************************************************
        @brief          Creates a 3D square-based pyramid with apex along the positive Y axis.
        @param          baseHalf      Half-extent of the square base along X and Z.
        @param          baseY         Y-coordinate of the base vertices.
        @param          apexY         Y-coordinate of the pyramid apex.
        @return         Mesh instance populated with pyramid geometry.
        *************************************************************************/
        static Mesh CreatePyramid(float baseHalf = 0.7f, float baseY = -0.6f, float apexY = 0.6f);

        /*!***********************************************************************
        @brief          Creates a 3D subdivided XZ-plane with upward (+Y) facing normals.
        @param          size          Overall edge length of the plane.
        @param          subdivisions  Number of grid subdivisions along each axis.
        @return         Mesh instance populated with plane geometry.
        *************************************************************************/
        static Mesh CreatePlane(float size = 10.0f, int subdivisions = 1);

        /*!***********************************************************************
        @brief          Creates a 3D axis-aligned cube outline rendered as line segments.
        @param          size          Edge length of the wirebox along each axis.
        @return         Mesh instance populated with wirebox line geometry.
        *************************************************************************/
        static Mesh CreateWirebox(float size = 1.0f);

        /*!***********************************************************************
        @brief          Creates a 3D UV-sphere centered on the origin.
        @param          radius          Radius of the sphere.
        @param          latitudeBands   Number of horizontal latitude subdivisions.
        @param          longitudeBands  Number of vertical longitude subdivisions.
        @return         Mesh instance populated with sphere geometry.
        *************************************************************************/
        static Mesh CreateSphere(float radius = 1.0f, int latitudeBands = 16, int longitudeBands = 32);

        /*!***********************************************************************
        @brief          Creates a straight 3D line segment between two world-space points.
        @param          from          Start point of the line segment.
        @param          to            End point of the line segment.
        @param          color         Per-vertex RGBA color applied to both endpoints.
        @return         Mesh instance populated with line segment geometry.
        *************************************************************************/
        static Mesh CreateLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color = glm::vec4(1.0f));

        /*!***********************************************************************
        @brief          Creates an XZ-plane ground grid of line segments on y=0, with
                        accent lines every majorEveryUnits units. The grid extends
                        +/-halfExtent units in both X and Z around the origin.
        @param          halfExtent       Distance from origin to edge along X and Z.
        @param          majorEveryUnits  Draw a brighter line on multiples of this value.
        @return         Mesh instance populated with grid line geometry and per-vertex colors.
        *************************************************************************/
        static Mesh CreateGrid(int halfExtent = 60, int majorEveryUnits = 10);

        /*!***********************************************************************
        @brief          Creates infinite-viewport X and Z axis lines through the origin
                        on y=0, rendered as line segments with distinct colors
                        (X = red, Z = blue) extending +/-axisLength units.
        @param          axisLength    Half-length of each axis in world units.
        @return         Mesh instance populated with axis line geometry and per-vertex colors.
        *************************************************************************/
        static Mesh CreateGridAxes(float axisLength);

        /*!***********************************************************************
        @brief          Binds the underlying GPU vertex array for rendering.
        *************************************************************************/
        void Bind() const;

        /*!***********************************************************************
        @brief          Unbinds the underlying GPU vertex array.
        *************************************************************************/
        void Unbind() const;

        /*!***********************************************************************
        @brief          Returns the total number of indices in the mesh index buffer.
        @return         Count of indices as an unsigned 32-bit integer.
        *************************************************************************/
        uint32_t GetIndexCount() const { return static_cast<uint32_t>(indices_.size()); }

        /*!***********************************************************************
        @brief          Returns the total number of vertices in the mesh vertex buffer.
        @return         Count of vertices as an unsigned 32-bit integer.
        *************************************************************************/
        uint32_t GetVertexCount() const { return static_cast<uint32_t>(vertices_.size()); }

        /*!***********************************************************************
        @brief          Retrieves a const reference to the mesh vertex attribute vector.
        @return         Const reference to the vector of Vertex objects.
        *************************************************************************/
        const std::vector<Vertex>& GetVertices() const { return vertices_; }

        /*!***********************************************************************
        @brief          Retrieves a const reference to the mesh index vector.
        @return         Const reference to the vector of uint32_t indices.
        *************************************************************************/
        const std::vector<uint32_t>& GetIndices() const { return indices_; }

        /*!***********************************************************************
        @brief          Retrieves a const reference to the mesh name identifier.
        @return         Const reference to the name string.
        *************************************************************************/
        const std::string& GetName() const { return name_; }

    private:
        std::vector<Vertex> vertices_;
        std::vector<uint32_t> indices_;
        std::string name_;
        mutable std::shared_ptr<OpenGL::VertexArray> vertexArray_;
    };
}
