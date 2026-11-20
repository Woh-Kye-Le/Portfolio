/* !
@file    	    Environment.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		30/09/2026
@brief
    Implements Environment::Apply, which uploads the sun, sky, fog and up to 8
    point lights to a shader.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/graphics/Renderer/Environment.hpp"

#include "engine/graphics/Shader/Shader.hpp"

namespace Engine::Renderer
{
    /*!***********************************************************************
    @brief          Sets the lighting uniforms a lit shader reads (u_Environment, u_SunDirection, u_SunColor,
                    u_SkyColor, u_GroundColor, u_Ambient, u_FogColor, u_FogDensity, u_CameraPos and the point-light
                    arrays): the sun and ambient light from lighting, the haze from this environment. Shaders without
                    them ignore them.
    @param          shader          Shader to upload the uniforms to.
    @param          lighting        Lights the sun, ambient light and point lights come from.
    @param          cameraPosition  World-space position of the camera, used for the fog and sky shading.
    *************************************************************************/
    void Environment::Apply(const Graphics::Shader& shader, const Graphics::LightingContext& lighting,
                            const glm::vec3& cameraPosition) const
    {
        // The sun is the scene's directional light (if there is one); the sky/haze shading uses it too
        const Graphics::Light* sun = lighting.FindSun();
        shader.SetInt("u_Environment", enabled ? 1 : 0);
        // Towards the sun: a light's direction is the way its light travels
        shader.SetFloat3("u_SunDirection", sun ? -glm::normalize(sun->direction) : glm::vec3(0.0f, 1.0f, 0.0f));
        shader.SetFloat3("u_SunColor", sun ? sun->color * sun->intensity : glm::vec3(0.0f));
        shader.SetFloat3("u_SkyColor", lighting.skyColor);
        shader.SetFloat3("u_GroundColor", lighting.groundColor);
        shader.SetFloat("u_Ambient", lighting.ambientStrength);
        shader.SetFloat3("u_FogColor", horizonColor);
        shader.SetFloat("u_FogDensity", fogDensity);
        shader.SetFloat3("u_CameraPos", cameraPosition);

        // Collect up to LightingContext::MaxLightsDefault point lights in order and
        // write each slot individually via the Shader's indexed-uniform names. Any
        // slot beyond the active count is cleared to zero so unused array slots
        // contribute nothing even if someone reads past u_PointLightCount.
        constexpr std::uint32_t kMax = Graphics::LightingContext::MaxLightsDefault;
        glm::vec3 positions[kMax]{};
        glm::vec3 colors[kMax]{};
        float ranges[kMax]{};
        std::uint32_t count = 0;
        for (const Graphics::Light& light : lighting.lights)
        {
            if (light.type != Graphics::LightType::Point)
            {
                continue;
            }
            if (count >= kMax)
            {
                break;
            }
            positions[count] = light.position;
            colors[count] = light.color * light.intensity;
            ranges[count] = light.range;
            ++count;
        }
        shader.SetInt("u_PointLightCount", static_cast<int>(count));

        // GLSL arrays are set one element at a time by name, e.g. "u_PointLightPos[2]"
        auto MakeIndexed = [](const std::string& base, std::uint32_t index)
        { return base + "[" + std::to_string(index) + "]"; };
        for (std::uint32_t i = 0; i < kMax; ++i)
        {
            shader.SetFloat3(MakeIndexed("u_PointLightPos", i), positions[i]);
            shader.SetFloat3(MakeIndexed("u_PointLightColor", i), colors[i]);
            shader.SetFloat(MakeIndexed("u_PointLightRange", i), ranges[i]);
        }
    }
}
