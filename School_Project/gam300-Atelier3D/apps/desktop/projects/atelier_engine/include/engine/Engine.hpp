/* !
@file    	    Engine.hpp
@author  	    c.kojat@digipen.edu [75%]
         	    w.kyele@digipen.edu [15%]
         	    j.cai@digipen.edu [5%]
         	    k.thantwin@digipen.edu [5%]
@date    		26/09/2026
@brief
    Convenience header that includes the engine's main public headers in one go.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// One include for everything an app needs from the engine (the editor's runtime uses this).
// Inside the engine itself, include the specific headers instead so builds stay faster.

#include "engine/assets/ResourceManager.hpp"
#include "engine/audio/AudioSystem.hpp"
#include "engine/core/Application.hpp"
#include "engine/core/Log.hpp"
#include "engine/core/Time.hpp"
#include "engine/game/GameBuild.hpp"
#include "engine/game/GameProject.hpp"
#include "engine/game/GameRuntime.hpp"
#include "engine/graphics/GraphicsManager.hpp"
#include "engine/graphics/Renderer/Renderer.hpp"
#include "engine/input/Input.hpp"
#include "engine/platform/Paths.hpp"
#include "engine/platform/Window.hpp"
#include "engine/serializer/CfgSerializer.hpp"
#include "engine/serializer/ISerializer.hpp"
#include "engine/serializer/JsonSerializer.hpp"
#include "engine/ui/ImGuiLayer.hpp"
#include "engine/messenger/Events.hpp"
#include "engine/messenger/EventBus.hpp"
