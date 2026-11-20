/* !
@file    	    Shape.cpp
@author  	    c.kojat@digipen.edu [70%]
         	    w.kyele@digipen.edu [30%]
@date    		02/10/2026
@brief
    Implements the Shape name table, ToString and the case-insensitive
    ParseShape.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/game/Shape.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <utility>

namespace Engine::Game
{
    namespace
    {
        // The names scene files use for each built-in shape. Keep this in sync with the Shape enum.
        constexpr std::array<std::pair<Shape, const char*>, 8> kShapeNames{{
            {Shape::Quad, "quad"},
            {Shape::Circle, "circle"},
            {Shape::Cube, "cube"},
            {Shape::Pyramid, "pyramid"},
            {Shape::Plane, "plane"},
            {Shape::Wirebox, "wirebox"},
            {Shape::Sphere, "sphere"},
            {Shape::Line, "line"},
        }};
    }

    const char* ToString(Shape shape)
    {
        for (const auto& [value, name] : kShapeNames)
        {
            if (value == shape)
            {
                return name;
            }
        }
        return "cube"; // shouldn't happen unless a new Shape was added without a name above
    }

    // Case doesn't matter, so "Cube" or "CUBE" in a hand-edited file still works
    std::optional<Shape> ParseShape(std::string_view name)
    {
        std::string lower(name);
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (const auto& [value, shapeName] : kShapeNames)
        {
            if (lower == shapeName)
            {
                return value;
            }
        }
        return std::nullopt;
    }
}
