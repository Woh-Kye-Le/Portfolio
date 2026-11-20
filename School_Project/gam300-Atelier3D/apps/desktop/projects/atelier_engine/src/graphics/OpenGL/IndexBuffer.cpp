/* !
@file    	    IndexBuffer.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Implements the IndexBuffer class methods that manage an OpenGL element
    buffer object, including construction, destruction, buffer creation from raw
    arrays or vectors, and binding operations for indexed rendering on the GPU.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Graphics Headers
#include "engine/graphics/OpenGL/IndexBuffer.hpp"

// Standard Library Headers
#include <GL/glew.h>

namespace Engine::Graphics::OpenGL
{
    /*!***********************************************************************
    @brief          Default constructor that creates an uninitialized IndexBuffer with no GPU resource.
    *************************************************************************/
    IndexBuffer::IndexBuffer() = default;

    /*!***********************************************************************
    @brief          Destructor that deletes the underlying OpenGL element buffer if it has been created.
    *************************************************************************/
    IndexBuffer::~IndexBuffer()
    {
        if (rendererID_ != 0)
        {
            glDeleteBuffers(1, &rendererID_);
            rendererID_ = 0;
        }
    }

    /*!***********************************************************************
    @brief          Creates an OpenGL element buffer and uploads raw index data to GPU memory as static draw.
    @param          indices       Pointer to the raw uint32_t index array to upload to the GPU.
    @param          count         Number of uint32_t indices in the array to store.
    *************************************************************************/
    void IndexBuffer::Create(const uint32_t* indices, uint32_t count)
    {
        if (rendererID_ == 0)
        {
            glGenBuffers(1, &rendererID_);
        }
        count_ = count;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rendererID_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(static_cast<std::size_t>(count) * sizeof(uint32_t)), indices,
                     GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    /*!***********************************************************************
    @brief          Creates an OpenGL element buffer and uploads index data from a uint32_t vector.
    @param          indices       Vector of uint32_t indices to upload to GPU memory as static draw.
    *************************************************************************/
    void IndexBuffer::Create(const std::vector<uint32_t>& indices)
    {
        Create(indices.data(), static_cast<uint32_t>(indices.size()));
    }

    /*!***********************************************************************
    @brief          Binds this index buffer to the OpenGL GL_ELEMENT_ARRAY_BUFFER target for indexed rendering.
    *************************************************************************/
    void IndexBuffer::Bind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rendererID_);
    }

    /*!***********************************************************************
    @brief          Unbinds the current index buffer by binding zero to GL_ELEMENT_ARRAY_BUFFER.
    *************************************************************************/
    void IndexBuffer::Unbind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}
