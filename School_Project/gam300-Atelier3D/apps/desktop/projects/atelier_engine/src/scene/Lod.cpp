/* !
@file               Lod.cpp
@author             w.kyele@digipen.edu [100%]
@date               03/11/2026
@brief
    Implements ChooseLodLevel: the level of detail for a distance, with a
    margin so a model does not flicker between two levels.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/scene/Lod.hpp"

namespace Engine::Scene
{
    namespace
    {
        // The coarsest level the object has whose distance (times factor) the camera is past
        int LevelAt(float distance, const std::array<float, 2>& starts, const std::array<bool, 2>& present,
                    float factor)
        {
            int level = 0;
            for (int i = 0; i < 2; ++i)
            {
                if (present[i] && distance >= starts[i] * factor)
                {
                    level = i + 1;
                }
            }
            return level;
        }
    }

    int ChooseLodLevel(float distance, int current, const std::array<float, 2>& starts,
                       const std::array<bool, 2>& present, float hysteresis)
    {
        const int farther = LevelAt(distance, starts, present, 1.0f);
        const bool currentExists = current == 0 || (current > 0 && current < kLodLevels && present[current - 1]);
        if (!currentExists || farther > current)
        {
            return farther;
        }
        // Back to a finer level only once the camera is clearly inside its switch distance
        const int nearer = LevelAt(distance, starts, present, 1.0f - hysteresis);
        return nearer < current ? nearer : current;
    }

    float ScreenSize(float radius, float distance, float projectionYY)
    {
        if (distance <= radius)
        {
            return 1000.0f; // the camera is inside it
        }
        return radius * projectionYY / distance;
    }

    int ChooseLodLevelForScreenSize(float screenSize, int current, int levels)
    {
        // The smaller on screen, the "farther": 1 / size grows as a model shrinks, like a distance
        const float farness = screenSize > 0.0f ? 1.0f / screenSize : 1e9f;
        const std::array<float, 2> starts{1.0f / kLodScreenSizes[0], 1.0f / kLodScreenSizes[1]};
        return ChooseLodLevel(farness, current, starts, {levels > 1, levels > 2});
    }
}
