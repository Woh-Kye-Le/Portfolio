/* !
@file    	    ScreenElementComponent.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		01/11/2026
@brief
    ScreenElementComponent: a thing on the game's screen (2D, over the 3D
    view): a text, a picture, a bar or a button.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace Engine::Scene
{
    // What a screen element shows
    enum class ScreenElementKind : std::uint8_t
    {
        Text,    // its text
        Picture, // one of the game's pictures
        Bar,     // a bar filled as far as a variable is towards its maximum
        Button,  // its text on a rounded button; clicking it runs its object's "when clicked on screen" scripts
    };

    // The point of the screen an element is placed from (so it stays in its corner at any window size)
    enum class ScreenAnchor : std::uint8_t
    {
        TopLeft,
        Top,
        TopRight,
        Left,
        Centre,
        Right,
        BottomLeft,
        Bottom,
        BottomRight,
    };

    inline constexpr std::array<std::pair<ScreenElementKind, std::string_view>, 4> kScreenElementKinds{
        {{ScreenElementKind::Text, "text"},
         {ScreenElementKind::Picture, "picture"},
         {ScreenElementKind::Bar, "bar"},
         {ScreenElementKind::Button, "button"}}};

    inline constexpr std::array<std::pair<ScreenAnchor, std::string_view>, 9> kScreenAnchors{
        {{ScreenAnchor::TopLeft, "top_left"},
         {ScreenAnchor::Top, "top"},
         {ScreenAnchor::TopRight, "top_right"},
         {ScreenAnchor::Left, "left"},
         {ScreenAnchor::Centre, "centre"},
         {ScreenAnchor::Right, "right"},
         {ScreenAnchor::BottomLeft, "bottom_left"},
         {ScreenAnchor::Bottom, "bottom"},
         {ScreenAnchor::BottomRight, "bottom_right"}}};

    // The names scene files use, e.g. "button" and "top_left"
    template <class Enum, std::size_t N>
    std::string_view NameOf(Enum value, const std::array<std::pair<Enum, std::string_view>, N>& names)
    {
        for (const auto& [each, name] : names)
        {
            if (each == value)
            {
                return name;
            }
        }
        return names.front().second;
    }

    template <class Enum, std::size_t N>
    std::optional<Enum> Parse(std::string_view name, const std::array<std::pair<Enum, std::string_view>, N>& names)
    {
        for (const auto& [each, known] : names)
        {
            if (known == name)
            {
                return each;
            }
        }
        return std::nullopt;
    }

    // Where an anchor is on the screen, as fractions of its width and height from the top left
    inline std::pair<float, float> AnchorPoint(ScreenAnchor anchor)
    {
        const auto index = static_cast<int>(anchor);
        const int column = index % 3;
        const int row = index / 3;
        return {static_cast<float>(column) * 0.5f, static_cast<float>(row) * 0.5f};
    }

    // A thing on the game's screen. Its middle is at its anchor plus (x, y), in percent of the screen's width and
    // height (down is +). Sizes are in percent of the screen's height, so it keeps its shape at any window size.
    // Blocks change it while the game plays (the Screen blocks); the editor shows it over the view, to be dragged.
    struct ScreenElementComponent
    {
        ScreenElementKind kind = ScreenElementKind::Text;
        std::string text = "Text"; // what a text says, a button's label, a bar's name
        std::string picture;       // a picture: one of the game's pictures, by its name ("heart") or path
        std::string barObject;     // a bar: the object whose variable fills it...
        std::string barVariable;   // ...that variable...
        float barMaximum = 100.0f; // ...and the value that fills it
        // Over its own object in the world (a bar, a name) instead of at a place on the screen. A bar with no object
        // named reads its own object's variable.
        bool aboveObject = false;
        ScreenAnchor anchor = ScreenAnchor::Centre;
        float x = 0.0f;
        float y = 0.0f;
        float height = 6.0f; // percent of the screen's height
        float width = 0.0f;  // a bar's or button's width, in percent of the screen's height (0: fitted)
        float colorR = 1.0f; // a text's colour, a picture's tint, a bar's fill, a button's colour
        float colorG = 1.0f;
        float colorB = 1.0f;
        int layer = 0; // higher layers are drawn over lower ones
        bool visible = true;

        bool operator==(const ScreenElementComponent&) const = default;
    };
}
