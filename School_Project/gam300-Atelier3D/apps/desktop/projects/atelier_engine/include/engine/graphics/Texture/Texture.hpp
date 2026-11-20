/* !
@file    	    Texture.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    This file provides texture-related enumerations, the TextureSpec
    configuration structure, the abstract Texture base class defining a GPU-
    backed 2D texture interface, and factory functions for creating texture
    instances.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

/*
 * FUTURE: atlas packing helpers.
 */

#include <cstdint>
#include <memory>
#include <string>

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          Enumeration of supported pixel formats for GPU textures.
    *************************************************************************/
    enum class TextureFormat : std::uint8_t
    {
        RGBA8,
        RGBA16F,
        R8,
        Depth24Stencil8,
    };

    /*!***********************************************************************
    @brief          Enumeration of texture coordinate wrapping modes.
    *************************************************************************/
    enum class TextureWrap : std::uint8_t
    {
        Repeat,
        ClampToEdge,
        MirroredRepeat,
    };

    /*!***********************************************************************
    @brief          Enumeration of texture minification and magnification filtering modes.
    *************************************************************************/
    enum class TextureFilter : std::uint8_t
    {
        Nearest,
        Linear,
        NearestMipmapNearest,
        LinearMipmapLinear,
    };

    /*!***********************************************************************
    @brief          Aggregated configuration parameters for texture creation and allocation.
    *************************************************************************/
    struct TextureSpec
    {
        uint32_t width = 0;
        uint32_t height = 0;
        TextureFormat format = TextureFormat::RGBA8;
        TextureWrap wrapS = TextureWrap::ClampToEdge;
        TextureWrap wrapT = TextureWrap::ClampToEdge;
        TextureFilter minFilter = TextureFilter::LinearMipmapLinear;
        TextureFilter magFilter = TextureFilter::Linear;
        bool hasMipmaps = true;
    };

    /*!***********************************************************************
    @brief          Abstract base class defining the interface for a 2D GPU texture.
    *************************************************************************/
    class Texture
    {
    public:
        /*!***********************************************************************
        @brief          Default constructor.
        *************************************************************************/
        Texture() = default;

        /*!***********************************************************************
        @brief          Virtual destructor for polymorphic destruction of derived texture types.
        *************************************************************************/
        virtual ~Texture() = default;

        /*!***********************************************************************
        @brief          Allocates a GPU texture with given dimensions and optionally uploads initial pixel data.
        @param          width         Width of the texture in pixels.
        @param          height        Height of the texture in pixels.
        @param          spec          Texture specification describing format and sampler state.
        @param          data          Optional pointer to initial pixel data to upload (may be nullptr).
        @return         True if allocation and optional upload succeeded, false otherwise.
        *************************************************************************/
        virtual bool Create(uint32_t width, uint32_t height, const TextureSpec& spec, const void* data = nullptr) = 0;

        /*!***********************************************************************
        @brief          Binds the texture to the specified texture unit slot for sampling.
        @param          slot          Zero-based index of the texture unit to bind to.
        *************************************************************************/
        virtual void Bind(uint32_t slot = 0) const = 0;

        /*!***********************************************************************
        @brief          Unbinds the texture from its currently bound texture unit.
        *************************************************************************/
        virtual void Unbind() const = 0;

        /*!***********************************************************************
        @brief          Returns the width of the texture in pixels.
        @return         Texture width in pixels.
        *************************************************************************/
        uint32_t GetWidth() const { return width_; }

        /*!***********************************************************************
        @brief          Returns the height of the texture in pixels.
        @return         Texture height in pixels.
        *************************************************************************/
        uint32_t GetHeight() const { return height_; }

        /*!***********************************************************************
        @brief          Returns the name identifier of this texture.
        @return         Const reference to the texture name string.
        *************************************************************************/
        const std::string& GetName() const { return name_; }

    protected:
        uint32_t width_ = 0;
        uint32_t height_ = 0;
        TextureSpec spec_{};
        std::string name_;
    };

    /*!***********************************************************************
    @brief          Factory function that creates a new empty Texture instance with an optional name
                    (the OpenGL implementation); allocate and upload it with Create.
    @param          name          Optional name identifier for the texture.
    @return         Unique pointer owning the newly created Texture instance.
    *************************************************************************/
    std::unique_ptr<Texture> CreateTexture(const std::string& name = "");
}
