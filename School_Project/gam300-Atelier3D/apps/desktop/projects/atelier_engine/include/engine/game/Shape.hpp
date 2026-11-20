/* !
@file    	    Shape.hpp
@author  	    c.kojat@digipen.edu [70%]
         	    w.kyele@digipen.edu [30%]
@date    		02/10/2026
@brief
    Declares the built-in Shape enum and the conversion to and from the names
    used in scene files.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace Engine::Game
{
    // The built-in shapes an object's renderer can draw (a mesh file, when set, is drawn instead)
    enum class Shape : std::uint8_t
    {
        Quad,
        Circle,
        Cube,
        Pyramid,
        Plane,
        Wirebox,
        Sphere,
        Line
    };

    // The name a shape has in scene files, e.g. "cube"
    const char* ToString(Shape shape);
    // Reads a shape name as scene files write it ("cube", "sphere", ...), ignoring case; nullopt if unknown
    std::optional<Shape> ParseShape(std::string_view name);
}
