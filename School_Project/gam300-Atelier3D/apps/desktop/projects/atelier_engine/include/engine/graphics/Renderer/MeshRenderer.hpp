/* !
@file    	    MeshRenderer.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		28/09/2026
@brief
    Declares MeshRenderer, which draws a mesh with a material and a model
    matrix.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/graphics/Camera/Camera.hpp"
#include "engine/graphics/Material/Material.hpp"
#include "engine/graphics/Mesh/Mesh.hpp"
#include "engine/graphics/Renderer/RenderAPI.hpp"

// Standard Library Headers
#include <cstdint>

namespace Engine::Renderer
{
    /*!***********************************************************************
    @brief          Draws one mesh with one material. It keeps no state of its own, so everything is static.
    *************************************************************************/
    class MeshRenderer
    {
    public:
        /*!***********************************************************************
        @brief          Default constructor.
        *************************************************************************/
        MeshRenderer() = default;
        /*!***********************************************************************
        @brief          Deleted copy constructor (all static API).
        *************************************************************************/
        MeshRenderer(const MeshRenderer&) = delete;
        /*!***********************************************************************
        @brief          Deleted copy assignment operator (all static API).
        @return         Reference to this MeshRenderer.
        *************************************************************************/
        MeshRenderer& operator=(const MeshRenderer&) = delete;

        /*!***********************************************************************
        @brief          Draws one mesh with one material: binds the shader, sets the common uniforms
                        (u_ViewProjection, u_Model, u_NormalMatrix), calls material.Bind(), binds the mesh, issues
                        DrawIndexed, then unbinds in reverse.
        @param          mesh           Mesh to draw.
        @param          material       Material (and shader) the mesh is drawn with.
        @param          model          Object's full model matrix, e.g.
                                       Scene::ToGraphicsTransform(...).ComposeModelMatrix().
        @param          camera         Camera the frame is seen through.
        @param          primitiveType  Primitive assembly type; defaults to triangles, pass Lines for
                                       wireboxes and lines.
        *************************************************************************/
        static void Draw(const Graphics::Mesh& mesh, const Graphics::Material& material, const glm::mat4& model,
                         const Graphics::Camera& camera,
                         Graphics::PrimitiveType primitiveType = Graphics::PrimitiveType::Triangles);

        /*!***********************************************************************
        @brief          Draws one submesh run of an already-uploaded mesh: same
                        sequence as Draw, but the indexed call starts at firstIndex and
                        covers indexCount indices. This is how a multi-material mesh is
                        drawn one material at a time.
        @param          mesh           Mesh holding the run's geometry.
        @param          material       Material of this submesh.
        @param          model          Object's full model matrix.
        @param          camera         Camera the frame is seen through.
        @param          firstIndex     Index inside the mesh's index buffer the run starts at.
        @param          indexCount     Number of indices in the run.
        @param          primitiveType  Primitive assembly type; defaults to triangles.
        *************************************************************************/
        static void DrawRange(const Graphics::Mesh& mesh, const Graphics::Material& material, const glm::mat4& model,
                              const Graphics::Camera& camera, std::uint32_t firstIndex, std::uint32_t indexCount,
                              Graphics::PrimitiveType primitiveType = Graphics::PrimitiveType::Triangles);
    };
}
