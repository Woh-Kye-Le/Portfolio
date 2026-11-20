/* !
@file    	    Environment.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		30/09/2026
@brief
    Declares Environment, the default sky, sun light, ambient light and distance
    haze settings applied to the lit shaders.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/graphics/Lighting/Lights.hpp"

#include <glm/glm.hpp>

#include <memory>

namespace Engine::Graphics
{
    class Shader;
    class Texture;
}

namespace Engine::Renderer
{
    /*!***********************************************************************
    @brief          The outdoor surroundings every 3D scene is drawn in: a sky (drawn behind everything) and haze that
                    fades distant things into the horizon for a sense of depth. The lights themselves (the sun, and the
                    sky / ground ambient light) come from a Graphics::LightingContext.
    *************************************************************************/
    struct Environment
    {
        bool enabled = true; // off: flat colours and a plain background

        glm::vec3 zenithColor = glm::vec3(0.20f, 0.38f, 0.68f);  // sky straight up
        glm::vec3 horizonColor = glm::vec3(0.64f, 0.74f, 0.86f); // sky at the horizon, and the haze
        glm::vec3 groundColor = glm::vec3(0.22f, 0.23f, 0.25f);  // below the horizon

        float fogDensity = 0.012f; // haze per unit of distance

        // A picture of the whole sky (a panorama: all the way round across its width, straight up to straight down
        // over its height) drawn instead of the colours; null: the colours. Turned round by skyTurn degrees.
        std::shared_ptr<Graphics::Texture> skyPicture;
        float skyTurn = 0.0f;

        /*!***********************************************************************
        @brief          Sets the lighting uniforms a lit shader reads (u_Environment, u_SunDirection, u_SunColor,
                        u_SkyColor, u_GroundColor, u_Ambient, u_FogColor, u_FogDensity, u_CameraPos and the point-light
                        arrays): the sun and ambient light from lighting, the haze from this environment. Shaders
                        without them ignore them.
        @param          shader          Shader to upload the uniforms to.
        @param          lighting        Lights the sun, ambient light and point lights come from.
        @param          cameraPosition  World-space position of the camera, used for the fog and sky shading.
        *************************************************************************/
        void Apply(const Graphics::Shader& shader, const Graphics::LightingContext& lighting,
                   const glm::vec3& cameraPosition) const;
    };
}
