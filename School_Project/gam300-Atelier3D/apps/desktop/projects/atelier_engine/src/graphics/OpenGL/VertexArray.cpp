/* !
@file    	    VertexArray.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Implements the VertexArray class methods that manage an OpenGL vertex array
    object, including construction, destruction, creation from mesh vertex and
    index data, the default vertex attribute layout (position, normal, texCoord,
    color), and binding operations for rendering.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Graphics Headers
#include "engine/graphics/OpenGL/VertexArray.hpp"

// Standard Library Headers
#include <GL/glew.h>

#include <cstddef>

namespace Engine::Graphics::OpenGL
{
    namespace
    {
        // OpenGL takes an attribute's byte offset into the vertex buffer as a pointer
        const void* AttributeOffset(std::size_t bytes)
        {
            return reinterpret_cast<const void*>(bytes); // NOLINT(performance-no-int-to-ptr): required by OpenGL
        }
    }

    /*!***********************************************************************
    @brief          Default constructor that creates an uninitialized VertexArray with no GPU resource.
    *************************************************************************/
    VertexArray::VertexArray() = default;

    /*!***********************************************************************
    @brief          Destructor that deletes the underlying OpenGL vertex array if it has been created.
    *************************************************************************/
    VertexArray::~VertexArray()
    {
        if (rendererID_ != 0)
        {
            glDeleteVertexArrays(1, &rendererID_);
            rendererID_ = 0;
        }
    }

    /*!***********************************************************************
    @brief          Creates a complete vertex array from mesh data, generating a vertex buffer, index buffer, and
                    default vertex layout.
    @param          vertices      Vector of Vertex structures with position, normal, texCoord, and color to upload to
                                  the vertex buffer.
    @param          indices       Vector of uint32_t face indices to upload to the index buffer.
    *************************************************************************/
    void VertexArray::Create(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
    {
        if (rendererID_ == 0)
        {
            glGenVertexArrays(1, &rendererID_);
        }
        Bind();

        vertexBuffer_ = std::make_shared<VertexBuffer>();
        vertexBuffer_->Create(vertices);
        vertexBuffer_->Bind();

        indexBuffer_ = std::make_shared<IndexBuffer>();
        indexBuffer_->Create(indices);
        indexBuffer_->Bind();

        SetDefaultVertexLayout();

        Unbind();
        vertexBuffer_->Unbind();
        indexBuffer_->Unbind();
    }

    /*!***********************************************************************
    @brief          Binds this vertex array to the OpenGL context so subsequent draw calls use its stored state.
    *************************************************************************/
    void VertexArray::Bind() const
    {
        glBindVertexArray(rendererID_);
    }

    /*!***********************************************************************
    @brief          Unbinds the current vertex array by binding vertex array zero.
    *************************************************************************/
    void VertexArray::Unbind() const
    {
        glBindVertexArray(0);
    }

    /*!***********************************************************************
    @brief          Configures the standard vertex attribute pointers for position, normal, texCoord, color and the
                    skinning joints and weights, based on the Vertex struct layout.
    *************************************************************************/
    void VertexArray::SetDefaultVertexLayout() const
    {
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), AttributeOffset(offsetof(Vertex, position)));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), AttributeOffset(offsetof(Vertex, normal)));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), AttributeOffset(offsetof(Vertex, texCoord)));

        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), AttributeOffset(offsetof(Vertex, color)));

        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), AttributeOffset(offsetof(Vertex, joints)));

        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), AttributeOffset(offsetof(Vertex, weights)));
    }
}
