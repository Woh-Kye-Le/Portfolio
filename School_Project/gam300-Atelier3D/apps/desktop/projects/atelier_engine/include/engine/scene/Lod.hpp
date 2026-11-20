/* !
@file               Lod.hpp
@author             w.kyele@digipen.edu [100%]
@date               03/11/2026
@brief
    Level of detail: which version of a model to draw. LOD 0 is the model as
    imported; LOD 1 and LOD 2 are the simpler versions the engine makes of it
    when it loads it (MeshSimplifier.hpp). The smaller the model is on screen,
    the simpler the version drawn.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Standard Library Headers
#include <array>

namespace Engine::Scene
{
    // LOD 0 (the object's own model), LOD 1 and LOD 2
    inline constexpr int kLodLevels = 3;

    // How far inside a switch distance the camera has to come back before the finer model returns (10%: a model
    // switching at 10 m goes back to the finer one under 9 m), so a camera resting on a switch distance does not
    // make the model flicker between two levels
    inline constexpr float kLodHysteresis = 0.1f;

    // How much of the view's height a model fills when LOD 1 and LOD 2 take over: below 40% of the view the
    // half-detail version is drawn, below 15% the fifth-detail one. On screen size rather than distance, so a
    // coin and a building both switch when their detail stops showing.
    inline constexpr std::array<float, 2> kLodScreenSizes{0.40f, 0.15f};

    /*!***********************************************************************
    @brief          How much of the view's height a model fills.
    @param          radius        Of the sphere around the model, in scene units (its scale applied).
    @param          distance      From the camera to the model's centre.
    @param          projectionYY  The camera projection's [1][1] (1 / tan of half the vertical field of view).
    @return         Its diameter over the view's height at that distance (1 = fills the view; large when the camera
                    is inside it).
    *************************************************************************/
    float ScreenSize(float radius, float distance, float projectionYY);

    /*!***********************************************************************
    @brief          The level to draw a model at for its size on screen (kLodScreenSizes), with the kLodHysteresis
                    margin before a finer level comes back.
    @param          screenSize    See ScreenSize.
    @param          current       The level drawn last frame.
    @param          levels        How many versions the model has (1 when it has no simpler ones, up to kLodLevels).
    *************************************************************************/
    int ChooseLodLevelForScreenSize(float screenSize, int current, int levels);

    /*!***********************************************************************
    @brief          Picks the level of detail to draw.
    @param          distance      From the camera to the object, in scene units.
    @param          current       The level drawn last frame (0 the first time).
    @param          starts        The distances LOD 1 and LOD 2 are drawn from.
    @param          present       Which of LOD 1 and LOD 2 the object has (a level it does not have is skipped).
    @param          hysteresis    See kLodHysteresis.
    @return         0, 1 or 2: the coarsest level whose distance the camera is past, except that a coarser level
                    drawn last frame stays until the camera comes hysteresis closer than its distance.
    *************************************************************************/
    int ChooseLodLevel(float distance, int current, const std::array<float, 2>& starts,
                       const std::array<bool, 2>& present, float hysteresis = kLodHysteresis);
}
