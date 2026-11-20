/* !
@file    	    VertexArray.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Provides the VertexArray class which encapsulates an OpenGL vertex array
    object (VAO) that combines a VertexBuffer and IndexBuffer with a predefined
    vertex attribute layout. It handles creation from mesh data, binding, and
    access to the underlying buffer objects for rendering.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/graphics/Mesh/Vertex.hpp"
#include "engine/graphics/OpenGL/IndexBuffer.hpp"
#include "engine/graphics/OpenGL/VertexBuffer.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace Engine::Graphics::OpenGL
{
    /*!***********************************************************************
    @brief          Encapsulates an OpenGL vertex array object combining vertex and index buffers with attribute layout.
    *************************************************************************/
    class VertexArray
    {
    public:
        /*!***********************************************************************
        @brief          Default constructor that creates an uninitialized VertexArray.
        *************************************************************************/
        VertexArray();
        /*!***********************************************************************
        @brief          Destructor that deletes the underlying OpenGL vertex array if it exists.
        *************************************************************************/
        ~VertexArray();

        /*!***********************************************************************
        @brief          Deleted copy constructor (GPU resources have a single owner).
        *************************************************************************/
        VertexArray(const VertexArray&) = delete;
        /*!***********************************************************************
        @brief          Deleted copy assignment operator (GPU resources have a single owner).
        @return         Reference to this VertexArray.
        *************************************************************************/
        VertexArray& operator=(const VertexArray&) = delete;

        /*!***********************************************************************
        @brief          Creates the vertex array from mesh vertex and index data, setting up the default vertex layout.
        @param          vertices      Vector of Vertex structures containing mesh vertex data.
        @param          indices       Vector of uint32_t indices defining the mesh face topology.
        *************************************************************************/
        void Create(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
        /*!***********************************************************************
        @brief          Binds the vertex array to the OpenGL context for rendering.
        *************************************************************************/
        void Bind() const;
        /*!***********************************************************************
        @brief          Unbinds the vertex array from the OpenGL context.
        *************************************************************************/
        void Unbind() const;

        /*!***********************************************************************
        @brief          Returns the underlying OpenGL vertex array ID.
        @return         The OpenGL renderer ID of the vertex array object.
        *************************************************************************/
        uint32_t GetRendererID() const { return rendererID_; }
        /*!***********************************************************************
        @brief          Returns the number of indices in the attached index buffer, or zero if none.
        @return         Number of indices available for indexed drawing.
        *************************************************************************/
        uint32_t GetIndexCount() const { return indexBuffer_ ? indexBuffer_->GetCount() : 0; }
        /*!***********************************************************************
        @brief          Returns a shared reference to the attached VertexBuffer object.
        @return         Const reference to the shared_ptr holding the VertexBuffer.
        *************************************************************************/
        const std::shared_ptr<VertexBuffer>& GetVertexBuffer() const { return vertexBuffer_; }
        /*!***********************************************************************
        @brief          Returns a shared reference to the attached IndexBuffer object.
        @return         Const reference to the shared_ptr holding the IndexBuffer.
        *************************************************************************/
        const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const { return indexBuffer_; }

    private:
        /*!***********************************************************************
        @brief          Configures the default vertex attribute pointer layout for position, normal, texCoord, and
                        color.
        *************************************************************************/
        void SetDefaultVertexLayout() const;

        uint32_t rendererID_ = 0;
        std::shared_ptr<VertexBuffer> vertexBuffer_;
        std::shared_ptr<IndexBuffer> indexBuffer_;
    };
}
