/* !
@file    	    IndexBuffer.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Provides the IndexBuffer class which encapsulates an OpenGL element buffer
    object (EBO) for storing vertex index data on the GPU. It supports creation,
    binding, and querying of index count and buffer identity for indexed
    rendering.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <cstdint>
#include <vector>

namespace Engine::Graphics::OpenGL
{
    /*!***********************************************************************
    @brief          Encapsulates an OpenGL element buffer object for GPU index storage.
    *************************************************************************/
    class IndexBuffer
    {
    public:
        /*!***********************************************************************
        @brief          Default constructor that creates an uninitialized IndexBuffer.
        *************************************************************************/
        IndexBuffer();
        /*!***********************************************************************
        @brief          Destructor that deletes the underlying OpenGL buffer if it exists.
        *************************************************************************/
        ~IndexBuffer();

        /*!***********************************************************************
        @brief          Deleted copy constructor (GPU resources have a single owner).
        *************************************************************************/
        IndexBuffer(const IndexBuffer&) = delete;
        /*!***********************************************************************
        @brief          Deleted copy assignment operator (GPU resources have a single owner).
        @return         Reference to this IndexBuffer.
        *************************************************************************/
        IndexBuffer& operator=(const IndexBuffer&) = delete;

        /*!***********************************************************************
        @brief          Creates and initializes the index buffer from a raw uint32_t array.
        @param          indices       Pointer to the raw index data array.
        @param          count         Number of indices in the data array.
        *************************************************************************/
        void Create(const uint32_t* indices, uint32_t count);
        /*!***********************************************************************
        @brief          Creates and initializes the index buffer from a vector of uint32_t indices.
        @param          indices       Vector containing the index data to upload.
        *************************************************************************/
        void Create(const std::vector<uint32_t>& indices);
        /*!***********************************************************************
        @brief          Binds the index buffer to the OpenGL context for indexed rendering.
        *************************************************************************/
        void Bind() const;
        /*!***********************************************************************
        @brief          Unbinds the index buffer from the OpenGL context.
        *************************************************************************/
        void Unbind() const;

        /*!***********************************************************************
        @brief          Returns the underlying OpenGL buffer ID.
        @return         The OpenGL renderer ID of the index buffer.
        *************************************************************************/
        uint32_t GetRendererID() const { return rendererID_; }
        /*!***********************************************************************
        @brief          Returns the number of indices stored in the buffer.
        @return         Number of uint32_t indices in the buffer.
        *************************************************************************/
        uint32_t GetCount() const { return count_; }

    private:
        uint32_t rendererID_ = 0;
        uint32_t count_ = 0;
    };
}
