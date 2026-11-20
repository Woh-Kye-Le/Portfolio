/* !
@file    	    Lights.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    This file contains the implementation of LightingContext helper methods,
    which construct directional, point, and spot Light instances with the
    appropriate parameters and append them to the scene's dynamic light list.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*
 * FUTURE: pack lights into shader UBO, distance/angle attenuation, hemispheric ambient.
 *         For now: simple typed AddX helpers.
 */

// Engine Graphics Headers
#include "engine/graphics/Lighting/Lights.hpp"

// Standard Library Headers
#include <algorithm>
#include <utility>

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          Constructs a directional Light and appends it to the active light list.
    @param          direction     Normalized world-space direction vector emitted from the light.
    @param          color         RGB color contribution of the directional light.
    @param          intensity     Scalar multiplier applied to the light color intensity.
    *************************************************************************/
    void LightingContext::AddDirectional(const glm::vec3& direction, const glm::vec3& color, float intensity)
    {
        Light l;
        l.type = LightType::Directional;
        l.direction = direction;
        l.color = color;
        l.intensity = intensity;
        lights.push_back(l);
    }

    /*!***********************************************************************
    @brief          Constructs a point Light and appends it to the active light list.
    @param          position      World-space position of the point light source.
    @param          color         RGB color contribution of the point light.
    @param          intensity     Scalar multiplier applied to the light color intensity.
    @param          range         Maximum falloff distance used for light attenuation.
    *************************************************************************/
    void LightingContext::AddPoint(const glm::vec3& position, const glm::vec3& color, float intensity, float range)
    {
        Light l;
        l.type = LightType::Point;
        l.position = position;
        l.color = color;
        l.intensity = intensity;
        l.range = range;
        lights.push_back(l);
    }

    /*!***********************************************************************
    @brief          Constructs a spot Light and appends it to the active light list.
    @param          position      World-space apex position of the spot light cone.
    @param          direction     Normalized direction vector the spot light cone is aimed toward.
    @param          color         RGB color contribution of the spot light.
    @param          intensity     Scalar multiplier applied to the light color intensity.
    @param          range         Maximum falloff distance used for light attenuation.
    @param          innerConeDeg  Inner cone half-angle in degrees for full-intensity region.
    @param          outerConeDeg  Outer cone half-angle in degrees beyond which the light falls to zero.
    *************************************************************************/
    void LightingContext::AddSpot(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color,
                                  float intensity, float range, float innerConeDeg, float outerConeDeg)
    {
        Light l;
        l.type = LightType::Spot;
        l.position = position;
        l.direction = direction;
        l.color = color;
        l.intensity = intensity;
        l.range = range;
        l.spotInnerConeDeg = innerConeDeg;
        l.spotOuterConeDeg = outerConeDeg;
        lights.push_back(l);
    }

    /*!***********************************************************************
    @brief          Removes all dynamically added lights from the lighting list, keeping ambient settings intact.
    *************************************************************************/
    void LightingContext::Clear()
    {
        lights.clear();
    }

    /*!***********************************************************************
    @brief          Returns the first directional light (the sun), or nullptr if there is none.
    *************************************************************************/
    const Light* LightingContext::FindSun() const
    {
        const auto it = std::find_if(lights.begin(), lights.end(),
                                     [](const Light& light) { return light.type == LightType::Directional; });
        return it == lights.end() ? nullptr : &*it;
    }
}
