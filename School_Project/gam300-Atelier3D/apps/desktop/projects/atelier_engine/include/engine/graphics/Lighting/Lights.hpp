/* !
@file    	    Lights.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    This file provides lighting data structures for the graphics engine. It
    defines the LightType enumeration, the Light struct representing
    directional/point/spot lights, and the LightingContext container that
    manages scene-level ambient, hemispheric, and dynamic light lists for
    rendering.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

/*
 * FUTURE: Directional/Point/Spot lights + scene-owned lighting container. Skeleton structs.
 */

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          Enumeration of supported dynamic light source types.
    *************************************************************************/
    enum class LightType : uint32_t // NOLINT(performance-enum-size): 4 bytes, as Light is laid out for the GPU
    {
        Directional = 0,
        Point = 1,
        Spot = 2,
    };

    /*!***********************************************************************
    @brief          GPU-compatible representation of a single directional, point, or spot light source.
    *************************************************************************/
    struct Light
    {
        LightType type = LightType::Directional;
        glm::vec3 color = glm::vec3(1.0f);
        float intensity = 1.0f;

        glm::vec3 position = glm::vec3(0.0f);
        float range = 10.0f;

        glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
        float spotInnerConeDeg = 30.0f;

        float spotOuterConeDeg = 45.0f;
        uint32_t castsShadow = 0;
        float _pad[2] = {0.0f, 0.0f};
    };

    /*!***********************************************************************
    @brief          Scene-level lighting container holding ambient settings and a dynamic list of lights.
    *************************************************************************/
    struct LightingContext
    {
        static constexpr uint32_t MaxLightsDefault = 8;

        glm::vec3 ambientColor = glm::vec3(0.1f, 0.12f, 0.18f);
        float ambientStrength = 0.25f;

        glm::vec3 skyColor = glm::vec3(0.3f, 0.5f, 0.9f);
        glm::vec3 groundColor = glm::vec3(0.2f, 0.18f, 0.14f);
        float hemiMix = 0.0f;

        std::array<float, 1> _pad = {0.0f};

        std::vector<Light> lights;

        /*!***********************************************************************
        @brief          Adds a directional (infinite) light to the scene lighting list.
        @param          direction     Normalized world-space direction vector pointing from the light.
        @param          color         RGB color of the directional light.
        @param          intensity     Luminous intensity scalar multiplier.
        *************************************************************************/
        void AddDirectional(const glm::vec3& direction, const glm::vec3& color, float intensity = 1.0f);

        /*!***********************************************************************
        @brief          Adds a positional omnidirectional point light to the scene lighting list.
        @param          position      World-space position of the point light.
        @param          color         RGB color of the point light.
        @param          intensity     Luminous intensity scalar multiplier.
        @param          range         Maximum influence distance for attenuation.
        *************************************************************************/
        void AddPoint(const glm::vec3& position, const glm::vec3& color, float intensity = 1.0f, float range = 10.0f);

        /*!***********************************************************************
        @brief          Adds a conical spot light to the scene lighting list with the given cone angles.
        @param          position      World-space apex position of the spot light.
        @param          direction     Normalized direction vector the spot light cone is pointing toward.
        @param          color         RGB color of the spot light.
        @param          intensity     Luminous intensity scalar multiplier.
        @param          range         Maximum influence distance for attenuation.
        @param          innerConeDeg  Inner cone half-angle in degrees (full intensity within this cone).
        @param          outerConeDeg  Outer cone half-angle in degrees (soft falloff begins after inner cone).
        *************************************************************************/
        void AddSpot(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color,
                     float intensity = 1.0f, float range = 10.0f, float innerConeDeg = 25.0f,
                     float outerConeDeg = 35.0f);

        /*!***********************************************************************
        @brief          Removes all lights from the lighting list, preserving ambient settings.
        *************************************************************************/
        void Clear();

        /*!***********************************************************************
        @brief          Returns the scene's sun: the first directional light in the list.
        @return         Pointer to that light, or nullptr if there is no directional light.
        *************************************************************************/
        const Light* FindSun() const;
    };
}
