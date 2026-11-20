/* !
@file    	    RenderSystem.hpp
@author  	    sung.h@digipen.edu [85%]
         	    w.kyele@digipen.edu [15%]
@date    		01/10/2026
@brief
    Declares RenderSystem: drawing entities, applying the Main camera and
    collecting the scene lights.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <cstdint>

namespace Engine::Scene
{
    class Scene;
}

namespace Engine::Assets
{
    class ResourceManager;
    class AssetGpuCache;
}

namespace Engine::Graphics
{
    class Mesh;
    class Camera3D;
}

namespace Engine::Renderer
{
    class Renderer3D;
}

namespace Engine::Graphics
{
    struct LightingContext;
}

namespace Engine::Systems
{
    // The ECS side of rendering. GameRuntime::Tick calls these every frame, in this order:
    // ApplyPrimaryCamera, CollectLights, then Draw.
    class RenderSystem
    {
    public:
        // Draws every visible entity that has a Transform and a MeshRenderer. Meshes/textures are uploaded to the
        // GPU the first time they're used (gpuCache). texturedCube is for cubes that have a texture but no model.
        static void Draw(Scene::Scene& scene, Renderer::Renderer3D& renderer, Assets::ResourceManager& resources,
                         Assets::AssetGpuCache& gpuCache, const Graphics::Mesh& texturedCube);
        // Points camera through the Main camera (primary CameraComponent + Transform). If there are several,
        // the one with the lowest entity id wins so it doesn't flicker between them. False if there's none.
        static bool ApplyPrimaryCamera(Scene::Scene& scene, Graphics::Camera3D& camera);
        // If the scene has any LightComponents, they replace the default lights in lighting (disabled ones are
        // skipped). A scene with none keeps the default lighting, so it isn't pitch black. seconds: how long the game
        // has been playing, which flickering lights flicker by; 0 (not playing): they stay steady.
        static void CollectLights(Scene::Scene& scene, Graphics::LightingContext& lighting, double seconds = 0.0);
        // How bright a flickering light is at a moment, 1 down to 1 - flicker. Each seed flickers in its own way.
        static float FlickerBrightness(double seconds, std::uint32_t seed, float flicker);
    };
}
