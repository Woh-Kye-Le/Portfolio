/* !
@file    	    LogicPanel.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/10/2026
@brief
    Declares LogicPanel, the Logic window: every object's block scripts as
    readable outlines, with what is running and any problems, to find and open
    an object's logic.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "editor/panels/Panel.hpp"
#include "engine/logic/Blocks.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace Engine::Logic
{
    class LogicRunner;
}

namespace Engine::Scene
{
    struct SceneDescription;
}

namespace Editor::Panels
{
    // The Logic window (the README's logic visualiser): each object that has blocks, and its scripts written out as
    // indented sentences, so a learner can read what controls what at a glance:
    //
    //   Player                                   2 scripts, 1 running
    //     when Space key pressed
    //       repeat 10
    //         move forward 1
    //
    // While the game runs, the blocks that are running are highlighted and problems are listed in red. A script
    // without a hat says that it does not run. Clicking an object or a line selects the object and shows the block in
    // the Blocks window.
    class LogicPanel : public Panel
    {
    public:
        LogicPanel();

        // What to show this frame: the scene, the selected object, and the running game's logic (nullptr when not
        // playing)
        void SetScene(const Engine::Scene::SceneDescription* scene, std::uint32_t selected,
                      const Engine::Logic::LogicRunner* runner);
        // Called with an object and one of its blocks (0: the object only) when a line is clicked
        void SetOnOpen(std::function<void(std::uint32_t objectId, std::uint32_t blockId)> onOpen);

        void DrawContents() override;

    private:
        void DrawBlocks(const std::vector<Engine::Logic::Block>& blocks, std::uint32_t objectId, int depth,
                        const std::vector<std::uint32_t>& active);

        const Engine::Scene::SceneDescription* scene_ = nullptr;
        std::uint32_t selected_ = 0;
        const Engine::Logic::LogicRunner* runner_ = nullptr;
        std::function<void(std::uint32_t, std::uint32_t)> onOpen_;
    };
}
