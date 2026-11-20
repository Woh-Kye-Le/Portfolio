/* !
@file    	    Texture.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    This file contains the Texture factory functions, which create the OpenGL
    implementation (OpenGL::TextureGL). Images are decoded by
    Assets::LoadTextureFromFile and uploaded with Texture::Create.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Graphics Headers
#include "engine/graphics/Texture/Texture.hpp"
#include "engine/graphics/OpenGL/TextureGL.hpp"

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          Creates a new empty Texture (no GPU storage until Create is called).
    @param          name          Optional name identifier for the created texture.
    @return         Unique pointer to the created Texture.
    *************************************************************************/
    std::unique_ptr<Texture> CreateTexture(const std::string& name)
    {
        return std::make_unique<OpenGL::TextureGL>(name);
    }
}
