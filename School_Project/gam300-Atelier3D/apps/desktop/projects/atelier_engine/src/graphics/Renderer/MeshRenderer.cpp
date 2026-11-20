/* !
@file    	    MeshRenderer.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		28/09/2026
@brief
    Implements MeshRenderer::Draw: sets the camera, model and normal matrices,
    binds the material and mesh, and issues the indexed draw call.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Graphics Headers
#include "engine/graphics/Renderer/MeshRenderer.hpp"

// Third-Party Headers
#include <glm/glm.hpp>

namespace Engine::Renderer
{
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
    void MeshRenderer::Draw(const Graphics::Mesh& mesh, const Graphics::Material& material, const glm::mat4& model,
                            const Graphics::Camera& camera, Graphics::PrimitiveType primitiveType)
    {
        DrawRange(mesh, material, model, camera, 0, mesh.GetIndexCount(), primitiveType);
    }

    /*!***********************************************************************
    @brief          Draws one submesh run of an already-uploaded mesh: same sequence as Draw, but the indexed
                    call starts at firstIndex and covers indexCount indices. This is how a multi-material mesh
                    is drawn one material at a time.
    @param          mesh           Mesh holding the run's geometry.
    @param          material       Material of this submesh.
    @param          model          Object's full model matrix.
    @param          camera         Camera the frame is seen through.
    @param          firstIndex     Index inside the mesh's index buffer the run starts at.
    @param          indexCount     Number of indices in the run.
    @param          primitiveType  Primitive assembly type; defaults to triangles.
    *************************************************************************/
    void MeshRenderer::DrawRange(const Graphics::Mesh& mesh, const Graphics::Material& material, const glm::mat4& model,
                                 const Graphics::Camera& camera, std::uint32_t firstIndex, std::uint32_t indexCount,
                                 Graphics::PrimitiveType primitiveType)
    {
        const Graphics::Shader* const shader = material.GetShader();
        if (!shader)
        {
            return;
        }

        // inverse-transpose so normals stay correct when the object is scaled unevenly
        const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

        shader->Bind();
        shader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
        shader->SetMat4("u_Model", model);
        shader->SetMat3("u_NormalMatrix", normalMatrix);

        material.Bind();
        mesh.Bind();
        // A mirrored object (an odd number of negative scales) has its triangles wound the other way: without this
        // culling would drop its outside and show its inside
        const bool mirrored = glm::determinant(glm::mat3(model)) < 0.0f;
        if (mirrored)
        {
            Graphics::RenderAPI::SetFrontFace(Graphics::FrontFace::CW);
        }
        Graphics::RenderAPI::DrawIndexedRange(firstIndex, indexCount, primitiveType);
        if (mirrored)
        {
            Graphics::RenderAPI::SetFrontFace(Graphics::FrontFace::CCW);
        }
        mesh.Unbind();
        shader->Unbind();
    }
}
