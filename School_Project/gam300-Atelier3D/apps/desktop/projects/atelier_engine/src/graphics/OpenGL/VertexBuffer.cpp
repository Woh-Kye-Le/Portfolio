/* !
@file    	    VertexBuffer.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Implements the VertexBuffer class methods that manage an OpenGL vertex
    buffer object, including construction, destruction, buffer creation, data
    updates, and binding operations for GPU vertex data storage.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Graphics Headers
#include "engine/graphics/OpenGL/VertexBuffer.hpp"

// Standard Library Headers
#include <GL/glew.h>

namespace Engine::Graphics::OpenGL
{
    /*!***********************************************************************
    @brief          Default constructor that creates an uninitialized VertexBuffer with no GPU resource.
    *************************************************************************/
    VertexBuffer::VertexBuffer() = default;

    /*!***********************************************************************
    @brief          Destructor that deletes the underlying OpenGL vertex buffer if it has been created.
    *************************************************************************/
    VertexBuffer::~VertexBuffer()
    {
        if (rendererID_ != 0)
        {
            glDeleteBuffers(1, &rendererID_);
            rendererID_ = 0;
        }
    }

    /*!***********************************************************************
    @brief          Creates an OpenGL vertex buffer and uploads raw vertex data to GPU memory as static draw.
    @param          data          Pointer to the raw vertex data bytes to upload to the GPU.
    @param          sizeBytes     Total size of the vertex data in bytes.
    *************************************************************************/
    void VertexBuffer::Create(const void* data, size_t sizeBytes)
    {
        if (rendererID_ == 0)
        {
            glGenBuffers(1, &rendererID_);
        }
        sizeBytes_ = sizeBytes;
        glBindBuffer(GL_ARRAY_BUFFER, rendererID_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeBytes), data, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    /*!***********************************************************************
    @brief          Updates a region of existing vertex buffer data on the GPU using glBufferSubData.
    @param          data          Pointer to the new raw data bytes to write into the buffer.
    @param          sizeBytes     Number of bytes to update starting from the offset.
    @param          offsetBytes   Byte offset from the beginning of the buffer where the update starts.
    *************************************************************************/
    void VertexBuffer::Update(const void* data, size_t sizeBytes, size_t offsetBytes) const
    {
        if (rendererID_ == 0)
        {
            return;
        }
        glBindBuffer(GL_ARRAY_BUFFER, rendererID_);
        glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offsetBytes), static_cast<GLsizeiptr>(sizeBytes), data);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    /*!***********************************************************************
    @brief          Binds this vertex buffer to the OpenGL GL_ARRAY_BUFFER target for rendering.
    *************************************************************************/
    void VertexBuffer::Bind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, rendererID_);
    }

    /*!***********************************************************************
    @brief          Unbinds the current vertex buffer by binding zero to GL_ARRAY_BUFFER.
    *************************************************************************/
    void VertexBuffer::Unbind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}
