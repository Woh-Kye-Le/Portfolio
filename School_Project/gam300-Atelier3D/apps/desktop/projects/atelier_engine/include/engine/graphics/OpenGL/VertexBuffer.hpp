/* !
@file    	    VertexBuffer.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Provides the VertexBuffer class which encapsulates an OpenGL vertex buffer
    object (VBO) for storing vertex data on the GPU. It supports creation,
    updates, binding, and querying of buffer properties for rendering
    operations.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Engine::Graphics::OpenGL
{
    /*!***********************************************************************
    @brief          Encapsulates an OpenGL vertex buffer object for GPU vertex storage.
    *************************************************************************/
    class VertexBuffer
    {
    public:
        /*!***********************************************************************
        @brief          Default constructor that creates an uninitialized VertexBuffer.
        *************************************************************************/
        VertexBuffer();
        /*!***********************************************************************
        @brief          Destructor that deletes the underlying OpenGL buffer if it exists.
        *************************************************************************/
        ~VertexBuffer();

        /*!***********************************************************************
        @brief          Deleted copy constructor (GPU resources have a single owner).
        *************************************************************************/
        VertexBuffer(const VertexBuffer&) = delete;
        /*!***********************************************************************
        @brief          Deleted copy assignment operator (GPU resources have a single owner).
        @return         Reference to this VertexBuffer.
        *************************************************************************/
        VertexBuffer& operator=(const VertexBuffer&) = delete;

        /*!***********************************************************************
        @brief          Creates and initializes the vertex buffer with raw data.
        @param          data          Pointer to the raw vertex data to upload.
        @param          sizeBytes     Size of the data in bytes.
        *************************************************************************/
        void Create(const void* data, size_t sizeBytes);
        /*!***********************************************************************
        @brief          Creates and initializes the vertex buffer from a vector of typed elements.
        @param          data          Vector containing the vertex data to upload.
        *************************************************************************/
        template <typename T> void Create(const std::vector<T>& data) { Create(data.data(), data.size() * sizeof(T)); }

        /*!***********************************************************************
        @brief          Updates a region of the vertex buffer with new data.
        @param          data          Pointer to the new data to upload.
        @param          sizeBytes     Size of the new data in bytes.
        @param          offsetBytes   Byte offset from the start of the buffer where updates begin.
        *************************************************************************/
        void Update(const void* data, size_t sizeBytes, size_t offsetBytes = 0) const;
        /*!***********************************************************************
        @brief          Binds the vertex buffer to the OpenGL context for rendering.
        *************************************************************************/
        void Bind() const;
        /*!***********************************************************************
        @brief          Unbinds the vertex buffer from the OpenGL context.
        *************************************************************************/
        void Unbind() const;

        /*!***********************************************************************
        @brief          Returns the underlying OpenGL buffer ID.
        @return         The OpenGL renderer ID of the vertex buffer.
        *************************************************************************/
        uint32_t GetRendererID() const { return rendererID_; }
        /*!***********************************************************************
        @brief          Returns the total size of the buffer data in bytes.
        @return         Size of the buffer in bytes.
        *************************************************************************/
        size_t GetSizeBytes() const { return sizeBytes_; }

    private:
        uint32_t rendererID_ = 0;
        size_t sizeBytes_ = 0;
    };
}
