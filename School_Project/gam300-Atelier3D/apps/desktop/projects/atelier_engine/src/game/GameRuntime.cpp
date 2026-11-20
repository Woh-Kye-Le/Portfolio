/* !
@file    	    GameRuntime.cpp
@author  	    c.kojat@digipen.edu [58%]
         	    w.kyele@digipen.edu [24%]
         	    j.cai@digipen.edu [10%]
         	    sung.h@digipen.edu [8%]
@date    		26/09/2026
@brief
    Implements GameRuntime: asset loading and release, building and editing the
    ECS world, the entity-spawn demo, and the per-frame update of the spin,
    player, camera, audio and render systems.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/game/GameRuntime.hpp"

#include "engine/assets/MeshSimplifier.hpp"
#include "engine/core/FixedSteps.hpp"
#include "engine/core/Log.hpp"
#include "engine/game/AssetLayout.hpp"
#include "engine/graphics/Renderer/RenderAPI.hpp"
#include "engine/messenger/Events.hpp"
#include "engine/platform/Window.hpp"
#include "engine/scene/EntitySpawner.hpp"
#include "engine/scene/Hierarchy.hpp"
#include "engine/scene/components/AnimatorComponent.hpp"
#include "engine/scene/systems/AnimationSystem.hpp"
#include "engine/scene/systems/AudioSystem.hpp"
#include "engine/scene/systems/HierarchySystem.hpp"
#include "engine/scene/systems/PhysicsSystem.hpp"
#include "engine/scene/systems/PlayerControllerSystem.hpp"
#include "engine/scene/systems/RenderSystem.hpp"
#include "engine/scene/systems/SpinSystem.hpp"
#include "engine/scripting/ScriptSource.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>

namespace Engine::Game
{
    namespace
    {
        // Whether two logics run the same: where a script sits on the Blocks canvas does not change what it does
        bool SameBlocks(const std::optional<Logic::LogicDesc>& a, const std::optional<Logic::LogicDesc>& b)
        {
            if (a.has_value() != b.has_value())
            {
                return false;
            }
            if (!a)
            {
                return true;
            }
            Logic::LogicDesc first = *a;
            Logic::LogicDesc second = *b;
            for (Logic::LogicDesc* logic : {&first, &second})
            {
                for (Logic::Script& script : logic->scripts)
                {
                    script.x = 0.0f;
                    script.y = 0.0f;
                }
            }
            return first == second;
        }

        // Formats Assets::LoadAudioFile can decode. FMOD plays plenty more; these are just the ones the
        // ResourceManager can turn into samples.
        constexpr std::array<std::string_view, 2> kDecodableAudioExtensions{".wav", ".ogg"};

        bool HasExtension(const std::string& path, std::span<const std::string_view> extensions)
        {
            std::string extension = std::filesystem::path(path).extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return std::find(extensions.begin(), extensions.end(), extension) != extensions.end();
        }

        // ---- How the game holds each component of a scene object
        //
        // A binding says, for one component of a GameObject: how it becomes ECS components on an entity (Add), how an
        // edit of it reaches a running entity (Update) and how the entity's values are read back (Read). kComponents
        // lists them: it is the only list of components in the runtime. A component the game holds as it is (the ECS
        // one is the scene one) is one line, Plain; one it holds differently has a binding of its own, with its three
        // directions together.

        using Engine::Scene::Entity;
        using Engine::Scene::GameObject;

        // An edit of a running entity: before and after are the object's two versions
        struct Edit
        {
            Engine::Scene::Scene& ecs;
            Entity entity;
            const GameObject& before;
            const GameObject& after;
            GameObject& changed; // gets what is to be added back, by the same code that built the entity (Add)
            const std::function<void(Entity)>& stopVoice;
        };

        template <class Component>
        void AddOrThrow(Engine::Scene::Scene& ecs, Entity entity, const Component& component, const char* what)
        {
            if (!ecs.AddComponent(entity, component))
            {
                throw std::runtime_error(std::string("Could not add the ECS ") + what + " component");
            }
        }

        template <class Member> struct ComponentTypeOf;
        template <class Component> struct ComponentTypeOf<std::optional<Component> GameObject::*>
        {
            using Type = Component;
        };

        // A component the game holds as it is: the same struct in the scene object and in the ECS
        template <auto Member> struct Plain
        {
            using Type = typename ComponentTypeOf<decltype(Member)>::Type;
            const char* name; // for the error if it cannot be added

            void Add(Engine::Scene::Scene& ecs, Entity entity, const GameObject& object, const auto& /*assets*/,
                     const GameProject* /*project*/, std::vector<std::filesystem::path>& /*clips*/) const
            {
                if (const auto& component = object.*Member)
                {
                    AddOrThrow(ecs, entity, *component, name);
                }
            }
            void Update(const Edit& edit) const
            {
                if (edit.before.*Member != edit.after.*Member)
                {
                    edit.ecs.template RemoveComponent<Type>(edit.entity);
                    edit.changed.*Member = edit.after.*Member;
                }
            }
            void Read(const Engine::Scene::Scene& ecs, Entity entity, GameObject& object) const
            {
                object.*Member = ecs.template GetComponent<Type>(entity);
            }
        };

        struct TransformBinding
        {
            void Add(Engine::Scene::Scene& ecs, Entity entity, const GameObject& object, const auto& /*assets*/,
                     const GameProject* /*project*/, std::vector<std::filesystem::path>& /*clips*/) const
            {
                if (object.transform)
                {
                    if (!ecs.SetTransform(entity, *object.transform))
                    {
                        throw std::runtime_error("Could not set the ECS transform");
                    }
                }
                else
                {
                    ecs.RemoveComponent<Engine::Scene::TransformComponent>(entity); // CreateEntity adds one
                }
            }
            void Update(const Edit& edit) const
            {
                Engine::Scene::Scene& ecs = edit.ecs;
                if (edit.before.transform != edit.after.transform)
                {
                    if (edit.after.transform)
                    {
                        // Set, not re-added: the entity keeps its other live state. A child's Transform is relative to
                        // its parent, so it goes into the link (HierarchySystem places the entity from it).
                        if (!ecs.HasComponent<Engine::Scene::TransformComponent>(edit.entity))
                        {
                            ecs.AddComponent<Engine::Scene::TransformComponent>(edit.entity);
                        }
                        if (auto* link = ecs.TryGetComponent<Engine::Scene::HierarchyComponent>(edit.entity))
                        {
                            link->local = *edit.after.transform;
                        }
                        else
                        {
                            ecs.SetTransform(edit.entity, *edit.after.transform);
                        }
                    }
                    else
                    {
                        ecs.RemoveComponent<Engine::Scene::TransformComponent>(edit.entity);
                    }
                }
                // Handled above: passing the live one leaves it as it is
                edit.changed.transform = ecs.GetTransform(edit.entity);
            }
            void Read(const Engine::Scene::Scene& ecs, Entity entity, GameObject& object) const
            {
                // A child's Transform is where it is in the world; the blocks and the Inspector use the one relative
                // to its parent
                const auto* link = ecs.TryGetComponent<Engine::Scene::HierarchyComponent>(entity);
                object.transform = link ? std::optional(link->local) : ecs.GetTransform(entity);
            }
        };

        // What an object's Renderable draws with, as the ECS component (assets: its loaded files)
        Engine::Scene::MeshRendererComponent MeshRendererOf(const Engine::Scene::RenderableDesc& renderable,
                                                            const auto& assets)
        {
            Engine::Scene::MeshRendererComponent meshRenderer;
            meshRenderer.shape = renderable.shape;
            meshRenderer.mesh = assets.mesh;
            meshRenderer.model = assets.model;
            meshRenderer.texture = assets.texture;
            meshRenderer.shader = assets.shader;
            meshRenderer.albedoR = renderable.albedoR;
            meshRenderer.albedoG = renderable.albedoG;
            meshRenderer.albedoB = renderable.albedoB;
            meshRenderer.albedoA = renderable.albedoA;
            meshRenderer.shapeSize = {renderable.shapeSizeX, renderable.shapeSizeY, renderable.shapeSizeZ};
            meshRenderer.shapeCenter = {renderable.shapeCenterX, renderable.shapeCenterY, renderable.shapeCenterZ};
            meshRenderer.material = assets.material;
            meshRenderer.modelTurn = renderable.modelTurn;
            meshRenderer.holdBone = renderable.holdBone;
            meshRenderer.viewModel = renderable.viewModel;
            if (assets.bounds)
            {
                // Turned with the model, so the collider fits it as it is drawn
                const Assets::BoundingBox box = Assets::Turned(*assets.bounds, renderable.modelTurn);
                meshRenderer.hasBounds = true;
                meshRenderer.boundsMin = box.min;
                meshRenderer.boundsMax = box.max;
            }
            return meshRenderer;
        }

        // Drawn by a mesh renderer, and spun by a spin component
        struct RenderableBinding
        {
            void Add(Engine::Scene::Scene& ecs, Entity entity, const GameObject& object, const auto& assets,
                     const GameProject* /*project*/, std::vector<std::filesystem::path>& /*clips*/) const
            {
                if (const auto& renderable = object.renderable)
                {
                    AddOrThrow(ecs, entity, MeshRendererOf(*renderable, assets), "mesh renderer");
                    AddOrThrow(ecs, entity, Engine::Scene::SpinComponent{.degreesPerSecond = renderable->rotationSpeed},
                               "spin");
                }
            }
            void Update(const Edit& edit) const
            {
                if (edit.before.renderable != edit.after.renderable)
                {
                    edit.ecs.RemoveComponent<Engine::Scene::MeshRendererComponent>(edit.entity);
                    edit.ecs.RemoveComponent<Engine::Scene::SpinComponent>(edit.entity);
                    edit.changed.renderable = edit.after.renderable;
                }
            }
            void Read(const Engine::Scene::Scene& ecs, Entity entity, GameObject& object) const
            {
                // The colour is what blocks change; the rest (shape, files, spin) is as saved
                const auto renderer = ecs.GetComponent<Engine::Scene::MeshRendererComponent>(entity);
                if (renderer && object.renderable)
                {
                    Engine::Scene::RenderableDesc& saved = *object.renderable;
                    saved.albedoR = renderer->albedoR;
                    saved.albedoG = renderer->albedoG;
                    saved.albedoB = renderer->albedoB;
                    saved.albedoA = renderer->albedoA;
                }
            }
        };

        // Played by an audio source component; its clip is an index into a table of files
        struct AudioBinding
        {
            void Add(Engine::Scene::Scene& ecs, Entity entity, const GameObject& object, const auto& /*assets*/,
                     const GameProject* project, std::vector<std::filesystem::path>& clips) const
            {
                if (const auto& audio = object.audioSource; audio && !audio->clip.empty())
                {
                    clips.push_back(project->ProjectDir() / audio->clip);

                    Engine::Scene::AudioSourceComponent source;
                    source.clip = static_cast<std::uint32_t>(clips.size()); // one-based index into the table
                    source.volume = audio->volume;
                    source.minDistance = audio->minDistance;
                    source.maxDistance = audio->maxDistance;
                    source.loop = audio->loop;
                    source.spatial = audio->spatial;
                    source.playOnStart = audio->playOnStart;
                    source.bus = Audio::BusFromString(audio->bus);
                    AddOrThrow(ecs, entity, source, "audio source");
                }
            }
            void Update(const Edit& edit) const
            {
                if (edit.before.audioSource == edit.after.audioSource)
                {
                    return;
                }
                auto* source = edit.ecs.TryGetComponent<Engine::Scene::AudioSourceComponent>(edit.entity);
                const auto restartFields = [](Engine::Scene::AudioSourceDesc audio)
                {
                    // What a playing voice can take on the fly: the AudioSystem pushes these every frame
                    audio.volume = 1.0f;
                    audio.minDistance = 0.0f;
                    audio.maxDistance = 0.0f;
                    audio.playOnStart = true;
                    return audio;
                };
                if (source && edit.before.audioSource && edit.after.audioSource &&
                    restartFields(*edit.before.audioSource) == restartFields(*edit.after.audioSource))
                {
                    source->volume = edit.after.audioSource->volume;
                    source->minDistance = edit.after.audioSource->minDistance;
                    source->maxDistance = edit.after.audioSource->maxDistance;
                    source->playOnStart = edit.after.audioSource->playOnStart;
                }
                else
                {
                    edit.stopVoice(edit.entity);
                    edit.ecs.RemoveComponent<Engine::Scene::AudioSourceComponent>(edit.entity);
                    edit.changed.audioSource = edit.after.audioSource;
                }
            }
            void Read(const Engine::Scene::Scene& /*ecs*/, Entity /*entity*/, GameObject& /*object*/) const
            {
                // Nothing a block or a frame changes: what is saved is what plays (its playback state is not a value)
            }
        };

        // Moved on by an animator component; the extra animation files it can use are the ones loaded with the assets
        struct AnimatorBinding
        {
            void Add(Engine::Scene::Scene& ecs, Entity entity, const GameObject& object, const auto& assets,
                     const GameProject* /*project*/, std::vector<std::filesystem::path>& /*clips*/) const
            {
                if (const auto& animator = object.animator)
                {
                    Engine::Scene::AnimatorComponent live = *animator;
                    live.sourceModels = assets.animationSources;
                    AddOrThrow(ecs, entity, live, "animator");
                }
            }
            void Update(const Edit& edit) const
            {
                if (edit.before.animator != edit.after.animator)
                {
                    edit.ecs.RemoveComponent<Engine::Scene::AnimatorComponent>(edit.entity);
                    edit.changed.animator = edit.after.animator;
                }
            }
            void Read(const Engine::Scene::Scene& /*ecs*/, Entity /*entity*/, GameObject& /*object*/) const
            {
                // Where it is in its clip is the game's, not a value: what is saved is what plays
            }
        };

        // Every component of a GameObject, in the order they are added: the only list of them in the runtime
        constexpr auto kComponents = std::tuple{
            TransformBinding{},
            RenderableBinding{},
            Plain<&GameObject::player>{"player controller"},
            Plain<&GameObject::camera>{"camera"},
            Plain<&GameObject::light>{"light"},
            Plain<&GameObject::screen>{"screen element"},
            AnimatorBinding{},
            Plain<&GameObject::sky>{"sky"},
            Plain<&GameObject::rigidBody>{"rigid body"},
            AudioBinding{},
        };

        template <class Function> void ForEachComponent(Function&& function)
        {
            std::apply([&function](const auto&... binding) { (function(binding), ...); }, kComponents);
        }

        std::string ToLower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        // "play sound": as loud as can be this close to the camera (which sits a few units behind the player), and
        // gone by this far
        constexpr float kBlockSoundFullVolumeDistance = 6.0f;
        constexpr float kBlockSoundSilentDistance = 40.0f;

        // A step that moved an object further than this put it there ("go to", a respawn): it is not smoothed
        constexpr float kMaxSmoothedDistance = 5.0f; // per step, so 300 units a second
        constexpr float kMaxSmoothedDegrees = 45.0f; // per step

        // Where every object that is not a child is (children are placed from their parents by the HierarchySystem)
        std::vector<std::pair<Engine::Scene::Entity, Engine::Scene::TransformComponent>> TopLevelTransforms(
            Engine::Scene::Scene& scene)
        {
            std::vector<std::pair<Engine::Scene::Entity, Engine::Scene::TransformComponent>> found;
            scene.ForEach<const Engine::Scene::TransformComponent>(
                [&](Engine::Scene::Entity entity, const Engine::Scene::TransformComponent& transform)
                {
                    if (!scene.HasComponent<Engine::Scene::HierarchyComponent>(entity))
                    {
                        found.emplace_back(entity, transform);
                    }
                });
            return found;
        }

        // How far a step moved and turned each object that was at before, for the ones it moved smoothly
        std::vector<std::pair<Engine::Scene::Entity, Engine::Scene::TransformComponent>> StepMoves(
            const Engine::Scene::Scene& scene,
            const std::vector<std::pair<Engine::Scene::Entity, Engine::Scene::TransformComponent>>& before)
        {
            std::vector<std::pair<Engine::Scene::Entity, Engine::Scene::TransformComponent>> moves;
            for (const auto& [entity, was] : before)
            {
                const auto now = scene.GetTransform(entity);
                if (!now || *now == was)
                {
                    continue;
                }
                Engine::Scene::TransformComponent move;
                move.x = now->x - was.x;
                move.y = now->y - was.y;
                move.z = now->z - was.z;
                // The short way round: 350 to 10 degrees turned 20, not -340
                move.rotation = std::remainder(now->rotation - was.rotation, 360.0f);
                move.rotationX = std::remainder(now->rotationX - was.rotationX, 360.0f);
                move.rotationZ = std::remainder(now->rotationZ - was.rotationZ, 360.0f);
                const float distance = std::sqrt(move.x * move.x + move.y * move.y + move.z * move.z);
                const float turn =
                    std::max({std::abs(move.rotation), std::abs(move.rotationX), std::abs(move.rotationZ)});
                if (distance <= kMaxSmoothedDistance && turn <= kMaxSmoothedDegrees)
                {
                    moves.emplace_back(entity, move);
                }
            }
            return moves;
        }
    }

    GameRuntime::GameRuntime() = default;

    GameRuntime::~GameRuntime()
    {
        Stop();
    }

    // Which game to play; Start() does the actual loading
    void GameRuntime::LoadProject(GameProject project)
    {
        // A game made before models had folders of their own is brought to the current layout first, so its scenes
        // find their files (a game that is already there is left alone)
        if (NeedsLayoutMigration(project.ProjectDir()))
        {
            const LayoutMigration migration = MigrateAssetLayout(project.ProjectDir());
            LOG_INFO(IO, "Moved {} asset files of '{}' to the current folder layout ({} files updated)",
                     migration.moved.size(), project.Settings().name, migration.rewritten);
            for (const std::string& warning : migration.warnings)
            {
                LOG_WARN(IO, "Asset layout: {}", warning);
            }
        }
        project_ = std::make_unique<GameProject>(std::move(project));
    }

    // Plays the game's start scene (from game.cfg)
    bool GameRuntime::Start()
    {
        if (!EnsureProject())
        {
            return false;
        }
        return Start(project_->LoadStartScene());
    }

    // Falls back to the first game it can find if nobody called LoadProject
    bool GameRuntime::EnsureProject()
    {
        if (project_)
        {
            return true;
        }
        const auto dir = GameProject::FindDefaultGameProjectDir();
        if (dir.empty())
        {
            LOG_ERROR(IO, "No game project found in {}", GameProject::ProjectsRoot().string());
            return false;
        }
        LoadProject(GameProject(dir));
        return true;
    }

    // Loads the assets, builds the ECS world and starts running
    bool GameRuntime::Start(Engine::Scene::SceneDescription scene)
    {
        if (!EnsureProject())
        {
            return false;
        }

        // A restart (another scene, or Play from the editor's view) keeps the graphics, and holds the previous scene's
        // files until the new one has taken its own: a file both use is not read or sent to the GPU again
        std::vector<ObjectAssets> held;
        if (running_)
        {
            held = objectAssets_;
            held.insert(held.end(), spawnedAssets_.begin(), spawnedAssets_.end());
            for (const ObjectAssets& assets : held)
            {
                HoldAssets(assets);
            }
            StopScene(true);
        }

        actions_ = Input::InputActions::Load(project_->InputActionsPath());
        const GameSettings& settings = project_->Settings();
        physics_.SetGravity({settings.gravityX, settings.gravityY, settings.gravityZ});
        blockPhysics_.Clear();
        scene_ = std::move(scene);

        // Objects sharing a file share one loaded copy: each Load* call only adds a reference
        EnsureResources();
        objectAssets_.clear();
        objectAssets_.reserve(scene_.objects.size());
        std::transform(scene_.objects.begin(), scene_.objects.end(), std::back_inserter(objectAssets_),
                       [this](const Engine::Scene::GameObject& object) { return LoadAssets(object); });
        // The objects have their own references now: what only the old scene (or a preload) used is let go
        for (const ObjectAssets& assets : held)
        {
            ReleaseAssets(assets);
        }
        for (const ObjectAssets& assets : preloaded_)
        {
            ReleaseAssets(assets);
        }
        preloaded_.clear();
        spawnedEntities_.clear();
        // Before the ECS world is built, so the objects in it can add their bodies. If Jolt cannot start (logged),
        // the scene still plays, without physics
        physics_.Init();
        RebuildEcsScene();
        // The objects' block scripts: "when Play starts" ones begin on the first frame that simulates
        logic_.Clear();
        Logic::Library library;
        Logic::LoadLibraryFile(project_->ProjectDir() / Logic::kLibraryFile, library); // problems are logged
        logic_.SetLibrary(std::move(library));
        for (std::size_t i = 0; i < scene_.objects.size() && i < ecsEntities_.size(); ++i)
        {
            const Engine::Scene::GameObject& object = scene_.objects[i];
            if (object.logic)
            {
                logic_.Set(object.id, object.name, ecsEntities_[i].GetId(), *object.logic);
            }
        }

        StopLuaScripts();
        luaErrorsReported_.clear();
        startupAudioErrors_.clear();

        for (const auto& object : scene_.objects)
        {
            if (object.audioSource && !object.audioSource->clip.empty())
            {
                std::error_code ec;

                if (!std::filesystem::is_regular_file(project_->ProjectDir() / object.audioSource->clip, ec))
                {
                    startupAudioErrors_.emplace_back(object.id, object.audioSource->clip);
                }
            }

            if (object.luaSource.empty() && object.luaFile.empty())
            {
                continue;
            }

            const auto id = object.id;

            Scripting::ScriptSource source{object.luaSource, {}};

            if (!object.luaFile.empty())
            {
                source = Scripting::ReadScriptSource(project_->ProjectDir(), object.luaFile);
            }

            const std::string scriptName =
                object.luaFile.empty() ? object.name + " [" + std::to_string(id) + "].lua" : "@" + object.luaFile;

            luaScripts_[id] = std::make_unique<Scripting::LuaScript>(scriptName, std::move(source.text), LuaHostFor(id),
                                                                     std::move(source.error), &luaCompiled_);
            if (!object.luaFile.empty())
            {
                luaFiles_[id] = object.luaFile;
            }
        }

        NameObjectsForLogic();
        logic_.SetSettings(settings_); // the player's settings outlast the scene that set them
        running_ = true;
        // Only a run in Play mode is a game start (the editor's edit-mode view runs too, frozen)
        if (events_ != nullptr && simulating_)
        {
            playRun_ = true;
            announcedSimulating_ = true;
            events_->Emit(Events::GameStarted{project_->Settings().name, scene_.objects.size()});
        }

        LOG_INFO(ECS, "{} '{}' ({} objects)", simulating_ ? "Playing" : "Editing", project_->Settings().name,
                 scene_.objects.size());

        return true;
    }

    void GameRuntime::Preload(const Engine::Scene::GameObject& object)
    {
        if (!EnsureProject())
        {
            return;
        }
        EnsureResources();
        preloaded_.push_back(LoadAssets(object));
    }

    void GameRuntime::EnsureResources()
    {
        if (resources_.RootDir() != project_->ProjectDir())
        {
            materials_.clear(); // their shaders and textures were the old ResourceManager's
            resources_ = Assets::ResourceManager(project_->ProjectDir());
        }
    }

    void GameRuntime::HoldAssets(const ObjectAssets& assets)
    {
        resources_.AddRef(assets.mesh);
        resources_.AddRef(assets.model);
        resources_.AddRef(assets.texture);
        resources_.AddRef(assets.shader);
        resources_.AddRef(assets.audio);
        for (const Assets::ModelHandle source : assets.animationSources)
        {
            resources_.AddRef(source);
        }
    }

    // Stops sounds, releases every asset and the GPU stuff
    void GameRuntime::Stop()
    {
        // Ends the Play run GameStarted announced, before anything is torn down so handlers can still read the scene
        if (playRun_)
        {
            playRun_ = false;
            if (events_ != nullptr)
            {
                events_->Emit(Events::GameStopped{});
            }
        }

        StopScene(false);
    }

    void GameRuntime::StopScene(bool keepGraphics)
    {
        StopLuaScripts();
        luaErrorsReported_.clear();
        startupAudioErrors_.clear();

        // silence everything first, otherwise sounds keep playing after Stop
        if (ecsScene_ && audio_ != nullptr)
        {
            Engine::Systems::AudioSystem::StopAll(*ecsScene_, *audio_);
        }

        for (const ObjectAssets& assets : objectAssets_)
        {
            ReleaseAssets(assets);
        }
        for (const ObjectAssets& assets : spawnedAssets_)
        {
            ReleaseAssets(assets);
        }
        objectAssets_.clear();
        spawnedAssets_.clear();
        for (const auto& [name, picture] : screenPictures_)
        {
            resources_.Release(picture);
        }
        screenPictures_.clear();
        screenPicturePaths_.clear();
        running_ = false;
        if (!keepGraphics)
        {
            for (const ObjectAssets& assets : preloaded_)
            {
                ReleaseAssets(assets);
            }
            preloaded_.clear();
        }
        if (graphicsInitialized_ && !keepGraphics)
        {
            assetGpuCache_.Shutdown();
            texturedCubeGpu_ = {};
            texturedCube_.rendererCacheId = 0;
            graphics_.Shutdown();
            graphicsInitialized_ = false;
        }
        ecsEntities_.clear();
        spawnedEntities_.clear();
        castRays_.clear();
        lastCastRays_.clear();
        logic_.Clear();
        ecsScene_.reset();
        lastStepMoves_.clear();
        audioClips_.clear();

        // Timers end with the run, and events queued during it are dropped, so neither reaches the next one
        timers_.Clear();
        if (events_ != nullptr)
        {
            events_->ClearQueue();
        }
        // The game's sounds outlast a scene: a button's click goes on playing while "go to scene" starts the next one
        if (!keepGraphics)
        {
            for (const auto& [name, sound] : blockSounds_)
            {
                for (const Audio::SoundId variant : sound.variants)
                {
                    if (audio_ != nullptr)
                    {
                        audio_->UnloadSound(variant);
                    }
                }
            }
            blockSounds_.clear();
        }
        physics_.Shutdown();
        collisions_.clear();
        contactMarks_.clear();
        forceMarks_.clear();
    }

    // The ECS demo button: copies one object into lots of plain entities
    std::size_t GameRuntime::SpawnEntities(std::size_t count, std::uint32_t templateObjectId)
    {
        if (!running_ || !ecsScene_ || count == 0)
        {
            return 0;
        }

        const auto found = std::find_if(scene_.objects.begin(), scene_.objects.end(),
                                        [templateObjectId](const Engine::Scene::GameObject& object)
                                        { return object.id == templateObjectId; });
        const Engine::Scene::GameObject* source = found != scene_.objects.end() ? &*found : nullptr;
        if (!source || !source->transform || !source->renderable)
        {
            throw std::invalid_argument("Select an object with a Transform and a Renderable to copy");
        }
        const Engine::Scene::TransformComponent& sourceTransform = *source->transform;
        const auto& sourceRenderable = *source->renderable;

        // The copies draw with the template's files: one more reference each batch keeps them loaded even if the
        // template is edited or removed while the copies exist
        const ObjectAssets assets = LoadAssets(Engine::Scene::GameObject{.renderable = source->renderable});

        // Where the template is now (in Play mode it may have moved), so the block forms around it
        const Engine::Scene::Entity templateEntity = EntityOf(templateObjectId);
        const Engine::Scene::TransformComponent origin =
            ecsScene_->GetTransform(templateEntity).value_or(sourceTransform);
        const float size = origin.scale * std::max({origin.scaleX, origin.scaleY, origin.scaleZ});

        Engine::Scene::SpawnOptions options;
        options.count = count;
        options.firstIndex = spawnedEntities_.size();
        options.spacing = 1.6f * std::max(size, 0.05f);
        options.origin = origin;
        options.meshRenderer = MeshRendererOf(sourceRenderable, assets);
        options.spinDegreesPerSecond = sourceRenderable.rotationSpeed;

        std::vector<Engine::Scene::Entity> spawned;
        try
        {
            spawned = Engine::Scene::SpawnCopies(*ecsScene_, options);
        }
        catch (...)
        {
            ReleaseAssets(assets);
            throw;
        }
        spawnedAssets_.push_back(assets);
        spawnedEntities_.insert(spawnedEntities_.end(), spawned.begin(), spawned.end());

        LOG_INFO(ECS, "Spawned {} copies of '{}' at runtime ({} live entities)", spawned.size(), source->name,
                 ecsScene_->EntityCount());
        return spawned.size();
    }

    // Removes the spawned copies and gives back their asset references
    std::size_t GameRuntime::ClearSpawnedEntities()
    {
        std::size_t destroyed = 0;

        if (ecsScene_)
        {
            destroyed = Engine::Scene::DestroyAll(*ecsScene_, spawnedEntities_);

            LOG_INFO(ECS, "Destroyed {} spawned entities ({} live)", destroyed, ecsScene_->EntityCount());
        }

        for (const ObjectAssets& assets : spawnedAssets_)
        {
            ReleaseAssets(assets);
        }
        spawnedAssets_.clear();
        spawnedEntities_.clear();
        return destroyed;
    }

    std::size_t GameRuntime::SpawnedEntityCount() const
    {
        return spawnedEntities_.size();
    }

    std::size_t GameRuntime::EntityCount() const
    {
        return ecsScene_ ? ecsScene_->EntityCount() : 0;
    }

    // Makes a brand new ECS world from scene_ (used when a scene starts)
    void GameRuntime::RebuildEcsScene()
    {
        auto newScene = std::make_unique<Engine::Scene::Scene>();

        std::vector<Engine::Scene::Entity> newEntities;
        newEntities.reserve(scene_.objects.size());

        std::vector<std::filesystem::path> newAudioClips;
        newAudioClips.reserve(scene_.objects.size());

        // Each object becomes one entity with the components it has; every entity exists before the children are
        // linked to their parents, which may come later in the scene
        for (std::size_t i = 0; i < scene_.objects.size(); ++i)
        {
            const Engine::Scene::Entity entity = newScene->CreateEntity();
            AddComponents(*newScene, entity, scene_.objects[i], objectAssets_[i], newAudioClips);
            newEntities.push_back(entity);
        }
        LinkParents(*newScene, scene_, newEntities);
        Engine::Systems::HierarchySystem::Update(*newScene);

        if (ecsScene_ && audio_ != nullptr)
        {
            Engine::Systems::AudioSystem::StopAll(*ecsScene_, *audio_);
        }

        // Replace the previous world only after the import succeeds
        ecsScene_ = std::move(newScene);
        lastStepMoves_.clear(); // its entities belonged to the previous world
        ecsEntities_ = std::move(newEntities);
        audioClips_ = std::move(newAudioClips);

        LOG_INFO(ECS, "Built {} entities ({} drawn, {} audio sources)", ecsEntities_.size(),
                 ecsScene_->Count<Engine::Scene::TransformComponent, Engine::Scene::MeshRendererComponent>(),
                 ecsScene_->Count<Engine::Scene::TransformComponent, Engine::Scene::AudioSourceComponent>());
    }

    // Scene object -> ECS components: each of kComponents adds its own. The only place this conversion happens
    void GameRuntime::AddComponents(Engine::Scene::Scene& ecs, Engine::Scene::Entity entity,
                                    const Engine::Scene::GameObject& object, const ObjectAssets& assets,
                                    std::vector<std::filesystem::path>& clips) const
    {
        ForEachComponent([&](const auto& component)
                         { component.Add(ecs, entity, object, assets, project_.get(), clips); });
    }

    // Used for Inspector edits while the game runs, so we don't rebuild everything
    void GameRuntime::ApplyEdit(Engine::Scene::Entity entity, const Engine::Scene::GameObject& before,
                                const Engine::Scene::GameObject& after, const ObjectAssets& assets)
    {
        // Each component that changed is removed (or set, if it can be), then what is left to add is added back from
        // changed by the same code that built the entity
        Engine::Scene::GameObject changed{.id = after.id, .name = after.name};
        const std::function<void(Engine::Scene::Entity)> stopVoice = [this](Engine::Scene::Entity voiceEntity)
        { StopVoice(voiceEntity); };
        const Edit edit{*ecsScene_, entity, before, after, changed, stopVoice};
        ForEachComponent([&edit](const auto& component) { component.Update(edit); });
        AddComponents(*ecsScene_, entity, changed, assets, audioClips_);
    }

    // Parents may be edited, added or removed in any order, so links are made once every entity exists
    void GameRuntime::LinkParents(Engine::Scene::Scene& ecs, const Engine::Scene::SceneDescription& scene,
                                  const std::vector<Engine::Scene::Entity>& entities)
    {
        std::unordered_map<std::uint32_t, Engine::Scene::Entity> entityOf;
        for (std::size_t i = 0; i < scene.objects.size() && i < entities.size(); ++i)
        {
            entityOf[scene.objects[i].id] = entities[i];
        }
        for (std::size_t i = 0; i < scene.objects.size() && i < entities.size(); ++i)
        {
            const Engine::Scene::GameObject& object = scene.objects[i];
            const Engine::Scene::Entity entity = entities[i];
            auto* link = ecs.TryGetComponent<Engine::Scene::HierarchyComponent>(entity);
            const auto parent = entityOf.find(object.parent);
            if (object.parent != 0 && object.transform && parent != entityOf.end())
            {
                if (link)
                {
                    link->parent = parent->second.GetId(); // keeps its live local transform
                }
                else if (!ecs.AddComponent(entity, Engine::Scene::HierarchyComponent{.parent = parent->second.GetId(),
                                                                                     .local = *object.transform}))
                {
                    throw std::runtime_error("Could not add the ECS hierarchy component");
                }
            }
            else if (link)
            {
                // At the top of the scene again: where it was relative to its parent is now where it is in the world
                const Engine::Scene::TransformComponent local = link->local;
                ecs.RemoveComponent<Engine::Scene::HierarchyComponent>(entity);
                ecs.SetTransform(entity, local);
            }
        }
    }

    // Stops one entity's sound (e.g. its Audio Source was removed or its clip changed)
    void GameRuntime::StopVoice(Engine::Scene::Entity entity)
    {
        auto* source = ecsScene_ ? ecsScene_->TryGetComponent<Engine::Scene::AudioSourceComponent>(entity) : nullptr;
        if (source && audio_ != nullptr)
        {
            audio_->Stop(source->voice);
            source->voice = {};
        }
    }

    std::optional<Engine::Scene::GameObject> GameRuntime::LiveObject(std::uint32_t objectId) const
    {
        const Engine::Scene::Entity entity = ecsScene_ ? EntityOf(objectId) : Engine::Scene::Entity{};
        const auto saved =
            std::find_if(scene_.objects.begin(), scene_.objects.end(),
                         [objectId](const Engine::Scene::GameObject& object) { return object.id == objectId; });
        if (!entity || saved == scene_.objects.end())
        {
            return std::nullopt;
        }
        // What the game keeps as saved (name, files, logic, ...) with each component's values as the game has them
        Engine::Scene::GameObject live = *saved;
        ForEachComponent([&](const auto& component) { component.Read(*ecsScene_, entity, live); });
        return live;
    }

    // Linear search is fine here - scenes are a few hundred objects at most (Live calls it once a frame, for the
    // selected object)
    Scripting::LuaHost GameRuntime::LuaHostFor(std::uint32_t id)
    {
        Scripting::LuaHost host;
        host.move = [this, id](double x, double y, double z)
        {
            const auto entity = EntityOf(id);

            if (!entity || !ecsScene_)
            {
                return;
            }

            auto transform = ecsScene_->GetTransform(entity);

            if (!transform)
            {
                return;
            }

            transform->x += static_cast<float>(x);
            transform->y += static_cast<float>(y);
            transform->z += static_cast<float>(z);

            ecsScene_->SetTransform(entity, *transform);
        };
        host.position = [this, id]() -> std::optional<std::array<double, 3>>
        {
            const auto entity = EntityOf(id);
            const auto transform = entity && ecsScene_ ? ecsScene_->GetTransform(entity) : std::nullopt;
            return transform ? std::optional(std::array<double, 3>{transform->x, transform->y, transform->z})
                             : std::nullopt;
        };
        host.playSound = [this, id](const std::string& name)
        {
            const auto entity = EntityOf(id);
            const auto at = entity && ecsScene_ ? ecsScene_->GetTransform(entity) : std::nullopt;
            logic_.RequestSound(name, at ? std::optional(glm::vec3(at->x, at->y, at->z)) : std::nullopt, id);
        };
        host.animate = [this, id](const std::string& clip, bool loop)
        {
            if (const auto entity = EntityOf(id); entity && ecsScene_)
            {
                Engine::Systems::AnimationSystem::Play(*ecsScene_, entity, clip, loop);
            }
        };
        host.send = [this, id](const std::string& message, double value) { EmitFromObject(id, message, value); };
        // A script stopped at a line stops the game with it: Resume carries both on
        host.paused = [this, id](int) { logic_.Pause(id); };
        host.log = [](const std::string& text) { LOG_INFO(Scripting, "{}", text); };
        host.require = [this](const std::string& name)
        { return Scripting::ReadRequiredScript(project_->ProjectDir(), name); };
        return host;
    }

    void GameRuntime::StopLuaScripts()
    {
        for (auto& [id, script] : luaScripts_)
        {
            script->Destroy();
        }
        luaScripts_.clear();
        luaFiles_.clear();
        luaFileTimes_.clear();
        luaFileCheckTicks_ = 0;
    }

    void GameRuntime::ReloadChangedLuaScripts()
    {
        const auto root = project_->ProjectDir();
        for (const auto& [id, file] : luaFiles_)
        {
            const auto script = luaScripts_.find(id);
            if (script == luaScripts_.end())
            {
                continue;
            }
            std::vector<std::filesystem::path> paths{Scripting::ScriptPath(root, file)};
            for (const std::string& name : script->second->Dependencies())
            {
                paths.push_back(root / Scripting::kScriptsFolder / (name + ".lua"));
            }
            bool changed = false;
            for (const std::filesystem::path& path : paths)
            {
                std::error_code ec;
                const auto written = std::filesystem::last_write_time(path, ec);
                if (ec)
                {
                    continue; // being saved, or gone: the script carries on as it is
                }
                const auto [known, added] = luaFileTimes_.try_emplace({id, path.string()}, written);
                changed = changed || (!added && known->second != written);
                known->second = written;
            }
            if (!changed)
            {
                continue;
            }
            Scripting::ScriptSource source = Scripting::ReadScriptSource(root, file);
            std::string error = std::move(source.error);
            if (error.empty() && script->second->Reload(std::move(source.text), error))
            {
                luaErrorsReported_.erase(id);
                LOG_INFO(Scripting, "Reloaded {}", file);
            }
            else
            {
                LOG_ERROR(Scripting, "{} was not reloaded: {}", file, error);
            }
        }
    }

    void GameRuntime::EmitFromObject(std::uint32_t objectId, std::string message, double value,
                                     std::map<std::string, std::string> data)
    {
        if (events_ != nullptr)
        {
            events_->Enqueue(Events::MessageBroadcast{
                .message = std::move(message), .senderObjectId = objectId, .value = value, .data = std::move(data)});
        }
    }

    Engine::Scene::Entity GameRuntime::EntityOf(std::uint32_t objectId) const
    {
        for (std::size_t i = 0; i < scene_.objects.size() && i < ecsEntities_.size(); ++i)
        {
            if (scene_.objects[i].id == objectId)
            {
                return ecsEntities_[i];
            }
        }
        return {};
    }

    std::vector<Engine::Systems::ColliderOutline> GameRuntime::ColliderOutlines()
    {
        if (!ecsScene_)
        {
            return {};
        }
        return Engine::Systems::PhysicsSystem::Outlines(*ecsScene_, physics_.IsInitialized() ? &physics_ : nullptr);
    }

    std::uint32_t GameRuntime::ObjectIdOf(Engine::Scene::Entity entity) const
    {
        for (std::size_t i = 0; i < scene_.objects.size() && i < ecsEntities_.size(); ++i)
        {
            if (entity && ecsEntities_[i] == entity)
            {
                return scene_.objects[i].id;
            }
        }
        return 0;
    }

    std::optional<Engine::Systems::RaycastHit> GameRuntime::Raycast(const glm::vec3& origin, const glm::vec3& direction,
                                                                    float maxDistance,
                                                                    Engine::Scene::Entity ignore) const
    {
        if (!ecsScene_)
        {
            return std::nullopt;
        }
        return Engine::Systems::PhysicsSystem::Raycast(*ecsScene_, physics_, origin, direction, maxDistance, ignore);
    }

    std::optional<Engine::Systems::RaycastHit> GameRuntime::CastRay(const glm::vec3& origin, const glm::vec3& direction,
                                                                    float maxDistance, Engine::Scene::Entity ignore)
    {
        const float directionLength = glm::length(direction);
        if (!ecsScene_ || maxDistance <= 0.0f || directionLength <= 0.0f)
        {
            return std::nullopt;
        }
        const glm::vec3 unit = direction / directionLength;
        std::optional<Engine::Systems::RaycastHit> hit = Raycast(origin, unit, maxDistance, ignore);
        std::string hitName;
        if (hit)
        {
            const Engine::Scene::GameObject* object = Engine::Scene::FindObject(scene_, ObjectIdOf(hit->entity));
            hitName = object ? object->name : "(a spawned copy)";
        }
        RecordRay(
            {origin, unit, maxDistance, hit ? std::optional<glm::vec3>(hit->point) : std::nullopt, std::move(hitName)});
        return hit;
    }

    std::optional<Engine::Physics::Gap> GameRuntime::DistanceBetween(Engine::Scene::Entity a,
                                                                     Engine::Scene::Entity b) const
    {
        if (!ecsScene_)
        {
            return std::nullopt;
        }
        return Engine::Systems::PhysicsSystem::DistanceBetween(*ecsScene_, physics_, a, b);
    }

    std::vector<Engine::Systems::NearbyEntity> GameRuntime::EntitiesWithin(const glm::vec3& point, float radius,
                                                                           Engine::Scene::Entity ignore) const
    {
        if (!ecsScene_)
        {
            return {};
        }
        return Engine::Systems::PhysicsSystem::EntitiesWithin(*ecsScene_, physics_, point, radius, ignore);
    }

    std::optional<glm::vec3> GameRuntime::VelocityOf(Engine::Scene::Entity entity) const
    {
        if (!ecsScene_)
        {
            return std::nullopt;
        }
        return Engine::Systems::PhysicsSystem::VelocityOf(*ecsScene_, physics_, entity);
    }

    std::optional<Engine::Systems::GroundHit> GameRuntime::GroundUnder(Engine::Scene::Entity entity) const
    {
        if (!ecsScene_)
        {
            return std::nullopt;
        }
        return Engine::Systems::PhysicsSystem::GroundUnder(*ecsScene_, physics_, entity);
    }

    void GameRuntime::RecordRay(const Engine::Systems::CastRay& ray)
    {
        if (simulating_)
        {
            Engine::Systems::DebugLineSystem::AddRay(castRays_, ray);
        }
    }

    void GameRuntime::EndRayFrame()
    {
        lastCastRays_ = std::move(castRays_);
        castRays_.clear();
    }

    std::shared_ptr<const Assets::LoadedMaterial> GameRuntime::MaterialOf(const std::string& path)
    {
        if (path.empty() || !project_)
        {
            return nullptr;
        }
        const std::string key = std::filesystem::path(path).lexically_normal().generic_string();
        auto found = materials_.find(key);
        if (found == materials_.end())
        {
            MaterialEntry entry{std::make_shared<Assets::LoadedMaterial>(), {}};
            std::error_code ec;
            entry.written = std::filesystem::last_write_time(project_->ProjectDir() / key, ec);
            ReadMaterial(key, *entry.material);
            found = materials_.emplace(key, std::move(entry)).first;
        }
        return found->second.material;
    }

    void GameRuntime::ReadMaterial(const std::string& path, Assets::LoadedMaterial& material)
    {
        std::string error;
        auto file = Assets::ReadMaterialFile(project_->ProjectDir() / path, &error);
        if (!file)
        {
            LOG_WARN(Resource, "Material {}: {} - drawn plain", path, error);
            file.emplace();
        }
        // The new shader and texture first, so one the material keeps is not unloaded and read again
        const Assets::ShaderHandle shader =
            file->shader.empty() ? Assets::ShaderHandle{} : resources_.LoadShader(file->shader);
        const Assets::TextureHandle texture =
            file->texture.empty() ? Assets::TextureHandle{} : resources_.LoadTexture(file->texture);
        resources_.Release(material.shader);
        resources_.Release(material.texture);
        material = Assets::LoadedMaterial{std::move(*file), shader, texture};
    }

    void GameRuntime::ReloadChangedMaterials()
    {
        if (!project_)
        {
            return;
        }
        for (auto& [path, entry] : materials_)
        {
            std::error_code ec;
            const auto written = std::filesystem::last_write_time(project_->ProjectDir() / path, ec);
            if (!ec && written != entry.written)
            {
                entry.written = written;
                ReadMaterial(path, *entry.material);
                LOG_INFO(Resource, "Material {} changed: read again", path);
            }
        }
    }

    // Loads (or adds a reference to) every file an object uses
    GameRuntime::ObjectAssets GameRuntime::LoadAssets(const Engine::Scene::GameObject& object)
    {
        ObjectAssets assets;
        if (const auto& renderable = object.renderable)
        {
            if (!renderable->mesh.empty())
            {
                assets.mesh = resources_.LoadMesh(renderable->mesh);
            }
            if (!renderable->model.empty())
            {
                assets.model = resources_.LoadModel(renderable->model);
            }
            if (!renderable->texture.empty())
            {
                assets.texture = resources_.LoadTexture(renderable->texture);
            }
            if (!renderable->material.empty())
            {
                assets.material = MaterialOf(renderable->material);
            }
            // What it draws, measured once for colliders that fit it: the model, which is drawn in place of the mesh
            if (const Assets::Model* model = assets.model.IsValid() ? resources_.Get(assets.model) : nullptr)
            {
                assets.bounds = Assets::RestBounds(*model);
            }
            else if (const Assets::Mesh* mesh = assets.mesh.IsValid() ? resources_.Get(assets.mesh) : nullptr)
            {
                assets.bounds = Assets::RestBounds(Assets::ModelData{.meshes = {*mesh}});
            }
            if (!renderable->shader.empty())
            {
                assets.shader = resources_.LoadShader(renderable->shader);
            }
        }
        if (const auto& animator = object.animator)
        {
            for (const std::string& source : animator->sources)
            {
                if (!source.empty())
                {
                    assets.animationSources.push_back(resources_.LoadModel(source));
                }
            }
        }
        // The ResourceManager copy exists so the Assets panel can list the clip; playback goes through FMOD, which
        // reads far more formats than LoadAudioFile does. Asking it for an mp3 only produces a failed load and a
        // misleading error in the log while the sound plays perfectly well.
        if (const auto& audio = object.audioSource;
            audio && !audio->clip.empty() && HasExtension(audio->clip, kDecodableAudioExtensions))
        {
            assets.audio = resources_.LoadAudio(audio->clip);
        }
        return assets;
    }

    // The opposite of LoadAssets
    void GameRuntime::ReleaseAssets(const ObjectAssets& assets)
    {
        resources_.Release(assets.mesh);
        resources_.Release(assets.model);
        resources_.Release(assets.texture);
        resources_.Release(assets.shader);
        resources_.Release(assets.audio);
        for (const Assets::ModelHandle source : assets.animationSources)
        {
            resources_.Release(source);
        }
    }

    // Lets the Inspector show whether an object's sound is playing
    Audio::VoiceId GameRuntime::EmitterVoice(std::uint32_t objectId) const
    {
        const Engine::Scene::Entity entity = EntityOf(objectId);
        if (!entity || !ecsScene_)
        {
            return {};
        }
        const auto* source = ecsScene_->TryGetComponent<Engine::Scene::AudioSourceComponent>(entity);
        return source ? source->voice : Audio::VoiceId{};
    }

    // Lets the Inspector and the Viewport show which level of detail an object is drawn with
    std::optional<GameRuntime::LodView> GameRuntime::LodOf(std::uint32_t objectId)
    {
        const Engine::Scene::Entity entity = EntityOf(objectId);
        if (!entity || !ecsScene_)
        {
            return std::nullopt;
        }
        const auto* renderer = ecsScene_->TryGetComponent<Engine::Scene::MeshRendererComponent>(entity);
        const Assets::Model* model = renderer && renderer->model.IsValid() ? resources_.Get(renderer->model) : nullptr;
        if (!model)
        {
            return std::nullopt;
        }
        const int level = std::min(renderer->lodLevel, static_cast<int>(model->lods.size()));
        return LodView{level, 1 + static_cast<int>(model->lods.size()), Assets::TriangleCount(*model, level),
                       renderer->lodScreenSize};
    }

    // The Viewport's stats line: how many models were drawn at each level, and the triangles they cost
    GameRuntime::LodStats GameRuntime::LodStatistics()
    {
        LodStats stats;
        if (!ecsScene_)
        {
            return stats;
        }
        ecsScene_->ForEach<const Engine::Scene::MeshRendererComponent>(
            [this, &stats](Engine::Scene::Entity, const Engine::Scene::MeshRendererComponent& renderer)
            {
                const Assets::Model* model =
                    renderer.visible && renderer.model.IsValid() ? resources_.Get(renderer.model) : nullptr;
                if (!model)
                {
                    return;
                }
                const int level = std::clamp(renderer.lodLevel, 0, static_cast<int>(model->lods.size()));
                ++stats.objects[static_cast<std::size_t>(level)];
                stats.triangles += Assets::TriangleCount(*model, level);
                stats.fullTriangles += Assets::TriangleCount(*model, 0);
            });
        return stats;
    }

    // Compares the new scene with the current one by id and only touches what changed
    void GameRuntime::UpdateScene(Engine::Scene::SceneDescription scene)
    {
        if (!running_ || !ecsScene_)
        {
            return;
        }

        std::vector<ObjectAssets> newAssets;
        std::vector<Engine::Scene::Entity> newEntities;
        newAssets.reserve(scene.objects.size());
        newEntities.reserve(scene.objects.size());
        std::vector<bool> kept(scene_.objects.size(), false);

        for (const Engine::Scene::GameObject& after : scene.objects)
        {
            const auto found =
                std::find_if(scene_.objects.begin(), scene_.objects.end(),
                             [&after](const Engine::Scene::GameObject& object) { return object.id == after.id; });
            if (found == scene_.objects.end())
            {
                // A new object
                const ObjectAssets assets = LoadAssets(after);
                const Engine::Scene::Entity entity = ecsScene_->CreateEntity();
                AddComponents(*ecsScene_, entity, after, assets, audioClips_);
                if (after.logic)
                {
                    logic_.Set(after.id, after.name, entity.GetId(), *after.logic);
                }
                newAssets.push_back(assets);
                newEntities.push_back(entity);
                continue;
            }

            const auto i = static_cast<std::size_t>(found - scene_.objects.begin());
            kept[i] = true;
            ObjectAssets assets = objectAssets_[i];
            if (*found != after)
            {
                // Edited blocks start again from the top (the object keeps where it got to); moving scripts about on
                // the canvas (Clean up) is not an edit
                if (!SameBlocks(found->logic, after.logic) || found->name != after.name)
                {
                    if (after.logic)
                    {
                        logic_.Set(after.id, after.name, ecsEntities_[i].GetId(), *after.logic);
                    }
                    else
                    {
                        logic_.Remove(after.id);
                    }
                }
                const auto sourcesOf = [](const Engine::Scene::GameObject& object)
                { return object.animator ? object.animator->sources : std::vector<std::string>{}; };
                if (found->renderable != after.renderable || found->audioSource != after.audioSource ||
                    sourcesOf(*found) != sourcesOf(after))
                {
                    // Load the new files before releasing the old ones, so files both use stay loaded
                    const ObjectAssets fresh = LoadAssets(after);
                    ReleaseAssets(assets);
                    assets = fresh;
                }
                ApplyEdit(ecsEntities_[i], *found, after, assets);
            }
            newAssets.push_back(assets);
            newEntities.push_back(ecsEntities_[i]);
        }

        // Objects that are gone
        for (std::size_t i = 0; i < kept.size(); ++i)
        {
            if (!kept[i])
            {
                logic_.Remove(scene_.objects[i].id);
                StopVoice(ecsEntities_[i]);
                Engine::Systems::PhysicsSystem::RemoveBody(*ecsScene_, physics_, ecsEntities_[i]);
                ecsScene_->DestroyEntity(ecsEntities_[i]);
                ReleaseAssets(objectAssets_[i]);
            }
        }

        objectAssets_ = std::move(newAssets);
        ecsEntities_ = std::move(newEntities);
        scene_ = std::move(scene);
        LinkParents(*ecsScene_, scene_, ecsEntities_);
        NameObjectsForLogic();
    }

    void GameRuntime::SetLibrary(Logic::Library library)
    {
        logic_.SetLibrary(std::move(library));
        for (std::size_t i = 0; i < scene_.objects.size() && i < ecsEntities_.size(); ++i)
        {
            const Engine::Scene::GameObject& object = scene_.objects[i];
            if (object.logic)
            {
                logic_.Set(object.id, object.name, ecsEntities_[i].GetId(), *object.logic);
            }
        }
    }

    void GameRuntime::NameObjectsForLogic()
    {
        std::vector<Logic::LogicRunner::NamedEntity> named;
        named.reserve(scene_.objects.size());
        for (std::size_t i = 0; i < scene_.objects.size() && i < ecsEntities_.size(); ++i)
        {
            named.push_back({.name = scene_.objects[i].name, .entity = ecsEntities_[i].GetId()});
        }
        logic_.SetObjects(std::move(named));
    }

    void GameRuntime::SetViewport(int x, int y, int width, int height)
    {
        viewport_ = Viewport{x, y, width, height};
    }

    void GameRuntime::UseWholeWindow()
    {
        viewport_.reset();
    }

    void GameRuntime::SetUseGameCamera(bool use)
    {
        useGameCamera_ = use;
    }

    void GameRuntime::SetShowGrid(bool show)
    {
        showGrid_ = show;
    }

    void GameRuntime::SetWireframe(bool wireframe)
    {
        wireframe_ = wireframe;
    }

    void GameRuntime::SetEnvironmentEnabled(bool enabled)
    {
        environmentEnabled_ = enabled;
    }

    bool GameRuntime::IsRunning() const
    {
        return running_;
    }

    const Engine::Scene::SceneDescription& GameRuntime::Scene() const
    {
        return scene_;
    }

    const Assets::ResourceManager& GameRuntime::Resources() const
    {
        return resources_;
    }

    Graphics::Camera3D* GameRuntime::Camera()
    {
        return graphicsInitialized_ ? &graphics_.GetRenderer().GetRenderer3D().Camera() : nullptr;
    }

    const Input::InputActions& GameRuntime::Actions() const
    {
        return actions_;
    }

    void GameRuntime::SetEventBus(Core::EventBus* events)
    {
        events_ = events;
        logic_.SetEventBus(events);
        messagesToBlocks_ = {};
        if (events != nullptr)
        {
            // The blocks' own broadcasts reach their receivers already; this passes on the ones engine code sends
            messagesToBlocks_ = events->Subscribe<Events::MessageBroadcast>(
                [this](const Events::MessageBroadcast& e)
                {
                    if (!e.fromBlocks)
                    {
                        logic_.Receive(e.message, e.value);
                    }
                });
        }

        // Timers started on the bus are counted down here; touches and timers that run out start the blocks' hats
        timers_.Connect(events);
        touchesToBlocks_ = {};
        timersToBlocks_ = {};
        if (events != nullptr)
        {
            touchesToBlocks_ = events->Subscribe<Events::CollisionBegan>(
                [this](const Events::CollisionBegan& e)
                {
                    const auto nameOf = [this](std::uint32_t id) -> std::string
                    {
                        const auto found =
                            std::find_if(scene_.objects.begin(), scene_.objects.end(),
                                         [id](const Engine::Scene::GameObject& object) { return object.id == id; });
                        return found != scene_.objects.end() ? found->name : std::string();
                    };
                    if (e.objectA != 0)
                    {
                        logic_.Touched(e.objectA, nameOf(e.objectB));
                    }
                    if (e.objectB != 0)
                    {
                        logic_.Touched(e.objectB, nameOf(e.objectA));
                    }
                });
            timersToBlocks_ = events->Subscribe<Events::TimerElapsed>([this](const Events::TimerElapsed& e)
                                                                      { logic_.TimerEnded(e.name); });
        }
    }

    void GameRuntime::SetSimulating(bool simulating)
    {
        if (simulating_ == simulating)
        {
            return;
        }
        simulating_ = simulating;
    }

    bool GameRuntime::IsSimulating() const
    {
        return simulating_;
    }

    bool GameRuntime::HasPlayer() const
    {
        return ecsScene_ &&
               ecsScene_->Count<Engine::Scene::TransformComponent, Engine::Scene::PlayerControllerComponent>() > 0;
    }

    // One frame: update the systems (in Play mode), place the camera, audio, lights, then draw
    void GameRuntime::Tick(double deltaSeconds, const Platform::Window& window, const Input::GameInput* input,
                           Audio::AudioSystem* audio)
    {
        // "go to scene" in the last frame: that scene starts now, before anything of this frame is drawn. A name is
        // a scene of data/scenes ("level2"); a path ("data/scenes/level2.json") is used as it is.
        if (pendingScene_ && project_)
        {
            std::string scene = std::move(*pendingScene_);
            pendingScene_.reset();
            if (scene.find('/') == std::string::npos && scene.find('\\') == std::string::npos)
            {
                scene = "data/scenes/" + scene;
            }
            if (std::filesystem::path(scene).extension() != ".json")
            {
                scene += ".json";
            }
            Engine::Scene::SceneDescription next;
            const auto report = project_->LoadSceneChecked(scene, next);
            if (report.ok)
            {
                Start(std::move(next));
                switchedScene_ = scene;
                LOG_INFO(Game, "Went to the scene {}", scene);
            }
            else
            {
                LOG_WARN(Game, "\"go to scene\": there is no scene {} ({})", scene, report.error);
            }
        }

        // GL setup is done on the first frame, not in Start(), because the OpenGL context has to exist first
        if (!graphicsInitialized_)
        {
            graphics_.Init();
            assetGpuCache_.Init();
            texturedCubeGpu_ = Graphics::Mesh::FromAssetData(texturedCube_, "PlaceholderCube");
            graphics_.SetClearColor(0.08f, 0.09f, 0.11f, 1.0f);
            graphicsInitialized_ = true;
        }

        if (viewport_)
        {
            graphics_.BeginFrame(viewport_->x, viewport_->y, viewport_->width, viewport_->height);
        }
        else
        {
            graphics_.BeginFrame(window.Width(), window.Height());
        }
        graphics_.Clear();

        if (!running_)
        {
            graphics_.EndFrame();
            return;
        }

        resources_.Update(deltaSeconds); // notices renamed/deleted asset files, and saved shaders and textures
        secondsSinceMaterialCheck_ += deltaSeconds;
        if (secondsSinceMaterialCheck_ >= 1.0)
        {
            secondsSinceMaterialCheck_ = 0.0;
            ReloadChangedMaterials();
        }

        auto& renderer3D = graphics_.GetRenderer().GetRenderer3D();
        audio_ = audio;
        collisions_.clear(); // a frame that does not simulate has none
        // Pause / Resume of a Play run. Sent here rather than in SetSimulating: the editor's Stop turns simulating off
        // too, just before it ends the run, and that is not a pause
        if (playRun_ && events_ != nullptr && simulating_ != announcedSimulating_)
        {
            announcedSimulating_ = simulating_;
            events_->Emit(Events::SimulationChanged{simulating_});
        }
        bool worldAdvanced = false; // the world moved this frame, so the rays cast in it become the last frame's
        // Systems that change the world only run in Play mode (edit mode and Pause just draw)
        if (simulating_)
        {
            Logic::LogicRunner::Keys keys;
            if (input)
            {
                keys.down = [input](int code) { return input->IsKeyDown(code); };
                keys.pressed = [input](int code) { return input->IsKeyPressed(code); };
                keys.released = [input](int code) { return input->IsKeyReleased(code); };
                keys.mouseReleased = [input](int code) { return input->IsMouseButtonReleased(code); };
                keys.mouseButton = [input](int button) { return input->IsMouseButtonDown(button); };
                keys.mousePressed = [input](int button) { return input->IsMouseButtonPressed(button); };
            }
            // Motion blocks walk Dynamic bodies through the physics, and Physics blocks jump and push them
            blockPhysics_.Bind(*ecsScene_, physics_);
            blockPhysics_.SetRayCaster(
                [this](const glm::vec3& origin, const glm::vec3& direction, float length, Engine::Scene::Entity ignore)
                { return CastRay(origin, direction, length, ignore); });
            blockPhysics_.SetRayRecorder([this](const Engine::Systems::CastRay& ray) { RecordRay(ray); });
            logic_.SetPhysics(&blockPhysics_);
            if (input)
            {
                Engine::Systems::PlayerControllerSystem::Update(*ecsScene_, *input, deltaSeconds);
            }
            // ActionPressed / ActionReleased for each action that went down / up this frame (input_actions.json)
            if (input != nullptr && events_ != nullptr)
            {
                for (const auto& entry : actions_.All())
                {
                    if (input->IsActionPressed(entry.first))
                    {
                        events_->Emit(Events::ActionPressed{entry.first});
                    }
                    if (input->IsActionReleased(entry.first))
                    {
                        events_->Emit(Events::ActionReleased{entry.first});
                    }
                }
                // KeyPressed for every key that went down, bound or not
                for (const Input::InputBinding& binding : Input::AllBindings())
                {
                    if (binding.device == Input::InputBinding::Device::Key && input->IsKeyPressed(binding.code))
                    {
                        events_->Emit(Events::KeyPressed{binding.code, Input::BindingName(binding)});
                    }
                }
            }
            // Once per frame while playing, after the input events
            if (events_ != nullptr)
            {
                events_->Emit(Events::GameUpdate{deltaSeconds});
            }

            // Block scripts and the physics go in fixed steps, one round of a "forever" to each, so "move 0.35 forward"
            // goes at the same speed at any frame rate. Before the systems below (children following parents, the
            // camera, the audio listener), which read where they put things this frame.
            constexpr double kStep = Physics::PhysicsWorld::FixedStep;
            // The player's shots go where its view looks
            blockPhysics_.SetAim(renderer3D.Camera().GetPosition(), renderer3D.Camera().GetForward());
            // A debugger frame step must be one of these steps: the same length
            static_assert(Logic::LogicRunner::kFrameSeconds == Physics::PhysicsWorld::FixedStep);
            logic_.NoteKeyPresses(keys);
            keys.pressed = nullptr; // from the notes above: a frame with two steps must not start a script twice
            keys.released = nullptr;
            keys.mouseReleased = nullptr;
            keys.mousePressed = nullptr;
            const int steps =
                Core::TakeFixedSteps(secondsToSimulate_, deltaSeconds, kStep, Physics::PhysicsWorld::MaxStepsPerUpdate);

            // The player moves before the systems below, which read where it put things. Not while the debugger holds
            // the world still; a frame step moves it one step, like everything else in it
            if (input)
            {
                if (const std::optional<double> playerSeconds = logic_.PlayerSeconds(steps, deltaSeconds))
                {
                    Engine::Systems::PlayerControllerSystem::Update(*ecsScene_, *input, *playerSeconds);
                }
            }

            bool advanced = false;     // the world moved in at least one step
            bool frameStepped = false; // one of them was a frame step
            std::vector<std::pair<Engine::Scene::Entity, Engine::Scene::TransformComponent>> beforeLastStep;
            for (int step = 0; step < steps; ++step)
            {
                if (step == steps - 1)
                {
                    beforeLastStep = TopLevelTransforms(*ecsScene_);
                }
                logic_.Update(*ecsScene_, kStep, keys);
                if (auto scene = logic_.TakeSceneRequest())
                {
                    pendingScene_ = std::move(scene);
                    break;
                }

                // A breakpoint, a block step or the Pause button stops the whole world, not just the scripts; a frame
                // step lets one step of it pass
                if (!logic_.IsPaused() || logic_.FrameStepped())
                {
                    for (const auto& [id, clip] : startupAudioErrors_)
                    {
                        Core::Logger::Get().Write(Core::LogLevel::Error, "Audio", "Audio clip not found: " + clip,
                                                  __FILE__, __LINE__, Core::LogSource::Engine,
                                                  {id, 0, logic_.DiagnosticSession()});
                    }

                    startupAudioErrors_.clear();

                    if (++luaFileCheckTicks_ >= kLuaFileCheckTicks)
                    {
                        luaFileCheckTicks_ = 0;
                        ReloadChangedLuaScripts();
                    }

                    // Lua advances only when the world advances.
                    for (auto& [id, script] : luaScripts_)
                    {
                        if (EntityOf(id))
                        {
                            script->Update(kStep);
                            if (!script->Error().empty() && luaErrorsReported_.insert(id).second)
                            {
                                Core::Logger::Get().Write(Core::LogLevel::Error, "Scripting", script->Error(), __FILE__,
                                                          __LINE__, Core::LogSource::Engine,
                                                          {id, 0, logic_.DiagnosticSession(), true});
                            }
                        }
                    }

                    advanced = true;
                    frameStepped = frameStepped || logic_.FrameStepped();
                    worldAdvanced = true;
                    blockPhysics_.BeforeStep(kStep);
                    const auto touched =
                        Engine::Systems::PhysicsSystem::Update(*ecsScene_, physics_, kStep, &resources_);
                    // Pushes are shown a moment: an impulse only lasts one step
                    if (auto pushes = physics_.TakePushes(); !pushes.empty() || ++forceMarkTicks_ > kForceMarkTicks)
                    {
                        forceMarks_.clear();
                        for (const Engine::Physics::Push& push : pushes)
                        {
                            forceMarks_.push_back({push.point, push.amount});
                        }
                        forceMarkTicks_ = 0;
                    }
                    collisions_.insert(collisions_.end(), touched.begin(), touched.end());
                    for (const auto& contact : touched)
                    {
                        if (contact.began)
                        {
                            logic_.NoteCollision(contact.a.GetId(), contact.b.GetId());
                            if (contactMarks_.size() >= kMaxContactMarks)
                            {
                                contactMarks_.erase(contactMarks_.begin());
                            }
                            contactMarks_.push_back({contact.point, contact.normal});
                        }
                    }
                    blockPhysics_.AfterStep();
                }
                else
                {
                    // No time passes, but a block step may have moved an object: its body goes with it
                    Engine::Systems::PhysicsSystem::SyncMoved(*ecsScene_, physics_);
                }
            }
            // CollisionBegan / CollisionEnded for what the physics reported in the steps above, by scene object id.
            // After the steps, so a handler that changes the scene does not change it under the physics.
            if (events_ != nullptr)
            {
                for (const Engine::Systems::Collision& collision : collisions_)
                {
                    const std::uint32_t a = ObjectIdOf(collision.a);
                    const std::uint32_t b = ObjectIdOf(collision.b);
                    if (collision.began)
                    {
                        events_->Emit(
                            Events::CollisionBegan{a, b, collision.impactSpeed, collision.point, collision.normal});
                    }
                    else
                    {
                        events_->Emit(Events::CollisionEnded{a, b});
                    }
                }
            }
            // Countdown timers, only while simulating (Pause stops them)
            timers_.Update(deltaSeconds);
            if (steps > 0)
            {
                lastStepMoves_ = StepMoves(*ecsScene_, beforeLastStep);
            }
            PlayBlockSounds();
            ApplySettings();

            if (!logic_.IsPaused() || advanced)
            {
                const double worldSeconds = frameStepped ? Logic::LogicRunner::kFrameSeconds : deltaSeconds;
                Engine::Systems::SpinSystem::Update(*ecsScene_, worldSeconds);
                Engine::Systems::AnimationSystem::Update(*ecsScene_, resources_, worldSeconds);
                playSeconds_ += worldSeconds;
            }
        }
        else
        {
            lastStepMoves_.clear(); // paused or editing: everything is drawn where it is
        }

        // Between steps, what the last step moved is drawn part of the way back, by the share of a step not simulated
        // yet; the real transforms are put back after drawing, so the blocks and the physics never see this
        std::vector<std::pair<Engine::Scene::Entity, Engine::Scene::TransformComponent>> realTransforms;
        const float behind =
            1.0f - static_cast<float>(std::clamp(secondsToSimulate_ / Physics::PhysicsWorld::FixedStep, 0.0, 1.0));
        for (const auto& [entity, move] : lastStepMoves_)
        {
            auto transform = ecsScene_->GetTransform(entity);
            if (!transform)
            {
                continue; // destroyed since
            }
            realTransforms.emplace_back(entity, *transform);
            transform->x -= move.x * behind;
            transform->y -= move.y * behind;
            transform->z -= move.z * behind;
            transform->rotation -= move.rotation * behind;
            transform->rotationX -= move.rotationX * behind;
            transform->rotationZ -= move.rotationZ * behind;
            ecsScene_->SetTransform(entity, *transform);
        }

        // Children follow their parents (in edit mode too, so an edited parent takes its children with it)
        Engine::Systems::HierarchySystem::Update(*ecsScene_);

        if (useGameCamera_)
        {
            // The Main camera, kept behind a following player; with no Main camera, the view behind the player;
            // with neither, the camera stays where it is
            Engine::Systems::PlayerControllerSystem::MoveMainCameraToPlayer(*ecsScene_, &physics_);
            if (!Engine::Systems::RenderSystem::ApplyPrimaryCamera(*ecsScene_, renderer3D.Camera()))
            {
                Engine::Systems::PlayerControllerSystem::FollowWithCamera(*ecsScene_, renderer3D.Camera(), &physics_);
            }
        }

        if (audio != nullptr)
        {
            Engine::Systems::AudioSystem::UpdateListener(*audio, renderer3D.Camera());
            Engine::Systems::AudioSystem::Update(*ecsScene_, *audio, audioClips_, simulating_ && !logic_.IsPaused());
        }

        auto lighting = Renderer::Renderer3D::DefaultLighting();
        Engine::Systems::RenderSystem::CollectLights(*ecsScene_, lighting, simulating_ ? playSeconds_ : 0.0);
        ApplySkyLight(lighting);
        renderer3D.SetLighting(lighting);

        // After the blocks, which may have changed the sky this frame
        Renderer::Environment environment = renderer3D.GetEnvironment();
        environment.enabled = environmentEnabled_;
        ApplySky(environment);
        renderer3D.SetEnvironment(environment);
        renderer3D.DrawSky();

        // Only the objects: the sky above and the grid below stay filled
        Graphics::RenderAPI::SetWireframe(wireframe_);
        Engine::Systems::RenderSystem::Draw(*ecsScene_, renderer3D, resources_, assetGpuCache_, texturedCubeGpu_);
        Graphics::RenderAPI::SetWireframe(false);

        if (showGrid_)
        {
            renderer3D.DrawGrid();
        }

        for (const auto& [entity, transform] : realTransforms)
        {
            ecsScene_->SetTransform(entity, transform);
        }
        if (!realTransforms.empty())
        {
            Engine::Systems::HierarchySystem::Update(*ecsScene_); // the children back where their parents really are
        }

        // The debugger's lines, over everything (in edit mode too)
        if (debugLines_.Any())
        {
            debugLineBatch_.Clear();
            const std::vector<Engine::Systems::ColliderOutline> colliders =
                debugLines_.colliders || debugLines_.bounds ? ColliderOutlines()
                                                            : std::vector<Engine::Systems::ColliderOutline>{};
            // While paused the last frame's rays show too: a breakpoint stops before its block casts anything. So they
            // do in a frame too short for a step of the world, or the rays would flicker on a fast screen
            std::vector<Engine::Systems::CastRay> rays = castRays_;
            if (debugLines_.rays && (logic_.IsPaused() || !worldAdvanced))
            {
                rays.insert(rays.end(), lastCastRays_.begin(), lastCastRays_.end());
            }
            Engine::Systems::DebugLineSystem::Collect(*ecsScene_, resources_, debugLines_, colliders, rays,
                                                      debugLineBatch_, contactMarks_, forceMarks_);
            debugLineBatch_.Draw(renderer3D.StandardShader(), renderer3D.Camera().GetViewProjectionMatrix());
        }
        if (worldAdvanced)
        {
            EndRayFrame();
        }

        graphics_.EndFrame();
    }

    // The scene's sky sets how much light comes from all around, which is what keeps a night dark
    void GameRuntime::ApplySkyLight(Graphics::LightingContext& lighting) const
    {
        for (const Engine::Scene::Entity entity : ecsEntities_)
        {
            if (const auto* sky = ecsScene_->TryGetComponent<Engine::Scene::SkyComponent>(entity))
            {
                lighting.ambientStrength = sky->ambient;
                return;
            }
        }
    }

    void GameRuntime::ApplySky(Renderer::Environment& environment)
    {
        Engine::Scene::SkyComponent sky; // the default sky
        for (const Engine::Scene::Entity entity : ecsEntities_)
        {
            if (const auto* found = ecsScene_->TryGetComponent<Engine::Scene::SkyComponent>(entity))
            {
                sky = *found;
                break;
            }
        }
        environment.zenithColor = {sky.topR, sky.topG, sky.topB};
        environment.horizonColor = {sky.horizonR, sky.horizonG, sky.horizonB};
        environment.groundColor = {sky.groundR, sky.groundG, sky.groundB};
        environment.fogDensity = std::max(sky.haze, 0.0f);
        environment.skyTurn = sky.turn;
        environment.skyPicture = nullptr;
        if (!sky.picture.empty() && ScreenPicture(sky.picture) != nullptr)
        {
            environment.skyPicture = assetGpuCache_.GetOrUploadTexture(screenPictures_.at(sky.picture), resources_);
        }
    }

    // A picture by the name the blocks give: a path to the file, or the name of one of the game's pictures (the first
    // whose file name, without its extension, is that name, any case)
    const Assets::Texture* GameRuntime::ScreenPicture(const std::string& name, std::string* path)
    {
        if (!running_ || name.empty() || !project_)
        {
            return nullptr;
        }
        auto found = screenPictures_.find(name);
        if (found == screenPictures_.end())
        {
            std::string file = name;
            if (std::filesystem::path(name).extension().empty())
            {
                const auto lower = [](std::string text)
                {
                    std::transform(text.begin(), text.end(), text.begin(),
                                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    return text;
                };
                file.clear();
                for (const std::string& each : project_->FindAssets("assets", kTextureExtensions))
                {
                    if (lower(std::filesystem::path(each).stem().string()) == lower(name))
                    {
                        file = each;
                        break;
                    }
                }
            }
            found = screenPictures_.emplace(name, file.empty() ? Assets::TextureHandle{} : resources_.LoadTexture(file))
                        .first;
            screenPicturePaths_[name] = file;
        }
        if (path)
        {
            *path = screenPicturePaths_[name];
        }
        const Assets::Texture* texture = found->second.IsValid() ? resources_.Get(found->second) : nullptr;
        return texture && texture->width > 0 && texture->height > 0 ? texture : nullptr;
    }

    // A sound by its path, or by its name like the screen's pictures ("sword_swing" is the game's sword_swing.wav, or
    // .ogg ...). A name with no sound of its own stands for its numbered variants: "step_stone" is step_stone_1,
    // step_stone_2, ... in that order.
    std::vector<std::string> GameRuntime::SoundFilesNamed(const std::string& name) const
    {
        if (!std::filesystem::path(name).extension().empty())
        {
            return {name};
        }
        const std::string wanted = ToLower(name);
        std::vector<std::pair<int, std::string>> variants;
        for (const std::string& each : project_->FindAssets("assets", kAudioExtensions))
        {
            const std::string stem = ToLower(std::filesystem::path(each).stem().string());
            if (stem == wanted)
            {
                return {each};
            }
            const std::string number = stem.size() > wanted.size() + 1 && stem.starts_with(wanted + "_")
                                           ? stem.substr(wanted.size() + 1)
                                           : std::string();
            if (!number.empty() &&
                std::all_of(number.begin(), number.end(), [](unsigned char c) { return std::isdigit(c); }))
            {
                variants.emplace_back(std::stoi(number), each);
            }
        }
        std::sort(variants.begin(), variants.end());
        std::vector<std::string> files;
        for (const auto& [number, file] : variants)
        {
            files.push_back(file);
        }
        return files;
    }

    // The settings the Game blocks changed: kept for the next scenes, and the ones the game itself does applied now
    // (the window's are handed to whoever has the window, TakeGameRequests)
    void GameRuntime::ApplySettings()
    {
        settings_ = logic_.Settings();
        const auto setting = [this](const char* name) -> std::optional<double>
        {
            const auto found = settings_.find(name);
            return found != settings_.end() ? std::optional(found->second) : std::nullopt;
        };
        if (const auto sensitivity = setting("sensitivity"))
        {
            ecsScene_->ForEach<Engine::Scene::PlayerControllerComponent>(
                [&](Engine::Scene::Entity, Engine::Scene::PlayerControllerComponent& player)
                { player.lookSensitivity = static_cast<float>(*sensitivity); });
        }
        if (audio_ != nullptr)
        {
            if (const auto master = setting("master volume"))
            {
                audio_->SetMasterVolume(static_cast<float>(*master / 100.0));
            }
            if (const auto music = setting("music volume"))
            {
                audio_->SetBusVolume(Audio::Bus::Music, static_cast<float>(*music / 100.0));
            }
            if (const auto sfx = setting("sfx volume"))
            {
                audio_->SetBusVolume(Audio::Bus::Sfx, static_cast<float>(*sfx / 100.0));
            }
        }
        for (Logic::LogicRunner::GameRequest& request : logic_.TakeGameRequests())
        {
            gameRequests_.push_back(request);
        }
    }

    std::vector<Logic::LogicRunner::GameRequest> GameRuntime::TakeGameRequests()
    {
        return std::exchange(gameRequests_, {});
    }

    void GameRuntime::PlayBlockSounds()
    {
        const auto requests = logic_.TakeSoundRequests();
        if (requests.empty() || !project_)
        {
            return;
        }
        for (const auto& request : requests)
        {
            const auto files = SoundFilesNamed(request.name);

            const bool exists =
                std::any_of(files.begin(), files.end(),
                            [this](const auto& file)
                            {
                                std::error_code ec;

                                return std::filesystem::is_regular_file(project_->ProjectDir() / file, ec);
                            });

            if (!exists)
            {
                logic_.ReportProblem(request.object, request.block, "Audio clip not found: " + request.name);

                continue;
            }

            if (audio_ == nullptr || !audio_->IsAvailable())
            {
                continue;
            }

            auto found = blockSounds_.find(request.name);
            if (found == blockSounds_.end())
            {
                BlockSound sound;
                for (const std::string& file : files)
                {
                    const Audio::SoundId loaded =
                        audio_->LoadSound((project_->ProjectDir() / file).string(),
                                          Audio::SoundLoadOptions{.spatial = true,
                                                                  .minDistance = kBlockSoundFullVolumeDistance,
                                                                  .maxDistance = kBlockSoundSilentDistance});
                    if (loaded.IsValid())
                    {
                        sound.variants.push_back(loaded);
                    }
                }
                if (sound.variants.empty())
                {
                    logic_.ReportProblem(request.object, request.block,
                                         "Audio clip could not be loaded: " + request.name);
                }
                found = blockSounds_.emplace(request.name, std::move(sound)).first;
            }
            BlockSound& sound = found->second;
            if (sound.variants.empty())
            {
                logic_.ReportProblem(request.object, request.block, "Audio clip could not be loaded: " + request.name);
            }
            if (!sound.variants.empty())
            {
                const Audio::VoiceId voice =
                    audio_->Play(sound.variants[sound.next++ % sound.variants.size()], false, Audio::Bus::Sfx);
                // Nowhere in the world (a screen button's click): where the listener is, full and centred
                audio_->SetVoicePosition(
                    voice, request.position.value_or(graphics_.GetRenderer().GetRenderer3D().Camera().GetPosition()));
            }
        }
    }

    std::uint32_t GameRuntime::ObjectOf(Engine::Scene::Entity::Id entity) const
    {
        for (std::size_t i = 0; i < ecsEntities_.size() && i < scene_.objects.size(); ++i)
        {
            if (ecsEntities_[i].GetId() == entity)
            {
                return scene_.objects[i].id;
            }
        }
        return 0;
    }
}
