/* !
@file    	    GameRuntime.hpp
@author  	    c.kojat@digipen.edu [58%]
         	    w.kyele@digipen.edu [24%]
         	    j.cai@digipen.edu [10%]
         	    sung.h@digipen.edu [8%]
@date    		22/09/2026
@brief
    Declares GameRuntime, which plays a game: turns a scene description into ECS
    entities, applies live edits, runs the systems each frame and draws from the
    game camera.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/assets/Animation.hpp"
#include "engine/assets/AssetGpuCache.hpp"
#include "engine/assets/MaterialFile.hpp"
#include "engine/assets/ResourceManager.hpp"
#include "engine/audio/AudioSystem.hpp"
#include "engine/game/BlockPhysics.hpp"
#include "engine/game/GameProject.hpp"
#include "engine/game/TimerSystem.hpp"
#include "engine/graphics/DebugLines.hpp"
#include "engine/graphics/GraphicsManager.hpp"
#include "engine/input/GameInput.hpp"
#include "engine/input/InputActions.hpp"
#include "engine/logic/LogicRunner.hpp"
#include "engine/messenger/EventBus.hpp"
#include "engine/physics/PhysicsWorld.hpp"
#include "engine/scene/Scene.hpp"
#include "engine/scene/systems/PhysicsSystem.hpp"
#include "engine/scripting/LuaScript.hpp"

#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Engine::Platform
{
    class Window;
}

namespace Engine::Game
{
    // Plays any game project: turns its scene into ECS entities and runs the systems over them each frame
    class GameRuntime
    {
    public:
        GameRuntime();
        ~GameRuntime();
        GameRuntime(const GameRuntime&) = delete;
        GameRuntime& operator=(const GameRuntime&) = delete;

        void LoadProject(GameProject project);
        // Plays the project's start scene. Returns false if no project is loaded and no default game project could be
        // found
        bool Start();
        // Plays the given scene instead of the start scene (e.g. one being edited and not saved yet), with the project's
        // assets and controls
        bool Start(Engine::Scene::SceneDescription scene);
        // A loading screen's step: reads the files one object of the scene about to start uses, and holds them until
        // that Start, which then finds them already read. Call it for each object, a few a frame, to show progress
        void Preload(const Engine::Scene::GameObject& object);
        void Stop();
        bool IsRunning() const;

        // The scene as it was started or last updated (the ECS holds the live state)
        const Engine::Scene::SceneDescription& Scene() const;
        // Assets used by the running scene (empty when stopped)
        const Assets::ResourceManager& Resources() const;
        // The live ECS world the systems run over (nullptr when stopped)
        Engine::Scene::Scene* Ecs() { return ecsScene_.get(); }

        // The objects' block logic as it runs (Play mode only): which blocks ran, problems, what objects said
        const Logic::LogicRunner& Blocks() const { return logic_; }
        // The block debugger (T16): Pause, Step and breakpoints (Play mode only). While it is stopped, the whole world
        // stands still (it still draws)
        Logic::LogicRunner& Debugger() { return logic_; }

        // Lines over the scene that show what the game cannot: boxes around objects, colliders, rays (off by default)
        void SetDebugLines(const Engine::Systems::DebugLineSettings& settings) { debugLines_ = settings; }

        // For the game's screen (the HUD the editor and a built game draw): "lock / free the mouse" since the last
        // call, a click on an object's thing on the screen, and a picture the blocks show, by its name ("heart") or
        // path ("assets/art/sprites/heart.png"). The picture stays loaded until Stop; nullptr if there is none.
        std::optional<bool> TakeMouseRequest() { return logic_.TakeMouseRequest(); }
        void ScreenClicked(std::uint32_t objectId) { logic_.ScreenClicked(objectId); }
        // The scene object an entity was built from (0: none, e.g. a spawned copy)
        std::uint32_t ObjectOf(Engine::Scene::Entity::Id entity) const;
        const Assets::Texture* ScreenPicture(const std::string& name, std::string* path = nullptr);
        // The scene "go to scene" switched to (its file, relative to the game); empty while the game still plays the
        // scene it started with
        const std::string& SwitchedScene() const { return switchedScene_; }

        // A changed block library (the editor saved or removed a library block): every object's scripts start again
        // with it. Start() reads the game's own library file.
        void SetLibrary(Logic::Library library);

        // The 3D camera the scene is drawn with, or nullptr before the first Tick() has set up graphics
        Graphics::Camera3D* Camera();

        // The game's named actions, read from configs/input_actions.json by Start() (so an edited file takes effect
        // the next time the game starts); the default bindings before that. Query them through an Input::GameInput.
        const Input::InputActions& Actions() const;

        // Simulating (the default) is Play mode: objects spin and the player moves. When not simulating (the editor's
        // edit mode) the scene stands still; Play mode is paused with Debugger().
        void SetSimulating(bool simulating);
        bool IsSimulating() const;
        // Where the game's events are sent, and the blocks' too (nullptr: nowhere). Set it before Start(), or
        // GameStarted is missed.
        void SetEventBus(Core::EventBus* events);
        // True if the scene has an object marked as the player
        bool HasPlayer() const;

        // Using the game camera (the default) draws the scene from the Main camera - kept behind the player when the
        // player's camera follows - or, with no Main camera, from behind the following player. Off (the editor's
        // edit mode), Camera() is left to the caller, so the editor's own camera controls never move the game's.
        void SetUseGameCamera(bool use);

        // Applies an edited version of the running scene, matching objects by id: only what changed is touched. An
        // object added or removed becomes or stops being an entity; a component added, removed or edited is added,
        // removed or replaced on its entity, so the system that runs it reacts on the next frame. Everything else
        // keeps its live state (a player keeps its place, objects keep their spin), and an AudioSource whose clip,
        // loop, bus and 3D setting are unchanged keeps playing with the new volume and distances. Assets still in use
        // stay loaded; new ones are loaded and unused ones released.
        void UpdateScene(Engine::Scene::SceneDescription scene);

        // The ECS stress demo: count copies of an object (by id) that has a Transform and a Renderable - its mesh,
        // texture, shader, tint and spin - in a block above it, 10 x 10 per layer (repeat calls add layers on top).
        // The copies are entities only: not saved or shown in the editor Hierarchy. Throws std::invalid_argument if
        // the object has no Transform or Renderable.
        std::size_t SpawnEntities(std::size_t count, std::uint32_t templateObjectId);
        std::size_t ClearSpawnedEntities();
        std::size_t SpawnedEntityCount() const;
        std::size_t EntityCount() const;

        // The voice the object with this id is playing on; invalid when it has no AudioSource or no voice yet
        Audio::VoiceId EmitterVoice(std::uint32_t objectId) const;

        // Level of detail of the models drawn last frame (Lod.hpp): how many objects were drawn at each level, and how
        // many triangles were drawn against how many the full models have
        struct LodStats
        {
            std::array<std::size_t, 3> objects{}; // at LOD 0, 1 and 2
            std::size_t triangles = 0;
            std::size_t fullTriangles = 0;
        };
        LodStats LodStatistics();
        // The level an object (by id) was drawn with last frame and its triangles then; nothing for an object without a
        // model, or before it has been drawn
        struct LodView
        {
            int level = 0;
            int levels = 1; // how many versions its model has (1: too small to need simpler ones)
            std::size_t triangles = 0;
            float screenSize = 0.0f; // how much of the view's height it filled
        };
        std::optional<LodView> LodOf(std::uint32_t objectId);

        // The object as the game has it now (a copy): its components with the values they have in the running game
        // (a child's Transform is relative to its parent), everything else as saved. nullopt if it is not in the
        // game. Every component the game holds is in it, so a new component needs nothing here.
        std::optional<Engine::Scene::GameObject> LiveObject(std::uint32_t objectId) const;

        // Draw into part of the window (x, y = bottom-left corner in pixels, OpenGL convention) instead of all of it
        void SetViewport(int x, int y, int width, int height);
        void UseWholeWindow();

        // The editor's ground grid, drawn with the scene (off by default: games do not show it)
        void SetShowGrid(bool show);
        // The scene's objects drawn as the edges of their triangles (the editor's Wire button), to see how detailed
        // a model is, and its level of detail change; off by default
        void SetWireframe(bool wireframe);

        // The default sky, lighting and haze (Renderer::Environment); on by default. Off: flat colours, plain background
        void SetEnvironmentEnabled(bool enabled);

        // Draws a frame. While simulating, first advances the scene by deltaSeconds and moves the player from input
        // (no input = the player stands still); then, when using the game camera, places it.
        void Tick(double deltaSeconds, const Platform::Window& window, const Input::GameInput* input = nullptr,
                  Audio::AudioSystem* audio = nullptr);

        // Graphics manager the runtime uses to initialize GLEW and draw frames. The editor uses
        // Graphics().IsInitialized() to know when it is safe to make GL calls from outside.
        Graphics::GraphicsManager& Graphics() { return graphics_; }
        const Graphics::GraphicsManager& Graphics() const { return graphics_; }

        // The running scene's physics: started by Start(), stepped by Tick() in Play mode, stopped by Stop()
        Physics::PhysicsWorld& Physics() { return physics_; }
        const Physics::PhysicsWorld& Physics() const { return physics_; }

        // The entities that started or stopped touching in the last frame that simulated (empty when not playing)
        const std::vector<Systems::Collision>& Collisions() const { return collisions_; }

        // Every rigid body's collider as lines in world space: where the physics has it in Play mode, where its
        // Transform puts it otherwise
        std::vector<Systems::ColliderOutline> ColliderOutlines();

        // How fast and which way an entity's body moves in the running game, in units per second (see
        // PhysicsSystem::VelocityOf); nullopt when it has no body or the game is not playing
        std::optional<glm::vec3> VelocityOf(Engine::Scene::Entity entity) const;

        // Keeps a ray the game cast for the Raycasts debug lines, in Play mode. Game code calls this: the queries below
        // only answer, as the Physics window asks them every frame and draws its own.
        void RecordRay(const Systems::CastRay& ray);
        // The rays recorded in this frame of the world, and in the one before it (those show while paused)
        const std::vector<Systems::CastRay>& CastRays() const { return castRays_; }
        const std::vector<Systems::CastRay>& LastCastRays() const { return lastCastRays_; }
        // The world moved on: this frame's rays become the last frame's
        void EndRayFrame();

        // The id of the scene object an entity was built from; 0 for a spawned copy, or an entity not in the scene
        std::uint32_t ObjectIdOf(Engine::Scene::Entity entity) const;

        // Casts a ray in the running game: the first entity whose collider it hits (see PhysicsSystem::Raycast).
        // nullopt when it hits nothing, and in edit mode, where there are no bodies.
        std::optional<Systems::RaycastHit> Raycast(const glm::vec3& origin, const glm::vec3& direction,
                                                   float maxDistance, Engine::Scene::Entity ignore = {}) const;
        // RAY ENTRY POINT: any new block or game or editor code that casts a ray in Play must call CastRay, not Raycast,
        // so its debug line shows.
        // The way game code looks (blocks, enemy vision, a camera, a shot): Raycast, and the ray kept for the Raycasts
        // debug lines (RecordRay). direction need not be unit length. A ray of no length or no direction looks at
        // nothing: nullopt, and no line.
        std::optional<Systems::RaycastHit> CastRay(const glm::vec3& origin, const glm::vec3& direction,
                                                   float maxDistance, Engine::Scene::Entity ignore = {});

        // How far apart two entities are in the running game, collider to collider (see
        // PhysicsSystem::DistanceBetween); nullopt when out of range or not playing
        std::optional<Physics::Gap> DistanceBetween(Engine::Scene::Entity a, Engine::Scene::Entity b) const;

        // The entities whose colliders reach within radius of a point in the running game, nearest first (see
        // PhysicsSystem::EntitiesWithin); empty when not playing
        std::vector<Systems::NearbyEntity> EntitiesWithin(const glm::vec3& point, float radius,
                                                          Engine::Scene::Entity ignore = {}) const;

        // The entity an entity stands on in the running game (see PhysicsSystem::GroundUnder); nullopt when it
        // stands on nothing or the game is not playing
        std::optional<Systems::GroundHit> GroundUnder(Engine::Scene::Entity entity) const;

        // The entity built for the scene object with this id, or an invalid entity
        Engine::Scene::Entity EntityOf(std::uint32_t objectId) const;

        // What the Game blocks asked of the window since the last call (fullscreen, its size, quitting): the game's
        // own window does them; the editor, whose window it is not, only stops playing on a quit
        std::vector<Logic::LogicRunner::GameRequest> TakeGameRequests();

        const auto& LuaScripts() const { return luaScripts_; }

        // An object sends a message: "when I receive" scripts hear it, and so does anything subscribed to
        // Events::MessageBroadcast. Delivered with the queued events of the next frame.
        void EmitFromObject(std::uint32_t objectId, std::string message, double value = 0.0,
                            std::map<std::string, std::string> data = {});

    private:
        // Loads the default game project if none was given; false (logged) if there is none
        bool EnsureProject();

        // Invalid handles for the asset kinds an object does not use
        struct ObjectAssets
        {
            Assets::MeshHandle mesh;
            Assets::ModelHandle model;
            Assets::TextureHandle texture;
            Assets::ShaderHandle shader;
            Assets::AudioHandle audio;
            std::optional<Assets::BoundingBox> bounds; // around the model (or else the mesh), for its collider
            std::shared_ptr<const Assets::LoadedMaterial> material;
            std::vector<Assets::ModelHandle> animationSources; // its Animator's animation files

            // The same file through AudioSystem, which is what actually plays it.
            // ResourceManager keeps the handle so the Assets panel can list it.
        };

        // Loads the assets an object uses (each Load* adds a reference; shared files are read once)
        ObjectAssets LoadAssets(const Engine::Scene::GameObject& object);
        void ReleaseAssets(const ObjectAssets& assets);
        // One more reference to each file of an object's, so releasing the object keeps them loaded
        void HoldAssets(const ObjectAssets& assets);
        // The project's ResourceManager, made anew only for another project (one kept keeps what it has read)
        void EnsureResources();
        // Stop, keeping the graphics and the files held for a restart when keepGraphics
        void StopScene(bool keepGraphics);
        std::vector<ObjectAssets> preloaded_; // read by Preload for the next Start

        // Part of the window to draw into (the editor's Viewport), in pixels
        struct Viewport
        {
            int x = 0;
            int y = 0;
            int width = 0;
            int height = 0;
        };

        // Builds the ECS world from scene_ and swaps it in
        void RebuildEcsScene();
        // Gives entity the components object has (the one place a scene object becomes ECS components, through the
        // component list in GameRuntime.cpp): plain-data ones are copied, renderable and audio source paths become the
        // loaded handles in assets. Audio clips are added to clips. Throws std::runtime_error if a component cannot be
        // added.
        void AddComponents(Engine::Scene::Scene& ecs, Engine::Scene::Entity entity,
                           const Engine::Scene::GameObject& object, const ObjectAssets& assets,
                           std::vector<std::filesystem::path>& clips) const;
        // The scene's sky (the first object's in the scene with one; without one the default sky) into environment
        void ApplySky(Renderer::Environment& environment);
        void ApplySkyLight(Graphics::LightingContext& lighting) const;
        // Turns entity (built from before) into one built from after, touching only the components that differ
        void ApplyEdit(Engine::Scene::Entity entity, const Engine::Scene::GameObject& before,
                       const Engine::Scene::GameObject& after, const ObjectAssets& assets);
        // Links each child entity to its parent's (HierarchyComponent) and unlinks entities that are no longer
        // children; entities[i] was built from scene.objects[i]. Throws std::runtime_error if a link cannot be added.
        static void LinkParents(Engine::Scene::Scene& ecs, const Engine::Scene::SceneDescription& scene,
                                const std::vector<Engine::Scene::Entity>& entities);
        // Tells the block logic every object's name and entity, for blocks that find another object ("go to Player")
        void NameObjectsForLogic();
        // Stops the voice of the entity's AudioSource, if it has one
        void StopVoice(Engine::Scene::Entity entity);
        // A material file of the game, loaded once and shared (its shader and texture too); null without a path
        std::shared_ptr<const Assets::LoadedMaterial> MaterialOf(const std::string& path);
        // Reads a material file into material, loading the shader and texture it names (and letting go of the old)
        void ReadMaterial(const std::string& path, Assets::LoadedMaterial& material);
        // Reads again the material files saved since they were read: every object using one changes with it
        void ReloadChangedMaterials();
        // Plays the sounds the blocks asked for this frame ("play sound"), each from where its object was
        void PlayBlockSounds();
        // Keeps and applies the settings the Game blocks changed this frame
        void ApplySettings();
        // The game's audio files a "play sound" name stands for: the one of that name, or its numbered variants
        std::vector<std::string> SoundFilesNamed(const std::string& name) const;

        std::unique_ptr<GameProject> project_;
        Input::InputActions actions_ = Input::InputActions::Defaults();
        Engine::Scene::SceneDescription scene_;
        Assets::ResourceManager resources_;
        std::vector<ObjectAssets> objectAssets_;
        Assets::Mesh texturedCube_ = Assets::PlaceholderMesh(); // cube objects with a texture but no mesh
        Graphics::Mesh texturedCubeGpu_;
        Graphics::GraphicsManager graphics_;
        Physics::PhysicsWorld physics_;
        std::vector<Systems::Collision> collisions_; // from the last frame's PhysicsSystem::Update
        std::vector<Systems::CastRay> castRays_;     // recorded in this frame of the world (RecordRay)
        std::vector<Systems::CastRay> lastCastRays_; // recorded in the one before it
        Assets::AssetGpuCache assetGpuCache_;
        std::optional<Viewport> viewport_; // none: the whole window
        std::unique_ptr<Engine::Scene::Scene> ecsScene_;
        std::vector<Engine::Scene::Entity> ecsEntities_; // ecsEntities_[i] was built from scene_.objects[i]
        std::vector<Engine::Scene::Entity> spawnedEntities_;
        Logic::LogicRunner logic_;                      // the objects' block scripts, stepped in Tick while simulating
        BlockPhysics blockPhysics_;                     // how the blocks walk, jump and push bodies in physics_
        Engine::Systems::DebugLineSettings debugLines_; // which debug lines Tick draws (SetDebugLines)
        Graphics::DebugLines debugLineBatch_;           // refilled every frame the lines are on
        std::optional<std::string> pendingScene_;       // "go to scene": started at the beginning of the next Tick
        std::string switchedScene_;
        std::unordered_map<std::string, Assets::TextureHandle> screenPictures_; // by the name the blocks used
        std::unordered_map<std::string, std::string> screenPicturePaths_;
        std::vector<ObjectAssets> spawnedAssets_; // one reference per spawn batch to the files its copies draw with
        struct MaterialEntry
        {
            std::shared_ptr<Assets::LoadedMaterial> material;
            std::filesystem::file_time_type written;
        };
        std::unordered_map<std::string, MaterialEntry> materials_; // by path; loaded with resources_
        double secondsSinceMaterialCheck_ = 0.0;
        double secondsToSimulate_ = 0.0; // time the blocks and the physics have not been stepped through yet
        double playSeconds_ = 0.0;       // how long the game has simulated, for what changes by itself (flickering)
        // What the last fixed step moved, and by how much (only the position and rotation fields are used). A frame
        // between steps draws those objects part of the way back, so they move smoothly on a screen that refreshes
        // faster than the steps (a 144 Hz monitor) instead of jumping every second or third frame.
        std::vector<std::pair<Engine::Scene::Entity, Engine::Scene::TransformComponent>> lastStepMoves_;
        // AudioSourceComponent::clip is a one based idx into this table
        std::vector<std::filesystem::path> audioClips_;
        // The player's settings (Game blocks): they outlast a scene, so a menu's choices hold in the game
        std::map<std::string, double> settings_;
        std::vector<Logic::LogicRunner::GameRequest> gameRequests_; // for the window's owner (TakeGameRequests)
        // The clips "play sound" loaded, by the name the blocks used: one, or its variants (step_stone_1, _2, ...)
        // played in turn. None: no such sound, not looked for again.
        struct BlockSound
        {
            std::vector<Audio::SoundId> variants;
            std::size_t next = 0;
        };
        std::unordered_map<std::string, BlockSound> blockSounds_;
        Audio::AudioSystem* audio_ = nullptr;
        Core::EventBus* events_ = nullptr;
        // A Play run is in progress: GameStarted was sent and GameStopped not yet (the editor's frozen edit-mode view
        // is never one), and whether its last SimulationChanged said simulating
        bool playRun_ = false;
        bool announcedSimulating_ = true;
        // Messages engine code sends on the bus, passed on to the blocks' "when I receive" scripts
        Core::Subscription messagesToBlocks_;
        // The game's countdown timers ("start timer" blocks), counted down in Tick while simulating
        TimerSystem timers_;
        // Touches and timers from the bus, passed on to the blocks' "when I touch" / "when timer ends" scripts
        Core::Subscription touchesToBlocks_;
        Core::Subscription timersToBlocks_;
        bool showGrid_ = false;
        bool wireframe_ = false;
        bool environmentEnabled_ = true;
        bool running_ = false;
        bool simulating_ = true;
        bool useGameCamera_ = true;
        bool graphicsInitialized_ = false;

        // What a script of this object can do in the game
        Scripting::LuaHost LuaHostFor(std::uint32_t id);
        // Calls OnDestroy in each script, and lets them go
        void StopLuaScripts();
        // Hot reload: a .lua file (or one it requires) that changed on disk is loaded into its running script
        void ReloadChangedLuaScripts();

        static constexpr int kLuaFileCheckTicks = 30; // two checks a second
        std::map<std::uint32_t, std::unique_ptr<Scripting::LuaScript>> luaScripts_;
        static constexpr std::size_t kMaxContactMarks = 64; // the latest are shown
        std::vector<Systems::ContactMark> contactMarks_;
        static constexpr int kForceMarkTicks = 20;
        std::vector<Systems::ForceMark> forceMarks_;
        int forceMarkTicks_ = 0;
        Scripting::LuaCompileCache luaCompiled_;
        std::map<std::uint32_t, std::string> luaFiles_; // the objects whose script is a file, and that file
        std::map<std::pair<std::uint32_t, std::string>, std::filesystem::file_time_type> luaFileTimes_;
        int luaFileCheckTicks_ = 0;
        std::set<std::uint32_t> luaErrorsReported_;

        std::vector<std::pair<std::uint32_t, std::string>> startupAudioErrors_;
    };
}
