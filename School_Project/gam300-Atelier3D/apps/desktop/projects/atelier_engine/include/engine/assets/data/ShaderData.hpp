/* !
@file               ShaderData.hpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Declares the CPU-side shader description produced by the shader loader and
    compiled into an OpenGL program on first use.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Standard Library Headers
#include <cstdint>
#include <string>

namespace Engine::Assets
{
    /*!***********************************************************************
    @brief          CPU-side shader sources plus the compiled OpenGL program name.
    *************************************************************************/
    struct ShaderData
    {
        std::string vertexSource;
        std::string fragmentSource;
        std::uint32_t program = 0;     // OpenGL program name; 0 = fixed-function fallback or not compiled yet
        bool compileAttempted = false; // true after the first compile attempt, success or not
    };
}
