/* !
@file    	    RenderSystem.cpp
@author  	    sung.h@digipen.edu [85%]
         	    w.kyele@digipen.edu [9%]
         	    c.kojat@digipen.edu [6%]
@date    		01/10/2026
@brief
    Implements RenderSystem: draws meshes or built-in shapes with materials,
    sets the camera from the primary CameraComponent, and gathers
    LightComponents for the renderer.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/scene/systems/RenderSystem.hpp"

#include "engine/assets/AssetGpuCache.hpp"
#include "engine/assets/ResourceManager.hpp"
#include "engine/graphics/Camera/Camera3D.hpp"
#include "engine/graphics/Lighting/Lights.hpp"
#include "engine/graphics/Material/Material.hpp"
#include "engine/graphics/Mesh/Mesh.hpp"
#include "engine/graphics/Model/Model.hpp"
#include "engine/graphics/Renderer/MeshRenderer.hpp"
#include "engine/graphics/Renderer/RenderAPI.hpp"
#include "engine/graphics/Renderer/Renderer3D.hpp"
#include "engine/scene/Lod.hpp"
#include "engine/scene/Scene.hpp"
#include "engine/scene/systems/AnimationSystem.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

namespace Engine::Systems
{

    bool RenderSystem::ApplyPrimaryCamera(Scene::Scene& scene, Graphics::Camera3D& camera)
    {
        Scene::Entity::Id bestId = 0;
        std::optional<Scene::TransformComponent> bestTransform;
        Scene::CameraComponent bestCamera;

        scene.ForEach<const Scene::TransformComponent, const Scene::CameraComponent>(
            [&](Scene::Entity entity, const Scene::TransformComponent& transform,
                const Scene::CameraComponent& component)
            {
                if (component.primary && (!bestTransform || entity.GetId() < bestId))
                {
                    bestId = entity.GetId();
                    bestTransform = transform;
                    bestCamera = component;
                }
            });

        if (!bestTransform)
        {
            return false;
        }

        const float pitch = glm::radians(bestTransform->rotationX);
        const float yaw = glm::radians(bestTransform->rotation);

        const glm::vec3 forward(std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw));

        const glm::mat4& projection = camera.GetProjectionMatrix();

        const float aspect = projection[0][0] != 0.0f ? projection[1][1] / projection[0][0] : 16.0f / 9.0f;

        camera.SetPerspective(bestCamera.fieldOfView, aspect, bestCamera.nearPlane, bestCamera.farPlane);

        camera.SetPosition(glm::vec3(bestTransform->x, bestTransform->y, bestTransform->z));

        camera.SetOrientation(forward);
        return true;
    }

    void RenderSystem::Draw(Scene::Scene& scene, Renderer::Renderer3D& renderer, Assets::ResourceManager& resources,
                            Assets::AssetGpuCache& gpuCache, const Graphics::Mesh& texturedCube)
    {
        // Collect first, draw after: the ForEach order can change when components get added/removed,
        // so we sort by id below to keep the draw order stable
        struct DrawItem
        {
            Scene::Entity entity;
            glm::mat4 model;
            Scene::MeshRendererComponent component;
            std::optional<Scene::AnimatorComponent> animator;
            int lod = 0; // which version of its model to draw (0: the model itself)
        };

        // A model file is drawn turned inside its object by its model turn; a built-in shape is not
        const auto turnedOf = [](const glm::mat4& model, float turn)
        { return turn == 0.0f ? model : glm::rotate(model, glm::radians(turn), glm::vec3(0.0f, 1.0f, 0.0f)); };

        std::vector<DrawItem> items;

        // A view model (the first-person arms and gun) is placed from the camera rather than the world: its
        // Transform says where it is in front of the eye
        const glm::mat4 cameraPlace = glm::inverse(renderer.Camera().GetViewMatrix());
        const glm::vec3 eye = renderer.Camera().GetPosition();
        const float projectionYY = renderer.Camera().GetProjectionMatrix()[1][1];
        scene.ForEach<const Scene::TransformComponent, const Scene::MeshRendererComponent>(
            [&items, &scene, &cameraPlace, &eye, &projectionYY,
             &resources](Scene::Entity entity, const Scene::TransformComponent& transform,
                         const Scene::MeshRendererComponent& component)
            {
                if (!component.visible)
                {
                    return;
                }

                const glm::mat4 local = Scene::ToGraphicsTransform(transform).ComposeModelMatrix();
                const glm::mat4 model = component.viewModel ? cameraPlace * local : local;

                // Level of detail: the smaller the model is on screen, the simpler the version of it drawn (the engine
                // made them when it loaded the model). The level and the size are kept on the entity, for the next
                // frame's choice (the margin against flicker) and for the editor to show.
                int lod = 0;
                const Assets::Model* data = component.model.IsValid() ? resources.Get(component.model) : nullptr;
                if (data && !data->lods.empty() && component.hasBounds && !component.viewModel)
                {
                    const float scale =
                        transform.scale * std::max({transform.scaleX, transform.scaleY, transform.scaleZ});
                    const float radius = 0.5f * glm::length(component.boundsMax - component.boundsMin) * scale;
                    const float distance = glm::length(glm::vec3(model[3]) - eye);
                    const float size = Scene::ScreenSize(radius, distance, projectionYY);
                    lod = Scene::ChooseLodLevelForScreenSize(size, component.lodLevel,
                                                             1 + static_cast<int>(data->lods.size()));
                    if (auto* live = scene.TryGetComponent<Scene::MeshRendererComponent>(entity))
                    {
                        live->lodLevel = lod;
                        live->lodScreenSize = size;
                    }
                }

                const auto* animator = scene.TryGetComponent<Scene::AnimatorComponent>(entity);
                items.push_back(
                    DrawItem{entity, model, component, animator ? std::optional(*animator) : std::nullopt, lod});
            });

        // Preserve creation order when archetyp membership change; view models after everything else (see below)
        std::sort(items.begin(), items.end(),
                  [](const DrawItem& a, const DrawItem& b)
                  {
                      return a.component.viewModel != b.component.viewModel ? b.component.viewModel
                                                                            : a.entity.GetId() < b.entity.GetId();
                  });

        // Gives a shader the frame's lights and camera, for the lighting maths
        const auto light = [&renderer](const Graphics::Shader& shader)
        {
            shader.Bind();
            renderer.GetEnvironment().Apply(shader, renderer.GetLighting(), renderer.Camera().GetPosition());
            shader.SetInt("u_Lit", 1);
        };

        // The clip an animator poses a model with: a clip of one of its animation files is made to move this model's
        // bones
        const auto clipOf = [&gpuCache, &resources](const Scene::AnimatorComponent& animator, Assets::ModelHandle model)
        {
            const auto found = Systems::AnimationSystem::FindClip(animator, model, resources);
            const bool own = found.from.index == model.index && found.from.generation == model.generation;
            return !found.clip || own ? found.clip : gpuCache.RetargetedClip(model, found.from, found.index, resources);
        };

        // Where something held is: at a bone of its parent's model as that is posed this frame, then its own place
        // relative to the parent. Nothing when the parent has no such bone: it stays where its Transform says.
        const auto heldAt = [&](const DrawItem& item) -> std::optional<glm::mat4>
        {
            const auto* link = scene.TryGetComponent<Scene::HierarchyComponent>(item.entity);
            if (!link)
            {
                return std::nullopt;
            }
            const auto parent = std::find_if(items.begin(), items.end(), [link](const DrawItem& each)
                                             { return each.entity.GetId() == link->parent; });
            if (parent == items.end() || !parent->component.model.IsValid())
            {
                return std::nullopt;
            }
            const std::shared_ptr<Graphics::Model> model =
                gpuCache.GetOrUploadModel(parent->component.model, resources, renderer.StandardShader());
            const Assets::AnimationClip* clip =
                model && parent->animator ? clipOf(*parent->animator, parent->component.model) : nullptr;
            const auto bone = model ? model->NodeWorld(item.component.holdBone, clip,
                                                       parent->animator ? parent->animator->time : 0.0f)
                                    : std::nullopt;
            if (!bone)
            {
                return std::nullopt;
            }
            return turnedOf(parent->model, parent->component.modelTurn) * *bone *
                   Scene::ToGraphicsTransform(link->local).ComposeModelMatrix();
        };

        // Both sides of every face: imported models are often open (a wall panel, a sign, a cape) and would vanish
        // when seen from behind. The lit shader lights a back face from its own side, and a closed shape's back
        // faces are hidden by the depth test anyway. Culling is back on for whatever is drawn after.
        Graphics::RenderAPI::EnableFaceCulling(false);
        bool viewModels = false;
        for (const DrawItem& item : items)
        {
            const auto& component = item.component;
            // The view models are drawn over the world, never into it: a gun held against a wall stays whole
            if (component.viewModel && !viewModels)
            {
                Graphics::RenderAPI::ClearDepth();
                viewModels = true;
            }
            const glm::vec4 albedoTint(component.albedoR, component.albedoG, component.albedoB, component.albedoA);
            using Primitive = Renderer::Renderer3D::Primitive;

            // A model or mesh file is drawn turned inside the object by its model turn; a built-in shape is not
            const std::optional<glm::mat4> held = component.holdBone.empty() ? std::nullopt : heldAt(item);
            const glm::mat4& placed = held ? *held : item.model;
            const glm::mat4 turned = turnedOf(placed, component.modelTurn);
            // ...and a built-in shape at its own size and centre inside the object (1 x 1 x 1 at the middle unless set)
            const glm::mat4 shaped = glm::scale(glm::translate(placed, component.shapeCenter), component.shapeSize);

            // Its Material, when it has one: its own shader (or the standard one), colour, texture and values
            std::optional<Graphics::Material> material;
            if (const auto& source = component.material)
            {
                std::shared_ptr<Graphics::Shader> custom;
                if (source->shader.IsValid())
                {
                    custom = gpuCache.GetOrUploadShader(source->shader, resources);
                }
                const Graphics::Shader& shader = custom ? *custom : renderer.StandardShader();
                light(shader);
                for (const auto& [name, value] : source->file.values)
                {
                    shader.SetFloat(name, value);
                }
                material.emplace();
                if (custom)
                {
                    material->SetShader(custom);
                }
                else
                {
                    material->SetShader(renderer.StandardShader());
                }
                material->SetAlbedoTint(source->file.color * albedoTint);
                material->SetMetallic(source->file.metallic);
                material->SetRoughness(source->file.roughness);
                material->SetEmissiveTint(source->file.emissive);
                material->SetEmissiveStrength(source->file.emissiveStrength);
                const Assets::TextureHandle texture = source->texture.IsValid() ? source->texture : component.texture;
                if (texture.IsValid())
                {
                    material->SetAlbedoTexture(gpuCache.GetOrUploadTexture(texture, resources));
                }
            }

            // For whole imported model, Upload it once, then let it draw each submesh with its own material (or the
            // object's Material in place of all of them). Its materials are built on the renderer's lit shader, so
            // that shader needs the lights too.
            if (component.model.IsValid())
            {
                const std::shared_ptr<Graphics::Model> modelGpu =
                    gpuCache.GetOrUploadModel(component.model, resources, renderer.StandardShader(), item.lod);
                if (modelGpu)
                {
                    light(renderer.StandardShader());

                    // An animator poses it at its clip's moment; without one it stands at rest
                    const Assets::AnimationClip* clip =
                        item.animator ? clipOf(*item.animator, component.model) : nullptr;
                    modelGpu->Draw(turned, renderer.Camera(), clip, item.animator ? item.animator->time : 0.0f,
                                   material ? &*material : nullptr);
                    continue;
                }
            }

            const Graphics::Mesh* drawMesh = nullptr;
            std::shared_ptr<Graphics::Mesh> meshGpu;
            glm::mat4 drawModel = shaped; // the textured cube standing in for the built-in one, unless a mesh file

            // Imported model? Upload it the first time, after that the cache just hands it back
            if (component.mesh.IsValid())
            {
                meshGpu = gpuCache.GetOrUploadMesh(component.mesh, resources);
                drawMesh = meshGpu.get();
                drawModel = turned;
            }

            // The built-in cube has no UVs, so a textured cube without a model uses this one instead
            const bool textured = component.texture.IsValid() || (material && material->GetAlbedoTexture());
            if (!drawMesh && component.shape == Game::Shape::Cube && textured)
            {
                drawMesh = &texturedCube;
            }

            if (drawMesh && material)
            {
                Renderer::MeshRenderer::Draw(*drawMesh, *material, drawModel, renderer.Camera());
                continue;
            }

            if (drawMesh)
            {
                std::shared_ptr<Graphics::Shader> shaderGpu;

                if (component.shader.IsValid())
                {
                    shaderGpu = gpuCache.GetOrUploadShader(component.shader, resources);
                }

                Graphics::Material plain;
                plain.SetAlbedoTint(albedoTint);

                // Custom shader if the object has one, otherwise the engine's lit shader. Either way it needs the
                // lights and camera position for the lighting maths.
                if (shaderGpu)
                {
                    light(*shaderGpu);
                    plain.SetShader(shaderGpu);
                }
                else
                {
                    light(renderer.StandardShader());
                    plain.SetShader(renderer.StandardShader());
                }

                if (component.texture.IsValid())
                {
                    plain.SetAlbedoTexture(gpuCache.GetOrUploadTexture(component.texture, resources));
                }

                Renderer::MeshRenderer::Draw(*drawMesh, plain, drawModel, renderer.Camera());

                continue;
            }

            // A built-in shape with a Material
            if (material)
            {
                const auto shape = [&]
                {
                    switch (component.shape)
                    {
                    case Game::Shape::Quad: return Primitive::Quad;
                    case Game::Shape::Circle: return Primitive::Circle;
                    case Game::Shape::Pyramid: return Primitive::Pyramid;
                    case Game::Shape::Plane: return Primitive::Plane;
                    case Game::Shape::Wirebox: return Primitive::Wirebox;
                    case Game::Shape::Sphere: return Primitive::Sphere;
                    case Game::Shape::Line: return Primitive::Line;
                    case Game::Shape::Cube: break;
                    }
                    return Primitive::Cube;
                }();
                renderer.DrawPrimitiveWith(shape, shaped, *material);
                continue;
            }

            // No model: draw the built-in shape with just the tint colour
            switch (component.shape)
            {
            case Game::Shape::Quad: renderer.DrawPrimitive(Primitive::Quad, shaped, albedoTint); break;
            case Game::Shape::Circle: renderer.DrawPrimitive(Primitive::Circle, shaped, albedoTint); break;
            case Game::Shape::Cube: renderer.DrawPrimitive(Primitive::Cube, shaped, albedoTint); break;
            case Game::Shape::Pyramid: renderer.DrawPrimitive(Primitive::Pyramid, shaped, albedoTint); break;
            case Game::Shape::Plane: renderer.DrawPrimitive(Primitive::Plane, shaped, albedoTint); break;
            case Game::Shape::Wirebox: renderer.DrawPrimitive(Primitive::Wirebox, shaped, albedoTint); break;
            case Game::Shape::Sphere: renderer.DrawPrimitive(Primitive::Sphere, shaped, albedoTint); break;
            case Game::Shape::Line: renderer.DrawPrimitive(Primitive::Line, shaped, albedoTint); break;
            }
        }
        Graphics::RenderAPI::EnableFaceCulling(true, Graphics::CullFace::Back, Graphics::FrontFace::CCW);
    }

    float RenderSystem::FlickerBrightness(double seconds, std::uint32_t seed, float flicker)
    {
        // A random height at every whole number, eased from one to the next
        const auto noise = [seed](double at)
        {
            const auto height = [seed](std::int64_t index)
            {
                auto mixed = static_cast<std::uint32_t>(index) * 2654435761u + seed * 40503u;
                mixed = (mixed ^ (mixed >> 15)) * 2246822519u;
                mixed = (mixed ^ (mixed >> 13)) * 3266489917u;
                return static_cast<float>(mixed ^ (mixed >> 16)) / 4294967295.0f;
            };
            const double whole = std::floor(at);
            const auto t = static_cast<float>(at - whole);
            const auto index = static_cast<std::int64_t>(whole);
            return glm::mix(height(index), height(index + 1), t * t * (3.0f - 2.0f * t));
        };
        // A slow sway with a gentle shiver over it, and now and then a deeper dip. Kept to a few changes a second: a
        // light that lights a whole dark scene and changes faster makes the whole picture strobe
        const float shiver = 0.6f * noise(seconds * 2.5) + 0.4f * noise(seconds * 6.0);
        const float dip = std::max(noise(seconds * 0.6 + 50.0) - 0.75f, 0.0f) * 4.0f;
        return 1.0f - std::clamp(flicker, 0.0f, 1.0f) * std::clamp(0.7f * shiver + 0.3f * dip, 0.0f, 1.0f);
    }

    void RenderSystem::CollectLights(Scene::Scene& scene, Graphics::LightingContext& lighting, double seconds)
    {
        struct LightEntry
        {
            Scene::Entity::Id id;
            Graphics::Light light;
        };

        std::vector<LightEntry> entries;
        bool hasLightComponents = false;

        scene.ForEach<const Scene::TransformComponent, const Scene::LightComponent>(
            [&](Scene::Entity entity, const Scene::TransformComponent& transform,
                const Scene::LightComponent& component)
            {
                hasLightComponents = true;

                if (!component.enabled)
                {
                    return;
                }

                // direction from the Transform rotation, same formula the camera uses
                const float pitch = glm::radians(transform.rotationX);
                const float yaw = glm::radians(transform.rotation);

                Graphics::Light light;
                light.type = component.type;
                light.color = glm::vec3(component.colorR, component.colorG, component.colorB);
                light.intensity = component.intensity;
                if (component.flicker > 0.0f && seconds > 0.0)
                {
                    light.intensity *= FlickerBrightness(seconds, entity.GetId(), component.flicker);
                }
                light.position = glm::vec3(transform.x, transform.y, transform.z);
                light.direction =
                    glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw));
                light.range = component.range;
                light.spotInnerConeDeg = component.spotInnerConeDeg;
                light.spotOuterConeDeg = component.spotOuterConeDeg;

                entries.push_back({entity.GetId(), light});
            });

        // Scenes without ECS lights retain the renderer's default lighting.
        if (!hasLightComponents)
        {
            return;
        }

        std::sort(entries.begin(), entries.end(), [](const LightEntry& a, const LightEntry& b) { return a.id < b.id; });

        // Disabled ECS lights should not leave old lights active.
        lighting.lights.clear();

        for (const LightEntry& entry : entries)
        {
            lighting.lights.push_back(entry.light);
        }
    }

}
