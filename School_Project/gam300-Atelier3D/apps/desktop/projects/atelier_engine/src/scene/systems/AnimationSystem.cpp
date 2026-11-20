/* !
@file    	    AnimationSystem.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		02/11/2026
@brief
    Implements AnimationSystem: moves animators on through their clips.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/scene/systems/AnimationSystem.hpp"

#include "engine/assets/Animation.hpp"
#include "engine/assets/ResourceManager.hpp"
#include "engine/scene/Scene.hpp"

#include <cmath>

namespace Engine::Systems
{
    AnimationSystem::FoundClip AnimationSystem::FindClip(const Scene::AnimatorComponent& animator,
                                                         Assets::ModelHandle model, Assets::ResourceManager& resources)
    {
        std::vector<Assets::ModelHandle> files{model};
        files.insert(files.end(), animator.sourceModels.begin(), animator.sourceModels.end());
        for (const Assets::ModelHandle file : files)
        {
            const Assets::Model* data = file.IsValid() ? resources.Get(file) : nullptr;
            if (!data || data->animations.empty())
            {
                continue;
            }
            const int index = animator.clip.empty() ? 0 : Assets::FindClip(data->animations, animator.clip);
            if (index >= 0)
            {
                return {&data->animations[static_cast<std::size_t>(index)], file, index};
            }
        }
        return {};
    }

    void AnimationSystem::Play(Scene::Scene& scene, Scene::Entity entity, const std::string& clip, bool loop)
    {
        if (!scene.TryGetComponent<Scene::AnimatorComponent>(entity))
        {
            scene.AddComponent(entity, Scene::AnimatorComponent{.playing = false});
        }
        auto* animator = scene.TryGetComponent<Scene::AnimatorComponent>(entity);
        if (animator->clip != clip || !animator->playing || animator->loop != loop)
        {
            animator->time = 0.0f;
        }
        animator->clip = clip;
        animator->loop = loop;
        animator->playing = true;
    }

    void AnimationSystem::Update(Scene::Scene& scene, Assets::ResourceManager& resources, double deltaSeconds)
    {
        const auto dt = static_cast<float>(deltaSeconds);
        scene.ForEach<Scene::AnimatorComponent, const Scene::MeshRendererComponent>(
            [&resources, dt](Scene::Entity, Scene::AnimatorComponent& animator,
                             const Scene::MeshRendererComponent& renderer)
            {
                if (!animator.playing || !renderer.model.IsValid())
                {
                    return;
                }
                const FoundClip found = FindClip(animator, renderer.model, resources);
                if (!found.clip)
                {
                    return;
                }
                const float duration = found.clip->duration;
                animator.time += dt * animator.speed;
                if (duration <= 0.0f)
                {
                    animator.time = 0.0f;
                }
                else if (animator.loop)
                {
                    // Backwards (a negative speed) wraps round to the end
                    animator.time = std::fmod(animator.time, duration);
                    if (animator.time < 0.0f)
                    {
                        animator.time += duration;
                    }
                }
                else if (animator.time >= duration || (animator.speed < 0.0f && animator.time <= 0.0f))
                {
                    animator.time = animator.time >= duration ? duration : 0.0f;
                    animator.playing = false;
                }
            });
    }
}
