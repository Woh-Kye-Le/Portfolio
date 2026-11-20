/* !
@file    	    Panel.hpp
@author  	    sanni.go@digipen.edu [79%]
         	    c.kojat@digipen.edu [10%]
         	    w.kyele@digipen.edu [11%]
@date    		22/09/2026
@brief
    Base class for the editor panels: a named window whose contents the editor
    draws into a docked window.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <string>
#include <string_view>

namespace Editor::Panels
{
    // A window of the editor. The editor hosts each panel in a docked window and draws its contents into it.
    class Panel
    {
    public:
        explicit Panel(std::string_view name) : name_(name) {}
        virtual ~Panel() = default;

        std::string_view Name() const { return name_; }
        // The panel's contents, into the current (docked) window
        virtual void DrawContents() = 0;

    private:
        std::string name_;
    };
}
