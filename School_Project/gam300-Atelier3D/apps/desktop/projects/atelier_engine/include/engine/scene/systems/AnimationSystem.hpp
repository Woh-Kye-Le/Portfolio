/* !
@file    	    AnimationSystem.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		02/11/2026
@brief
    Declares AnimationSystem, which moves the animations of objects that have
    an AnimatorComponent, and the clip an animator shows.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/assets/ResourceManager.hpp"
#include "engine/assets/data/ModelData.hpp"
#include "engine/scene/Entity.hpp"

#include <string>
#include <vector>

namespace Engine::Assets
{
    class ResourceManager;
}

namespace Engine::Scene
{
    class Scene;
    struct AnimatorComponent;
}

namespace Engine::Systems
{
    // Moves every playing animator on through its clip: a looping clip starts again at its end, any other stops there
    // (and stops playing). Only runs in Play mode.
    class AnimationSystem
    {
    public:
        static void Update(Scene::Scene& scene, Assets::ResourceManager& resources, double deltaSeconds);

        // Plays a clip on the entity, giving it an animator if it has none. Asked every frame the clip carries on; a new
        // clip, or "once" after it ended, starts from the beginning.
        static void Play(Scene::Scene& scene, Scene::Entity entity, const std::string& clip, bool loop);

        // A clip an animator can play, and the file it is in
        struct FoundClip
        {
            const Assets::AnimationClip* clip = nullptr; // null: there is no such clip
            Assets::ModelHandle from;                    // its file: the model itself, or one of the animation files
            int index = -1;                              // into that file's animations
        };
        // The clip an animator plays: the one of its name (any case) among its model's own animations, then among
        // those of its animation files; with no name, the first there is
        static FoundClip FindClip(const Scene::AnimatorComponent& animator, Assets::ModelHandle model,
                                  Assets::ResourceManager& resources);
    };
}
