/* !
@file    	    GameObjectJson.cpp
@author  	    sung.h@digipen.edu [70%]
         	    c.kojat@digipen.edu [30%]
@date    		20/10/2026
@brief
    Implements the game object JSON layout shared by scene and prefab files:
    writing each component, forgiving reads that report missing or invalid
    fields and use defaults, and fixing ids and parents.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "GameObjectJson.hpp"

#include "engine/logic/Blocks.hpp"
#include "engine/scene/Hierarchy.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <numeric>
#include <optional>
#include <set>
#include <utility>

namespace Engine::Scene::Detail
{
    namespace
    {
        using Serializer::JSONSerializer;

        // light types are saved as words ("point") instead of numbers so the file is easy to read and edit
        const char* LightTypeName(Graphics::LightType type)
        {
            switch (type)
            {
            case Graphics::LightType::Directional: return "directional";
            case Graphics::LightType::Spot: return "spot";
            case Graphics::LightType::Point: break;
            }
            return "point";
        }

        std::optional<Graphics::LightType> ParseLightType(const std::string& name)
        {
            if (name == "directional")
            {
                return Graphics::LightType::Directional;
            }
            if (name == "point")
            {
                return Graphics::LightType::Point;
            }
            if (name == "spot")
            {
                return Graphics::LightType::Spot;
            }
            return std::nullopt;
        }

        // ---------------------------------------------------------------- Writing
        // One function per component. Each one makes its own JSON object inside the current game object,
        // e.g. "transform": { "x": 0, ... }. Keep the keys the same as the Read* functions below!

        void WriteTransform(JSONSerializer& json, const TransformComponent& transform)
        {
            json.CreateObject("transform");
            json.EnterObject("transform");
            json.WriteFloat("x", transform.x);
            json.WriteFloat("y", transform.y);
            json.WriteFloat("z", transform.z);
            json.WriteFloat("rotation_x", transform.rotationX);
            json.WriteFloat("rotation", transform.rotation);
            json.WriteFloat("rotation_z", transform.rotationZ);
            json.WriteFloat("size", transform.scale);
            json.WriteFloat("scale_x", transform.scaleX);
            json.WriteFloat("scale_y", transform.scaleY);
            json.WriteFloat("scale_z", transform.scaleZ);
            json.LeaveObject();
        }

        void WriteRenderable(JSONSerializer& json, const RenderableDesc& renderable)
        {
            json.CreateObject("renderable");
            json.EnterObject("renderable");
            json.WriteString("shape", Game::ToString(renderable.shape));
            json.WriteFloat("rotation_speed", renderable.rotationSpeed);
            // Asset paths only when set, and the tint only when it is not plain white
            for (const auto& [key, path] :
                 {std::pair{"mesh", &renderable.mesh}, std::pair{"model", &renderable.model},
                  std::pair{"texture", &renderable.texture}, std::pair{"shader", &renderable.shader},
                  std::pair{"material", &renderable.material}})
            {
                if (!path->empty())
                {
                    json.WriteString(key, *path);
                }
            }
            if (renderable.modelTurn != 0.0f)
            {
                json.WriteFloat("model_turn", renderable.modelTurn);
            }
            if (!renderable.holdBone.empty())
            {
                json.WriteString("hold_bone", renderable.holdBone);
            }
            if (renderable.viewModel)
            {
                json.WriteBool("view_model", true);
            }
            if (renderable.albedoR != 1.0f || renderable.albedoG != 1.0f || renderable.albedoB != 1.0f ||
                renderable.albedoA != 1.0f)
            {
                json.WriteFloat("albedo_r", renderable.albedoR);
                json.WriteFloat("albedo_g", renderable.albedoG);
                json.WriteFloat("albedo_b", renderable.albedoB);
                json.WriteFloat("albedo_a", renderable.albedoA);
            }
            // The shape's own size and centre only when it has them (see RenderableDesc)
            if (renderable.shapeSizeX != 1.0f || renderable.shapeSizeY != 1.0f || renderable.shapeSizeZ != 1.0f ||
                renderable.shapeCenterX != 0.0f || renderable.shapeCenterY != 0.0f || renderable.shapeCenterZ != 0.0f)
            {
                json.WriteFloat("shape_size_x", renderable.shapeSizeX);
                json.WriteFloat("shape_size_y", renderable.shapeSizeY);
                json.WriteFloat("shape_size_z", renderable.shapeSizeZ);
                json.WriteFloat("shape_center_x", renderable.shapeCenterX);
                json.WriteFloat("shape_center_y", renderable.shapeCenterY);
                json.WriteFloat("shape_center_z", renderable.shapeCenterZ);
            }
            json.LeaveObject();
        }

        void WriteAudioSource(JSONSerializer& json, const AudioSourceDesc& audio)
        {
            json.CreateObject("audio_source");
            json.EnterObject("audio_source");
            json.WriteString("clip", audio.clip);
            json.WriteFloat("volume", audio.volume);
            json.WriteBool("loop", audio.loop);
            json.WriteBool("play_on_start", audio.playOnStart);
            json.WriteBool("spatial", audio.spatial);
            json.WriteFloat("min_distance", audio.minDistance);
            json.WriteFloat("max_distance", audio.maxDistance);
            json.WriteString("bus", audio.bus);
            json.LeaveObject();
        }

        void WritePlayer(JSONSerializer& json, const PlayerControllerComponent& player)
        {
            json.CreateObject("player");
            json.EnterObject("player");
            json.WriteFloat("speed", player.speed);
            json.WriteFloat("fast_multiplier", player.fastMultiplier);
            json.WriteFloat("camera_offset_y", player.cameraOffsetY);
            json.WriteFloat("camera_offset_z", player.cameraOffsetZ);
            json.WriteBool("follow_camera", player.followCamera);
            json.WriteFloat("look_sensitivity", player.lookSensitivity);
            json.WriteFloat("camera_pitch", player.cameraPitch);
            json.LeaveObject();
        }

        void WriteCamera(JSONSerializer& json, const CameraComponent& camera)
        {
            json.CreateObject("camera");
            json.EnterObject("camera");
            json.WriteFloat("field_of_view", camera.fieldOfView);
            json.WriteFloat("near_plane", camera.nearPlane);
            json.WriteFloat("far_plane", camera.farPlane);
            json.WriteBool("primary", camera.primary);
            json.LeaveObject();
        }

        void WriteLight(JSONSerializer& json, const LightComponent& light)
        {
            json.CreateObject("light");
            json.EnterObject("light");
            json.WriteString("type", LightTypeName(light.type));
            json.WriteBool("enabled", light.enabled);
            json.WriteFloat("color_r", light.colorR);
            json.WriteFloat("color_g", light.colorG);
            json.WriteFloat("color_b", light.colorB);
            json.WriteFloat("intensity", light.intensity);
            json.WriteFloat("range", light.range);
            json.WriteFloat("spot_inner", light.spotInnerConeDeg);
            json.WriteFloat("spot_outer", light.spotOuterConeDeg);
            if (light.flicker != 0.0f)
            {
                json.WriteFloat("flicker", light.flicker);
            }
            json.LeaveObject();
        }

        void WriteScreen(JSONSerializer& json, const ScreenElementComponent& screen)
        {
            json.CreateObject("screen");
            json.EnterObject("screen");
            json.WriteString("kind", std::string(NameOf(screen.kind, kScreenElementKinds)));
            json.WriteString("text", screen.text);
            json.WriteString("picture", screen.picture);
            json.WriteString("bar_object", screen.barObject);
            json.WriteString("bar_variable", screen.barVariable);
            json.WriteFloat("bar_maximum", screen.barMaximum);
            json.WriteString("anchor", std::string(NameOf(screen.anchor, kScreenAnchors)));
            json.WriteFloat("x", screen.x);
            json.WriteFloat("y", screen.y);
            json.WriteFloat("height", screen.height);
            json.WriteFloat("width", screen.width);
            json.WriteFloat("color_r", screen.colorR);
            json.WriteFloat("color_g", screen.colorG);
            json.WriteFloat("color_b", screen.colorB);
            json.WriteInt("layer", screen.layer);
            json.WriteBool("visible", screen.visible);
            if (screen.aboveObject)
            {
                json.WriteBool("above_object", true);
            }
            json.LeaveObject();
        }

        void WriteAnimator(JSONSerializer& json, const AnimatorComponent& animator)
        {
            json.CreateObject("animator");
            json.EnterObject("animator");
            json.WriteString("clip", animator.clip);
            json.WriteBool("playing", animator.playing);
            json.WriteBool("loop", animator.loop);
            json.WriteFloat("speed", animator.speed);
            if (!animator.sources.empty())
            {
                json.CreateArray("sources");
                json.EnterArray("sources");
                for (const std::string& source : animator.sources)
                {
                    json.WriteString(source);
                }
                json.LeaveArray();
            }
            json.LeaveObject();
        }

        // The sky's fields, by their key in the file
        auto SkyFields(auto& sky)
        {
            return std::array{std::pair{"top_r", &sky.topR},         std::pair{"top_g", &sky.topG},
                              std::pair{"top_b", &sky.topB},         std::pair{"horizon_r", &sky.horizonR},
                              std::pair{"horizon_g", &sky.horizonG}, std::pair{"horizon_b", &sky.horizonB},
                              std::pair{"ground_r", &sky.groundR},   std::pair{"ground_g", &sky.groundG},
                              std::pair{"ground_b", &sky.groundB},   std::pair{"haze", &sky.haze},
                              std::pair{"turn", &sky.turn}};
        }

        void WriteSky(JSONSerializer& json, const SkyComponent& sky)
        {
            json.CreateObject("sky");
            json.EnterObject("sky");
            for (const auto& [key, value] : SkyFields(sky))
            {
                json.WriteFloat(key, *value);
            }
            json.WriteString("picture", sky.picture);
            if (sky.ambient != SkyComponent::kDefaultAmbient)
            {
                json.WriteFloat("ambient", sky.ambient);
            }
            json.LeaveObject();
        }

        void WriteRigidBody(JSONSerializer& json, const RigidBodyComponent& body)
        {
            json.CreateObject("rigid_body");
            json.EnterObject("rigid_body");
            json.WriteString("type", BodyTypeName(body.type));
            json.WriteFloat("mass", body.mass);
            json.WriteFloat("gravity_scale", body.gravityScale);
            json.WriteFloat("friction", body.friction);
            json.WriteFloat("bounciness", body.bounciness);
            if (!body.surface.empty())
            {
                json.WriteString("surface", body.surface);
            }
            if (body.keepUpright)
            {
                json.WriteBool("keep_upright", true);
            }
            const std::pair<const char*, bool> locks[] = {
                {"lock_move_x", body.lockMoveX}, {"lock_move_y", body.lockMoveY}, {"lock_move_z", body.lockMoveZ},
                {"lock_turn_x", body.lockTurnX}, {"lock_turn_y", body.lockTurnY}, {"lock_turn_z", body.lockTurnZ}};
            for (const auto& [key, locked] : locks)
            {
                if (locked)
                {
                    json.WriteBool(key, true);
                }
            }
            if (body.layers != RigidBodyComponent{}.layers)
            {
                json.WriteInt("layers", body.layers);
            }
            json.WriteFloat("start_velocity_x", body.startVelocityX);
            json.WriteFloat("start_velocity_y", body.startVelocityY);
            json.WriteFloat("start_velocity_z", body.startVelocityZ);
            json.WriteString("shape", ColliderShapeName(body.shape));
            json.WriteFloat("center_x", body.centerX);
            json.WriteFloat("center_y", body.centerY);
            json.WriteFloat("center_z", body.centerZ);
            json.WriteFloat("size_x", body.sizeX);
            json.WriteFloat("size_y", body.sizeY);
            json.WriteFloat("size_z", body.sizeZ);
            json.WriteFloat("radius", body.radius);
            json.WriteFloat("height", body.height);
            if (!body.parts.empty())
            {
                json.CreateArray("parts");
                json.EnterArray("parts");
                for (std::size_t i = 0; i < body.parts.size(); ++i)
                {
                    const ColliderPart& part = body.parts[i];
                    json.CreateObject();
                    json.EnterArrayElement(i);
                    json.WriteString("shape", ColliderShapeName(part.shape));
                    json.WriteFloat("center_x", part.centerX);
                    json.WriteFloat("center_y", part.centerY);
                    json.WriteFloat("center_z", part.centerZ);
                    json.WriteFloat("size_x", part.sizeX);
                    json.WriteFloat("size_y", part.sizeY);
                    json.WriteFloat("size_z", part.sizeZ);
                    json.WriteFloat("radius", part.radius);
                    json.WriteFloat("height", part.height);
                    json.LeaveObject();
                }
                json.LeaveArray();
            }
            json.LeaveObject();
        }

        // A list of blocks, each with only the fields its kind has:
        //   { "id": 3, "block": "repeat", "numbers": [10], "body": [ ... ] }
        //   { "id": 4, "block": "if", "slots": [ { "id": 5, "block": "key_down", "text": "Space" } ], "body": [] }
        // An empty condition slot is written as {}.
        void WriteBlocks(JSONSerializer& json, const std::string& key, const std::vector<Logic::Block>& blocks)
        {
            json.CreateArray(key);
            json.EnterArray(key);
            for (std::size_t i = 0; i < blocks.size(); ++i)
            {
                const Logic::Block& block = blocks[i];
                json.CreateObject();
                json.EnterArrayElement(i);
                if (block.id != 0)
                {
                    const Logic::BlockInfo& info = Logic::InfoOf(block.op);
                    json.WriteInt("id", static_cast<int>(block.id));
                    json.WriteString("block", std::string(info.name));
                    // A My Block's numbers are its inputs': how many depends on the My Block
                    const bool anyNumber = std::any_of(block.numbers.begin(), block.numbers.end(),
                                                       [](float number) { return number != 0.0f; });
                    const std::size_t count = block.op == Logic::Op::Call ? (anyNumber ? block.numbers.size() : 0)
                                                                          : Logic::NumberCount(block.op);
                    if (count > 0)
                    {
                        json.CreateArray("numbers");
                        json.EnterArray("numbers");
                        for (std::size_t n = 0; n < count && n < block.numbers.size(); ++n)
                        {
                            json.WriteFloat(block.numbers[n]);
                        }
                        json.LeaveArray();
                    }
                    if (Logic::HasKeyField(block.op) || Logic::HasTextField(block.op) ||
                        Logic::HasVariableField(block.op))
                    {
                        json.WriteString("text", block.text);
                    }
                    // Value blocks used instead of numbers, by position (only when there is one)
                    if (std::any_of(block.inputs.begin(), block.inputs.end(),
                                    [](const Logic::Block& input) { return input.id != 0; }))
                    {
                        WriteBlocks(json, "inputs", block.inputs);
                    }
                    if (Logic::SlotCount(block.op) > 0)
                    {
                        WriteBlocks(json, "slots", block.slots);
                    }
                    if (info.hasBody)
                    {
                        WriteBlocks(json, "body", block.body);
                    }
                    if (info.hasElse)
                    {
                        WriteBlocks(json, "else", block.elseBody);
                    }
                }
                json.LeaveObject();
            }
            json.LeaveArray();
        }

        // "logic": { "variables": [ { "name": "score", "value": 0 }, { "name": "alive", "value": 1,
        //                             "kind": "true_false" } ],
        //            "scripts": [ { "x": 40, "y": 30, "blocks": [ ... ] } ] }
        // A number variable has no "kind", so scenes from before true/false variables stay the same.
        void WriteLogic(JSONSerializer& json, const Logic::LogicDesc& logic)
        {
            json.CreateObject("logic");
            json.EnterObject("logic");
            if (!logic.variables.empty())
            {
                json.CreateArray("variables");
                json.EnterArray("variables");
                for (std::size_t i = 0; i < logic.variables.size(); ++i)
                {
                    json.CreateObject();
                    json.EnterArrayElement(i);
                    json.WriteString("name", logic.variables[i].name);
                    json.WriteFloat("value", logic.variables[i].value);
                    if (logic.variables[i].kind == Logic::VariableKind::TrueFalse)
                    {
                        json.WriteString("kind", "true_false");
                    }
                    json.LeaveObject();
                }
                json.LeaveArray();
            }
            json.CreateArray("scripts");
            json.EnterArray("scripts");
            for (std::size_t i = 0; i < logic.scripts.size(); ++i)
            {
                const Logic::Script& script = logic.scripts[i];
                json.CreateObject();
                json.EnterArrayElement(i);
                json.WriteFloat("x", script.x);
                json.WriteFloat("y", script.y);
                WriteBlocks(json, "blocks", script.blocks);
                json.LeaveObject();
            }
            json.LeaveArray();
            json.LeaveObject();
        }

        // "prefab": { "path": "assets/prefabs/Enemy.prefab", "object": 1, "overrides": [ "health/health" ] }
        void WritePrefabInstance(JSONSerializer& json, const PrefabInstance& link)
        {
            json.CreateObject("prefab");
            json.EnterObject("prefab");
            json.WriteString("path", link.prefab);
            if (!link.guid.empty())
            {
                json.WriteString("guid", link.guid);
            }
            json.WriteInt("object", static_cast<int>(link.object));
            json.CreateArray("overrides");
            json.EnterArray("overrides");
            for (const std::string& property : link.overrides)
            {
                json.WriteString(property);
            }
            json.LeaveArray();
            json.LeaveObject();
        }

        // id, name, parent (only when it has one), then only the components the object has
        void WriteGameObject(JSONSerializer& json, const GameObject& object)
        {
            json.WriteInt("id", static_cast<int>(object.id));
            json.WriteString("name", object.name);

            if (!object.luaFile.empty())
            {
                json.WriteString("lua_file", object.luaFile);
            }
            if (!object.luaSource.empty())
            {
                json.WriteString("lua_source", object.luaSource);
            }

            if (object.parent != 0)
            {
                json.WriteInt("parent", static_cast<int>(object.parent));
            }
            if (object.transform)
            {
                WriteTransform(json, *object.transform);
            }
            if (object.renderable)
            {
                WriteRenderable(json, *object.renderable);
            }
            if (object.audioSource)
            {
                WriteAudioSource(json, *object.audioSource);
            }
            if (object.player)
            {
                WritePlayer(json, *object.player);
            }
            if (object.camera)
            {
                WriteCamera(json, *object.camera);
            }
            if (object.light)
            {
                WriteLight(json, *object.light);
            }
            if (object.screen)
            {
                WriteScreen(json, *object.screen);
            }
            if (object.animator)
            {
                WriteAnimator(json, *object.animator);
            }
            if (object.sky)
            {
                WriteSky(json, *object.sky);
            }
            if (object.rigidBody)
            {
                WriteRigidBody(json, *object.rigidBody);
            }
            if (object.logic)
            {
                WriteLogic(json, *object.logic);
            }
            if (object.prefabInstance)
            {
                WritePrefabInstance(json, *object.prefabInstance);
            }
        }

        // ---------------------------------------------------------------- Reading

        // Reads fields of the current JSON object. A missing field (when required) or one of the wrong type keeps the
        // value it has, the default, and adds a warning naming where it is, e.g. "object 3 (Crate) transform".
        class FieldReader
        {
        public:
            FieldReader(JSONSerializer& json, SceneLoadReport& report, std::string where)
                : json_(json), report_(report), where_(std::move(where))
            {
            }

            void Number(const char* key, float& value, bool required = true)
            {
                if (Present(key, required) && !json_.TryReadFloat(key, value))
                {
                    Warn(std::format("\"{}\" is not a number - using {}", key, value));
                }
            }

            void WholeNumber(const char* key, int& value, bool required = true)
            {
                if (Present(key, required) && !json_.TryReadInt(key, value))
                {
                    Warn(std::format("\"{}\" is not a whole number - using {}", key, value));
                }
            }

            void Flag(const char* key, bool& value, bool required = true)
            {
                if (Present(key, required) && !json_.TryReadBool(key, value))
                {
                    Warn(std::format("\"{}\" is not true or false - using {}", key, value));
                }
            }

            void Text(const char* key, std::string& value, bool required = true)
            {
                if (Present(key, required) && !json_.TryReadString(key, value))
                {
                    Warn(std::format("\"{}\" is not text - using \"{}\"", key, value));
                }
            }

            void Warn(const std::string& message) { report_.warnings.push_back(where_ + ": " + message); }

        private:
            bool Present(const char* key, bool required)
            {
                if (json_.HasKey(key))
                {
                    return true;
                }
                if (required)
                {
                    Warn(std::format("missing \"{}\"", key));
                }
                return false;
            }

            JSONSerializer& json_;
            SceneLoadReport& report_;
            std::string where_;
        };

        TransformComponent ReadTransform(JSONSerializer& json, SceneLoadReport& report, const std::string& where)
        {
            TransformComponent transform;
            json.EnterObject("transform");
            FieldReader read(json, report, where + " transform");
            read.Number("x", transform.x);
            read.Number("y", transform.y);
            read.Number("z", transform.z);
            read.Number("rotation_x", transform.rotationX);
            read.Number("rotation", transform.rotation);
            read.Number("rotation_z", transform.rotationZ);
            read.Number("size", transform.scale);
            read.Number("scale_x", transform.scaleX);
            read.Number("scale_y", transform.scaleY);
            read.Number("scale_z", transform.scaleZ);
            if (!(transform.scale > 0.0f)) // also catches NaN
            {
                read.Warn("\"size\" must be above 0 - using 1");
                transform.scale = 1.0f;
            }
            json.LeaveObject();
            return transform;
        }

        RenderableDesc ReadRenderable(JSONSerializer& json, SceneLoadReport& report, const std::string& where)
        {
            RenderableDesc renderable;
            json.EnterObject("renderable");
            FieldReader read(json, report, where + " renderable");
            std::string shape = Game::ToString(renderable.shape);
            read.Text("shape", shape);
            if (const auto parsed = Game::ParseShape(shape))
            {
                renderable.shape = *parsed;
            }
            else
            {
                read.Warn(std::format("unknown shape \"{}\" - using cube", shape));
            }
            read.Number("rotation_speed", renderable.rotationSpeed);
            read.Text("mesh", renderable.mesh, false);
            read.Text("model", renderable.model, false);
            read.Text("texture", renderable.texture, false);
            read.Text("shader", renderable.shader, false);
            read.Text("material", renderable.material, false);
            read.Number("model_turn", renderable.modelTurn, false);
            read.Text("hold_bone", renderable.holdBone, false);
            read.Flag("view_model", renderable.viewModel, false);
            read.Number("albedo_r", renderable.albedoR, false);
            read.Number("albedo_g", renderable.albedoG, false);
            read.Number("albedo_b", renderable.albedoB, false);
            read.Number("albedo_a", renderable.albedoA, false);
            for (const auto& [key, value] :
                 {std::pair{"shape_size_x", &renderable.shapeSizeX}, std::pair{"shape_size_y", &renderable.shapeSizeY},
                  std::pair{"shape_size_z", &renderable.shapeSizeZ}})
            {
                read.Number(key, *value, false);
                if (!(*value > 0.0f)) // also catches NaN
                {
                    read.Warn(std::format("\"{}\" must be above 0 - using 1", key));
                    *value = 1.0f;
                }
            }
            read.Number("shape_center_x", renderable.shapeCenterX, false);
            read.Number("shape_center_y", renderable.shapeCenterY, false);
            read.Number("shape_center_z", renderable.shapeCenterZ, false);
            json.LeaveObject();
            return renderable;
        }

        AudioSourceDesc ReadAudioSource(JSONSerializer& json, SceneLoadReport& report, const std::string& where)
        {
            AudioSourceDesc audio;
            json.EnterObject("audio_source");
            FieldReader read(json, report, where + " audio_source");
            read.Text("clip", audio.clip);
            read.Number("volume", audio.volume);
            read.Flag("loop", audio.loop);
            read.Flag("play_on_start", audio.playOnStart);
            read.Flag("spatial", audio.spatial);
            read.Number("min_distance", audio.minDistance);
            read.Number("max_distance", audio.maxDistance);
            read.Text("bus", audio.bus);
            json.LeaveObject();
            return audio;
        }

        PlayerControllerComponent ReadPlayer(JSONSerializer& json, SceneLoadReport& report, const std::string& where)
        {
            PlayerControllerComponent player;
            json.EnterObject("player");
            FieldReader read(json, report, where + " player");
            read.Number("speed", player.speed);
            read.Number("fast_multiplier", player.fastMultiplier);
            read.Number("camera_offset_y", player.cameraOffsetY);
            read.Number("camera_offset_z", player.cameraOffsetZ);
            read.Flag("follow_camera", player.followCamera);
            read.Number("look_sensitivity", player.lookSensitivity);
            read.Number("camera_pitch", player.cameraPitch);
            json.LeaveObject();
            return player;
        }

        CameraComponent ReadCamera(JSONSerializer& json, SceneLoadReport& report, const std::string& where)
        {
            CameraComponent camera;
            json.EnterObject("camera");
            FieldReader read(json, report, where + " camera");
            read.Number("field_of_view", camera.fieldOfView);
            read.Number("near_plane", camera.nearPlane);
            read.Number("far_plane", camera.farPlane);
            read.Flag("primary", camera.primary);
            json.LeaveObject();
            return camera;
        }

        LightComponent ReadLight(JSONSerializer& json, SceneLoadReport& report, const std::string& where)
        {
            LightComponent light;
            json.EnterObject("light");
            FieldReader read(json, report, where + " light");
            std::string type = LightTypeName(light.type);
            read.Text("type", type);
            if (const auto parsed = ParseLightType(type))
            {
                light.type = *parsed;
            }
            else
            {
                read.Warn(std::format("unknown light type \"{}\" - using point", type));
            }
            read.Flag("enabled", light.enabled);
            read.Number("color_r", light.colorR);
            read.Number("color_g", light.colorG);
            read.Number("color_b", light.colorB);
            read.Number("intensity", light.intensity);
            read.Number("range", light.range);
            read.Number("spot_inner", light.spotInnerConeDeg);
            read.Number("spot_outer", light.spotOuterConeDeg);
            read.Number("flicker", light.flicker, false);
            json.LeaveObject();
            return light;
        }

        ScreenElementComponent ReadScreen(JSONSerializer& json, SceneLoadReport& report, const std::string& where)
        {
            ScreenElementComponent screen;
            json.EnterObject("screen");
            FieldReader read(json, report, where + " screen");
            std::string kind(NameOf(screen.kind, kScreenElementKinds));
            read.Text("kind", kind);
            if (const auto parsed = Parse(kind, kScreenElementKinds))
            {
                screen.kind = *parsed;
            }
            else
            {
                read.Warn(std::format("unknown kind \"{}\" - using text", kind));
            }
            read.Text("text", screen.text, false);
            read.Text("picture", screen.picture, false);
            read.Text("bar_object", screen.barObject, false);
            read.Text("bar_variable", screen.barVariable, false);
            read.Number("bar_maximum", screen.barMaximum, false);
            std::string anchor(NameOf(screen.anchor, kScreenAnchors));
            read.Text("anchor", anchor, false);
            if (const auto parsed = Parse(anchor, kScreenAnchors))
            {
                screen.anchor = *parsed;
            }
            else
            {
                read.Warn(std::format("unknown anchor \"{}\" - using centre", anchor));
            }
            for (const auto& [key, value] :
                 {std::pair{"x", &screen.x}, std::pair{"y", &screen.y}, std::pair{"height", &screen.height},
                  std::pair{"width", &screen.width}, std::pair{"color_r", &screen.colorR},
                  std::pair{"color_g", &screen.colorG}, std::pair{"color_b", &screen.colorB}})
            {
                read.Number(key, *value, false);
            }
            read.WholeNumber("layer", screen.layer, false);
            read.Flag("visible", screen.visible, false);
            read.Flag("above_object", screen.aboveObject, false);
            json.LeaveObject();
            return screen;
        }

        AnimatorComponent ReadAnimator(JSONSerializer& json, SceneLoadReport& report, const std::string& where)
        {
            AnimatorComponent animator;
            json.EnterObject("animator");
            FieldReader read(json, report, where + " animator");
            read.Text("clip", animator.clip, false);
            read.Flag("playing", animator.playing, false);
            read.Flag("loop", animator.loop, false);
            read.Number("speed", animator.speed, false);
            if (json.HasArray("sources"))
            {
                for (const auto& source : json.GetCurrentNode().at("sources"))
                {
                    if (source.is_string())
                    {
                        animator.sources.push_back(source.get<std::string>());
                    }
                    else
                    {
                        read.Warn("an entry of \"sources\" is not text - skipped");
                    }
                }
            }
            json.LeaveObject();
            return animator;
        }

        SkyComponent ReadSky(JSONSerializer& json, SceneLoadReport& report, const std::string& where)
        {
            SkyComponent sky;
            json.EnterObject("sky");
            FieldReader read(json, report, where + " sky");
            for (const auto& [key, value] : SkyFields(sky))
            {
                read.Number(key, *value, false);
            }
            read.Text("picture", sky.picture, false);
            read.Number("ambient", sky.ambient, false);
            json.LeaveObject();
            return sky;
        }

        RigidBodyComponent ReadRigidBody(JSONSerializer& json, SceneLoadReport& report, const std::string& where)
        {
            RigidBodyComponent body;
            json.EnterObject("rigid_body");
            FieldReader read(json, report, where + " rigid body");
            std::string type = BodyTypeName(body.type);
            read.Text("type", type);
            if (const auto parsed = ParseBodyType(type))
            {
                body.type = *parsed;
            }
            else
            {
                read.Warn(std::format("unknown body type \"{}\" - using dynamic", type));
            }
            read.Number("mass", body.mass);
            if (!(body.mass > 0.0f)) // also catches NaN
            {
                read.Warn("\"mass\" must be above 0 - using 1");
                body.mass = 1.0f;
            }

            // Not required, like the collider below: older rigid bodies were saved without them
            read.Number("gravity_scale", body.gravityScale, false);
            read.Number("friction", body.friction, false);
            if (!(body.friction >= 0.0f)) // also catches NaN
            {
                read.Warn("\"friction\" must be 0 or more - using 0.5");
                body.friction = RigidBodyComponent{}.friction;
            }
            read.Number("bounciness", body.bounciness, false);
            read.Text("surface", body.surface, false);
            read.Flag("keep_upright", body.keepUpright, false);
            read.Flag("lock_move_x", body.lockMoveX, false);
            read.Flag("lock_move_y", body.lockMoveY, false);
            read.Flag("lock_move_z", body.lockMoveZ, false);
            read.Flag("lock_turn_x", body.lockTurnX, false);
            read.Flag("lock_turn_y", body.lockTurnY, false);
            read.Flag("lock_turn_z", body.lockTurnZ, false);
            read.WholeNumber("layers", body.layers, false);
            read.Number("start_velocity_x", body.startVelocityX, false);
            read.Number("start_velocity_y", body.startVelocityY, false);
            read.Number("start_velocity_z", body.startVelocityZ, false);
            if (!(body.bounciness >= 0.0f && body.bounciness <= 1.0f))
            {
                const float kept = std::isnan(body.bounciness) ? 0.0f : std::clamp(body.bounciness, 0.0f, 1.0f);
                read.Warn(std::format("\"bounciness\" must be from 0 to 1 - using {}", kept));
                body.bounciness = kept;
            }

            // The collider: not required, since rigid bodies saved before collider shapes existed have none (Auto)
            std::string shape = ColliderShapeName(body.shape);
            read.Text("shape", shape, false);
            if (const auto parsed = ParseColliderShape(shape))
            {
                body.shape = *parsed;
            }
            else
            {
                read.Warn(std::format("unknown collider shape \"{}\" - using auto", shape));
            }
            read.Number("center_x", body.centerX, false);
            read.Number("center_y", body.centerY, false);
            read.Number("center_z", body.centerZ, false);
            const RigidBodyComponent defaults;
            const auto positive = [&read](const char* key, float& value, float fallback)
            {
                read.Number(key, value, false);
                if (!(value > 0.0f))
                {
                    read.Warn(std::format("\"{}\" must be above 0 - using {}", key, fallback));
                    value = fallback;
                }
            };
            positive("size_x", body.sizeX, defaults.sizeX);
            positive("size_y", body.sizeY, defaults.sizeY);
            positive("size_z", body.sizeZ, defaults.sizeZ);
            positive("radius", body.radius, defaults.radius);
            positive("height", body.height, defaults.height);
            if (json.HasArray("parts"))
            {
                json.EnterArray("parts");
                const std::size_t count = json.GetCurrentNode().size();
                for (std::size_t i = 0; i < count; ++i)
                {
                    json.EnterArrayElement(i);
                    FieldReader fields(json, report, std::format("{} rigid_body part {}", where, i));
                    ColliderPart part;
                    std::string partShape = ColliderShapeName(part.shape);
                    fields.Text("shape", partShape, false);
                    const auto parsed = ParseColliderShape(partShape);
                    if (parsed == ColliderShape::Box || parsed == ColliderShape::Sphere ||
                        parsed == ColliderShape::Capsule)
                    {
                        part.shape = *parsed;
                    }
                    else
                    {
                        fields.Warn(std::format("a part cannot be \"{}\" - using box", partShape));
                    }
                    fields.Number("center_x", part.centerX, false);
                    fields.Number("center_y", part.centerY, false);
                    fields.Number("center_z", part.centerZ, false);
                    fields.Number("size_x", part.sizeX, false);
                    fields.Number("size_y", part.sizeY, false);
                    fields.Number("size_z", part.sizeZ, false);
                    fields.Number("radius", part.radius, false);
                    fields.Number("height", part.height, false);
                    body.parts.push_back(part);
                    json.LeaveObject();
                }
                json.LeaveArray();
            }
            json.LeaveObject();
            return body;
        }

        // Where blocks are read into, which decides the blocks that may be there: a script's own list, the inside of a
        // C block, condition slots, or the value blocks in place of numbers
        enum class BlockPlace
        {
            Script,
            Inside,
            Slots,
            Inputs
        };

        // Reads the list of blocks under key (if there is one). Blocks of an unknown kind, and blocks that cannot be
        // where they are (a condition in a stack, a hat that does not start a script, a stack block in a slot), are
        // left out with a warning; a slot or input keeps its place, empty.
        std::vector<Logic::Block> ReadBlocks(JSONSerializer& json, const std::string& key, SceneLoadReport& report,
                                             const std::string& where, BlockPlace place)
        {
            std::vector<Logic::Block> blocks;
            if (!json.HasArray(key))
            {
                return blocks;
            }
            json.EnterArray(key);
            const std::size_t count = json.GetCurrentNode().size();
            for (std::size_t i = 0; i < count; ++i)
            {
                json.EnterArrayElement(i);
                const std::string at = std::format("{} {} {}", where, key, i);
                std::optional<Logic::Block> read;
                if (json.GetCurrentNode().is_object() && json.HasKey("block"))
                {
                    std::string name;
                    FieldReader fields(json, report, at);
                    fields.Text("block", name);
                    if (const auto op = Logic::OpNamed(name))
                    {
                        Logic::Block block = Logic::MakeBlock(*op, 0);
                        int id = 0;
                        fields.WholeNumber("id", id);
                        block.id = id > 0 ? static_cast<std::uint32_t>(id) : 0;
                        if (json.HasArray("numbers"))
                        {
                            const auto& numbers = json.GetCurrentNode().at("numbers");
                            for (std::size_t n = 0; n < numbers.size() && n < block.numbers.size(); ++n)
                            {
                                if (numbers[n].is_number())
                                {
                                    block.numbers[n] = numbers[n].get<float>();
                                }
                                else
                                {
                                    fields.Warn(
                                        std::format("number {} is not a number - using {}", n, block.numbers[n]));
                                }
                            }
                        }
                        fields.Text("text", block.text, false);
                        block.inputs = ReadBlocks(json, "inputs", report, at, BlockPlace::Inputs);
                        block.inputs.resize(*op == Logic::Op::Call ? block.numbers.size() : Logic::NumberCount(*op));
                        block.slots = ReadBlocks(json, "slots", report, at, BlockPlace::Slots);
                        block.slots.resize(Logic::SlotCount(*op));
                        block.body = ReadBlocks(json, "body", report, at, BlockPlace::Inside);
                        block.elseBody = ReadBlocks(json, "else", report, at, BlockPlace::Inside);
                        if (!Logic::InfoOf(*op).hasBody)
                        {
                            block.body.clear();
                        }
                        if (!Logic::InfoOf(*op).hasElse)
                        {
                            block.elseBody.clear();
                        }
                        read = std::move(block);
                    }
                    else
                    {
                        fields.Warn(std::format("unknown block \"{}\" - left out", name));
                    }
                }
                else if ((place != BlockPlace::Slots && place != BlockPlace::Inputs) ||
                         !json.GetCurrentNode().is_object())
                {
                    report.warnings.push_back(at + ": not a block - left out");
                }

                if (read)
                {
                    // A hat only starts a script, and a condition or value is either in its slot or lying loose on
                    // the canvas on its own (a script of just that block)
                    using Logic::BlockShape;
                    const BlockShape shape = Logic::InfoOf(read->op).shape;
                    const auto loose = [](BlockShape each)
                    { return each == BlockShape::Condition || each == BlockShape::Value; };
                    const bool startsScript = place == BlockPlace::Script && blocks.empty();
                    const bool afterLooseCondition =
                        place == BlockPlace::Script && !blocks.empty() && loose(Logic::InfoOf(blocks.front().op).shape);
                    bool fits = false;
                    if (place == BlockPlace::Slots)
                    {
                        fits = shape == BlockShape::Condition;
                    }
                    else if (place == BlockPlace::Inputs)
                    {
                        fits = shape == BlockShape::Value;
                    }
                    else if (loose(shape) || shape == BlockShape::Hat)
                    {
                        fits = startsScript;
                    }
                    else
                    {
                        fits = !afterLooseCondition;
                    }
                    if (!fits)
                    {
                        report.warnings.push_back(std::format("{}: a \"{}\" block cannot go there - left out", at,
                                                              Logic::InfoOf(read->op).name));
                        read.reset();
                    }
                }
                if (read)
                {
                    blocks.push_back(std::move(*read));
                }
                else if (place == BlockPlace::Slots || place == BlockPlace::Inputs)
                {
                    blocks.emplace_back(); // the slot stays, empty
                }
                json.LeaveObject();
            }
            json.LeaveArray();
            return blocks;
        }

        // Block ids must be unique and above 0 (the runtime reports blocks by id): others get new ones
        // (in slots and inputs, id 0 is an empty one, not a block)
        void FixBlockIds(std::vector<Logic::Block>& blocks, bool slots, std::set<std::uint32_t>& used,
                         std::uint32_t& nextId, bool& fixed)
        {
            for (Logic::Block& block : blocks)
            {
                if (slots && block.id == 0)
                {
                    continue;
                }
                if (!used.insert(block.id).second)
                {
                    block.id = nextId++;
                    used.insert(block.id);
                    fixed = true;
                }
                FixBlockIds(block.inputs, true, used, nextId, fixed);
                FixBlockIds(block.slots, true, used, nextId, fixed);
                FixBlockIds(block.body, false, used, nextId, fixed);
                FixBlockIds(block.elseBody, false, used, nextId, fixed);
            }
        }

        Logic::LogicDesc ReadLogic(JSONSerializer& json, SceneLoadReport& report, const std::string& where)
        {
            Logic::LogicDesc logic;
            json.EnterObject("logic");
            const std::string at = where + " logic";
            // Variables: each needs a name of its own; a repeated or missing one is left out
            if (json.HasArray("variables"))
            {
                json.EnterArray("variables");
                const std::size_t count = json.GetCurrentNode().size();
                for (std::size_t i = 0; i < count; ++i)
                {
                    json.EnterArrayElement(i);
                    const std::string variable = std::format("{} variable {}", at, i);
                    Logic::Variable read;
                    if (json.GetCurrentNode().is_object())
                    {
                        FieldReader fields(json, report, variable);
                        fields.Text("name", read.name);
                        fields.Number("value", read.value, false);
                        std::string kind;
                        fields.Text("kind", kind, false);
                        if (kind == "true_false")
                        {
                            read.kind = Logic::VariableKind::TrueFalse;
                        }
                        else if (!kind.empty())
                        {
                            fields.Warn("kind \"" + kind + "\" is not known - read as a number");
                        }
                    }
                    if (Logic::CanAddVariable(logic, read.name))
                    {
                        logic.variables.push_back(std::move(read));
                    }
                    else
                    {
                        report.warnings.push_back(variable + ": no name, or a name already used - left out");
                    }
                    json.LeaveObject();
                }
                json.LeaveArray();
            }
            if (json.HasArray("scripts"))
            {
                json.EnterArray("scripts");
                const std::size_t count = json.GetCurrentNode().size();
                for (std::size_t i = 0; i < count; ++i)
                {
                    json.EnterArrayElement(i);
                    const std::string script = std::format("{} script {}", at, i);
                    if (json.GetCurrentNode().is_object())
                    {
                        Logic::Script read;
                        FieldReader fields(json, report, script);
                        fields.Number("x", read.x);
                        fields.Number("y", read.y);
                        read.blocks = ReadBlocks(json, "blocks", report, script, BlockPlace::Script);
                        if (!read.blocks.empty())
                        {
                            logic.scripts.push_back(std::move(read));
                        }
                    }
                    else
                    {
                        report.warnings.push_back(script + ": not a JSON object - skipped");
                    }
                    json.LeaveObject();
                }
                json.LeaveArray();
            }
            else
            {
                report.warnings.push_back(at + ": missing \"scripts\"");
            }
            json.LeaveObject();

            std::set<std::uint32_t> used{0};
            std::uint32_t nextId = Logic::NextBlockId(logic);
            bool fixed = false;
            for (Logic::Script& script : logic.scripts)
            {
                FixBlockIds(script.blocks, false, used, nextId, fixed);
            }
            if (fixed)
            {
                report.warnings.push_back(at + ": blocks without a valid or with a repeated \"id\" - given new ones");
            }
            return logic;
        }

        // A link without a prefab path or object is no use to anyone, so it is dropped (the object stays)
        std::optional<PrefabInstance> ReadPrefabInstance(JSONSerializer& json, SceneLoadReport& report,
                                                         const std::string& where)
        {
            PrefabInstance link;
            json.EnterObject("prefab");
            FieldReader read(json, report, where + " prefab");
            read.Text("path", link.prefab);
            read.Text("guid", link.guid, false);
            int object = 0;
            read.WholeNumber("object", object);
            link.object = object > 0 ? static_cast<std::uint32_t>(object) : 0;
            if (json.HasArray("overrides"))
            {
                for (const auto& property : json.GetCurrentNode().at("overrides"))
                {
                    if (property.is_string())
                    {
                        link.overrides.push_back(property.get<std::string>());
                    }
                    else
                    {
                        read.Warn("an entry of \"overrides\" is not text - skipped");
                    }
                }
                std::sort(link.overrides.begin(), link.overrides.end());
                link.overrides.erase(std::unique(link.overrides.begin(), link.overrides.end()), link.overrides.end());
            }
            json.LeaveObject();
            if (link.prefab.empty() || link.object == 0)
            {
                read.Warn("needs a \"path\" and an \"object\" above 0 - no longer linked to a prefab");
                return std::nullopt;
            }
            return link;
        }

        // The current JSON object as a game object. label says where it is in warnings, e.g. "object 3".
        GameObject ReadGameObject(JSONSerializer& json, SceneLoadReport& report, const std::string& label)
        {
            GameObject object;
            FieldReader read(json, report, label);
            int id = 0;
            read.WholeNumber("id", id);
            object.id = id > 0 ? static_cast<std::uint32_t>(id) : 0;
            read.Text("name", object.name);
            read.Text("lua_file", object.luaFile, false);
            read.Text("lua_source", object.luaSource, false);
            int parent = 0;
            read.WholeNumber("parent", parent, false);
            object.parent = parent > 0 ? static_cast<std::uint32_t>(parent) : 0;

            const std::string where = std::format("{} ({})", label, object.name);
            // Only the components the object lists; unknown ones (such as a newer build's) are skipped
            if (json.HasObject("transform"))
            {
                object.transform = ReadTransform(json, report, where);
            }
            if (json.HasObject("renderable"))
            {
                object.renderable = ReadRenderable(json, report, where);
            }
            if (json.HasObject("audio_source"))
            {
                object.audioSource = ReadAudioSource(json, report, where);
            }
            if (json.HasObject("player"))
            {
                object.player = ReadPlayer(json, report, where);
            }
            if (json.HasObject("camera"))
            {
                object.camera = ReadCamera(json, report, where);
            }
            if (json.HasObject("light"))
            {
                object.light = ReadLight(json, report, where);
            }
            if (json.HasObject("screen"))
            {
                object.screen = ReadScreen(json, report, where);
            }
            if (json.HasObject("animator"))
            {
                object.animator = ReadAnimator(json, report, where);
            }
            if (json.HasObject("sky"))
            {
                object.sky = ReadSky(json, report, where);
            }
            if (json.HasObject("rigid_body"))
            {
                object.rigidBody = ReadRigidBody(json, report, where);
            }
            if (json.HasObject("logic"))
            {
                object.logic = ReadLogic(json, report, where);
            }
            if (json.HasObject("prefab"))
            {
                object.prefabInstance = ReadPrefabInstance(json, report, where);
            }
            return object;
        }

        // Ids must be unique and above 0 (the runtime finds an object's entity by id): a missing, invalid or
        // repeated one is replaced with a new id
        void FixIds(std::vector<GameObject>& objects, SceneLoadReport& report, const std::string& what)
        {
            std::uint32_t nextId = std::accumulate(objects.begin(), objects.end(), std::uint32_t{1},
                                                   [](std::uint32_t next, const GameObject& object)
                                                   { return std::max(next, object.id + 1); });
            std::set<std::uint32_t> used;
            for (std::size_t i = 0; i < objects.size(); ++i)
            {
                GameObject& object = objects[i];
                if (object.id != 0 && used.insert(object.id).second)
                {
                    continue;
                }
                report.warnings.push_back(std::format("{} {} ({}): {} - using id {}", what, i, object.name,
                                                      object.id == 0 ? "no valid \"id\"" : "repeated \"id\"", nextId));
                object.id = nextId++;
                used.insert(object.id);
            }
        }

        // A parent must be another object in the same file, and following the parents must never come back round to
        // where it started: otherwise the object goes to the top of the scene
        void FixParents(std::vector<GameObject>& objects, SceneLoadReport& report, const std::string& what)
        {
            SceneDescription scene{.objects = std::move(objects)};
            for (std::size_t i = 0; i < scene.objects.size(); ++i)
            {
                GameObject& object = scene.objects[i];
                if (object.parent == 0)
                {
                    continue;
                }
                const bool missing = FindObject(scene, object.parent) == nullptr;
                if (missing || object.parent == object.id || IsAncestorOf(scene, object.id, object.parent))
                {
                    report.warnings.push_back(std::format(
                        "{} {} ({}): {} {} - placed at the top of the scene", what, i, object.name,
                        missing ? "no object has the parent id" : "a loop of parents through", object.parent));
                    object.parent = 0;
                }
            }
            objects = std::move(scene.objects);
        }
    }

    void WriteObjects(JSONSerializer& json, const std::vector<GameObject>& objects)
    {
        json.CreateArray("objects");
        json.EnterArray("objects");
        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            json.CreateObject();
            json.EnterArrayElement(i);
            WriteGameObject(json, objects[i]);
            json.LeaveObject();
        }
        json.LeaveArray();
    }

    std::vector<GameObject> ReadObjects(JSONSerializer& json, SceneLoadReport& report, const std::string& what)
    {
        std::vector<GameObject> objects;
        json.EnterArray("objects");
        const std::size_t count = json.GetCurrentNode().size();
        for (std::size_t i = 0; i < count; ++i)
        {
            json.EnterArrayElement(i);
            if (json.GetCurrentNode().is_object())
            {
                objects.push_back(ReadGameObject(json, report, std::format("{} {}", what, i)));
            }
            else
            {
                report.warnings.push_back(std::format("{} {}: not a JSON object - skipped", what, i));
            }
            json.LeaveObject();
        }
        json.LeaveArray();

        FixIds(objects, report, what);
        FixParents(objects, report, what);
        return objects;
    }

    void WriteBlockList(JSONSerializer& json, const std::string& key, const std::vector<Logic::Block>& blocks)
    {
        WriteBlocks(json, key, blocks);
    }

    std::vector<Logic::Block> ReadBlockList(JSONSerializer& json, const std::string& key, SceneLoadReport& report,
                                            const std::string& where)
    {
        // Read as the inside of a C block: stack blocks only, no hats and no loose conditions or values
        std::vector<Logic::Block> blocks = ReadBlocks(json, key, report, where, BlockPlace::Inside);
        std::set<std::uint32_t> used{0};
        std::uint32_t nextId = 1;
        Logic::Renumber(blocks, nextId); // its own ids, from 1
        bool fixed = false;
        FixBlockIds(blocks, false, used, nextId, fixed);
        return blocks;
    }

    nlohmann::json ToJson(const GameObject& object)
    {
        JSONSerializer json;
        json.ParseJSON("{}");
        WriteGameObject(json, object);
        return json.GetCurrentNode();
    }

    GameObject FromJson(const nlohmann::json& node)
    {
        JSONSerializer json;
        json.SetCurrentNode(node);
        SceneLoadReport ignored; // made by ToJson (or a copy of it), so nothing is missing
        return ReadGameObject(json, ignored, "object");
    }
}
