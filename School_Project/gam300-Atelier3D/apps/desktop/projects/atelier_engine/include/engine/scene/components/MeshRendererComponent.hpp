/* !
@file    	    MeshRendererComponent.hpp
@author  	    sung.h@digipen.edu [59%]
         	    c.kojat@digipen.edu [23%]
         	    w.kyele@digipen.edu [18%]
@date    		01/10/2026
@brief
    MeshRendererComponent: the shape or mesh, texture, shader and tint an entity
    is drawn with.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/assets/MaterialFile.hpp"
#include "engine/assets/ResourceManager.hpp"
#include "engine/game/Shape.hpp"

#include <glm/vec3.hpp>

#include <array>
#include <memory>

namespace Engine::Scene
{
    // What RenderSystem draws for an entity. If there's no mesh loaded it falls back to the built-in shape.
    // The handles point into the ResourceManager, so lots of entities can share one model/texture.
    struct MeshRendererComponent
    {

        Engine::Game::Shape shape = Engine::Game::Shape::Cube;

        // invalid handle = not used (e.g. no texture)
        Engine::Assets::MeshHandle mesh;
        Engine::Assets::TextureHandle texture;
        Engine::Assets::ShaderHandle shader;

        // A whole imported model (its own meshes, materials and textures); drawn before mesh/texture above.
        Engine::Assets::ModelHandle model;

        bool visible = true;

        // Its material (shared by every object using the same file, and changed in place when the file is saved);
        // null: the texture and shader above
        std::shared_ptr<const Engine::Assets::LoadedMaterial> material;
        float modelTurn = 0.0f; // degrees its model or mesh is turned round Y inside the object
        std::string holdBone;   // a bone of its parent's model it stays in as that animates (a sword in a hand)
        bool viewModel = false; // placed from the camera and drawn over the world (first-person arms and gun)

        // tint multiplied into the colour/texture, white = unchanged
        float albedoR = 1.0f;
        float albedoG = 1.0f;
        float albedoB = 1.0f;
        float albedoA = 1.0f;

        // A built-in shape's size and centre inside the object (RenderableDesc), before the Transform
        glm::vec3 shapeSize{1.0f};
        glm::vec3 shapeCenter{0.0f};

        // The box around its model or mesh at rest, in its own units (read when the file was loaded), so a collider
        // can fit what it draws. hasBounds is false for a built-in shape.
        bool hasBounds = false;
        glm::vec3 boundsMin{0.0f};
        glm::vec3 boundsMax{0.0f};

        // Level of detail (Lod.hpp): which of its model's versions RenderSystem drew last frame (0 the full model,
        // 1 and 2 the simpler ones the engine made when the model was loaded), and how much of the view's height the
        // model filled then
        int lodLevel = 0;
        float lodScreenSize = 0.0f;
    };

}
