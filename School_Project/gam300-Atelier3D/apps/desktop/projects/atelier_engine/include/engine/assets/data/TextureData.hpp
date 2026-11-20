/* !
@file               TextureData.hpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Declares the CPU-side texture description produced by the texture loader and
    uploaded to the GPU on first use.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Standard Library Headers
#include <cstdint>
#include <vector>

namespace Engine::Assets
{
    /*!***********************************************************************
    @brief          CPU-side RGBA8 texture pixels plus the uploaded GPU texture name.
    *************************************************************************/
    struct TextureData
    {
        int width = 0;
        int height = 0;
        std::vector<std::uint8_t> rgba; // width * height * 4
        std::uint32_t glId = 0;         // OpenGL texture name; 0 = not uploaded yet
    };
}
