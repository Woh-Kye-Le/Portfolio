/* !
@file    	    LogicRunner.hpp
@author  	    c.kojat@digipen.edu [85%]
         	    zhihenghenrey.hu@digipen.edu [15%]
@date    		27/10/2026
@brief
    Declares LogicRunner, which runs the block logic of a playing game's
    objects on their ECS entities, and reports which blocks ran, the
    variables' values and any problems so the editor can show them. Also the
    block debugger's state: pause, step one block and breakpoints.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/logic/BreakpointFile.hpp"

#include "engine/logic/Blocks.hpp"
#include "engine/logic/Library.hpp"
#include "engine/scene/Entity.hpp"

#include <glm/vec3.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Engine::Scene
{
    class Scene;
    struct TransformComponent;
}

namespace Engine::Core
{
    class EventBus;
}

namespace Engine::Logic
{
    // Whether the block scripts run freely, stand still, or run one block and stop again
    enum class RunMode : std::uint8_t
    {
        Running,
        Paused,
        StepOnce,  // runs one block, then stops again
        StepFrame, // runs one frame (every script to its next wait or loop end), then stops again
    };

    // Why the scripts stopped
    enum class PauseReason : std::uint8_t
    {
        None,
        User,         // the Pause button
        Breakpoint,   // a block with a breakpoint was about to run
        Step,         // Step ran one block (with no block next: the script has ended, and all its blocks show as done)
        StepLast,     // Step ran a script's last block; the next Step shows the script as ended
        Focus,        // a click on a block picked the script Step runs
        Frame,        // Step ran one frame
        InfiniteLoop, // for F05: a script ran kMaxSteps blocks without a wait
    };

    // Where the scripts stopped, and the blocks they stop at
    struct DebugState
    {
        RunMode mode = RunMode::Running;
        PauseReason reason = PauseReason::None;
        std::uint32_t pausedObject = 0; // the object whose script it stopped in (0: none)
        std::uint32_t pausedBlock = 0;  // the block that runs next (0: none, e.g. the script has ended)
        int pausedThread = -1;          // which of the object's scripts (-1: none)
        std::uint32_t pauses = 0;       // goes up each time the scripts stop, so the editor notices every stop
        std::map<std::uint32_t, std::set<std::uint32_t>> breakpoints; // object id -> block ids

        // Stopped in no script
        void ClearPlace()
        {
            pausedObject = 0;
            pausedBlock = 0;
            pausedThread = -1;
        }
    };
    // Runs objects' block logic in Play mode, like Scratch: every script that starts with a hat is a thread of its
    // own. "when Play starts" scripts start on the first update, "when key pressed" ones each time their key goes
    // down and "when I receive" ones each time their message is broadcast (on the update after; a script already
    // running starts again from the top), unless still running. Each update a running script carries on until it
    // yields: a loop goes round once per update (the block after it runs on the update after its last round, so
    // that round is seen too), "wait", "wait until" and "glide" wait across updates, and a script
    // never runs more than kMaxSteps blocks in one update, so a long script cannot freeze the game.
    //
    // The game can be debugged (T16): Pause stops every script before its next block, Step runs one block and stops
    // again, and a breakpoint stops them all just before its block runs. While stopped nothing runs and time stands
    // still (GameRuntime stops the rest of the world too).
    //
    // Blocks act on the object's entity: Motion on its Transform (relative to its parent, for a child, as the
    // Inspector shows it; a Dynamic Rigid Body walks by its velocity instead, through PhysicsAccess), Physics on its
    // Rigid Body, Looks on its Renderable (and, for a group, on everything in it: its members follow it as it
    // moves anyway), Health on its Health. Each object has its own variables, which
    // start from their values in the logic each time it is Set. A block that cannot do its job (a missing component,
    // variable or object, a division by zero) does what it can, and is reported once as a problem in plain words (and
    // in the Console).
    class LogicRunner
    {
    public:
        static constexpr int kMaxSteps = 10000;
        // The time one frame step lets pass
        static constexpr double kFrameSeconds = 1.0 / 60.0;
        // Library blocks running library blocks deeper than this are stopped (one that runs itself, without end)
        static constexpr std::size_t kMaxCallDepth = 64;
        // A block counts as "running" (for highlighting) this long after it last ran
        static constexpr double kActiveSeconds = 0.3;

        // What keys are doing this frame, by key code (Input::KeyCode)
        struct Keys
        {
            std::function<bool(int)> down;         // held
            std::function<bool(int)> pressed;      // went down this frame
            std::function<bool(int)> mouseButton;  // a mouse button (Input::MouseButton) is held in the game
            std::function<bool(int)> mousePressed; // a mouse button went down this frame
            std::function<bool(int)> released;
            std::function<bool(int)> mouseReleased;
        };

        // A block that could not do its job
        struct Problem
        {
            std::uint32_t block = 0;
            std::string message;

            bool operator==(const Problem&) const = default;
        };

        static std::vector<Problem> ValidateVariables(const LogicDesc& logic);

        // What the Physics blocks, and Motion blocks on a moving body, need of the game's physics. GameRuntime gives
        // the runner one (Game::BlockPhysics); without it Motion blocks move the Transform, and Physics blocks report
        // that the object has no body to act on.
        class PhysicsAccess
        {
        public:
            virtual ~PhysicsAccess() = default;
            // Moves the entity's Dynamic body by offset this frame, by its velocity, so what is in the way stops it.
            // False if it has none: the Transform is moved instead.
            virtual bool Walk(Scene::Entity entity, const glm::vec3& offset) = 0;
            // Speeds its Dynamic body up by this much, once (units per second). False if it has none.
            virtual bool Push(Scene::Entity entity, const glm::vec3& velocityChange) = 0;
            // Force in world axes, for this simulation step.
            virtual bool ApplyForce(Scene::Entity, const glm::vec3&, double) { return false; }
            // Makes its Dynamic body jump this high, if it stands on something (true either way). False if it has none.
            virtual bool Jump(Scene::Entity entity, float height) = 0;
            // Whether its body stands on something; nullopt if it has no Rigid Body
            virtual std::optional<bool> OnGround(Scene::Entity entity) const = 0;
            // Whether a ray from the entity hits another body within length. direction is unit length.
            virtual bool Raycast(Scene::Entity entity, const glm::vec3& direction, float length) = 0;
            // The Surface of what is just under the entity's body (a child: its parent's body), "" when that has
            // none; nullopt when nothing is under it (in the air) or it has no body at all
            virtual std::optional<std::string> SurfaceUnder(Scene::Entity entity) const = 0;
            // A line from the entity (a first-person player: from its view, along it), or towards target when one is
            // given, starting height above the entity's feet, and up to the degrees off. What it reaches first (never
            // the entity's own body), or nullopt for nothing.
            struct RayResult
            {
                Scene::Entity::Id entity = 0;
                float distance = 0.0f;
            };
            virtual std::optional<RayResult> CastRay(Scene::Entity from, Scene::Entity target, float height,
                                                     float range, float yawOff, float pitchOff) const = 0;
        };

        // Something an object said lately ("say"), for a speech bubble over it
        struct Speech
        {
            std::uint32_t objectId = 0;
            std::string objectName;
            Scene::Entity::Id entity = 0;
            std::string text;
            double age = 0.0; // seconds since it was said
        };

        // An object of the scene by name, for "go to", "point towards" and "distance to"
        struct NamedEntity
        {
            std::string name;
            Scene::Entity::Id entity = 0;
        };

        LogicRunner();
        ~LogicRunner();
        LogicRunner(LogicRunner&&) noexcept;
        LogicRunner& operator=(LogicRunner&&) noexcept;

        // Runs (or restarts, after an edit) the object's logic on the entity; its scripts start on the next update and
        // its variables start again from their values in logic
        void Set(std::uint32_t objectId, std::string objectName, Scene::Entity::Id entity, const LogicDesc& logic);
        // Stops and forgets the object's logic
        void Remove(std::uint32_t objectId);
        void Clear();
        bool Has(std::uint32_t objectId) const;
        // Every object of the scene (with logic or not), so blocks can find one by name; the first with a name wins
        void SetObjects(std::vector<NamedEntity> objects);
        // The game's block library, for the library blocks objects Set from now on run (objects already Set keep the
        // library they started with: Set them again to use a changed one)
        void SetLibrary(Library library);
        // The game's physics for the blocks (nullptr: none). The runner does not own it.
        void SetPhysics(PhysicsAccess* physics) { physics_ = physics; }

        // Moves every object's scripts on by one frame of deltaSeconds
        void Update(Scene::Scene& ecs, double deltaSeconds, const Keys& keys);
        // Keeps this frame's presses for the next Update, for a game that does not run one every frame: a key tapped
        // on a frame without one still starts its "when key pressed" scripts
        void NoteKeyPresses(const Keys& keys);
        // Physics queues contact beginnings for the next simulation step.
        void NoteCollision(Scene::Entity::Id a, Scene::Entity::Id b);

        // The object's blocks that ran in the last kActiveSeconds, or that a script is waiting on now (sorted)
        std::vector<std::uint32_t> ActiveBlocks(std::uint32_t objectId) const;
        // How many of the object's scripts are running right now
        std::size_t RunningScripts(std::uint32_t objectId) const;
        // Where one of the object's scripts (one that starts with a hat) is
        struct ScriptState
        {
            std::size_t script = 0; // its index in the object's LogicDesc::scripts
            bool running = false;   // false before it starts and after it ends
            std::uint32_t last = 0; // the block it ran last (its hat once it starts; 0: it has not started)
            std::uint32_t next = 0; // the block it runs next, after a wait it is on (0: not started, or ends next)
            std::vector<std::uint32_t>
                before; // the other blocks it ran since the script started, once each, in first-run order (F02)
            bool justEnded =
                false; // it ended in the latest frame or Step, so its last block is still the one running (F02)

            bool operator==(const ScriptState&) const = default;
        };
        // Every script that can run, in script order
        std::vector<ScriptState> Scripts(std::uint32_t objectId) const;
        // The problems the object's blocks ran into, oldest first (each block's once)
        std::vector<Problem> Problems(std::uint32_t objectId) const;
        // What the object last said ("say" block); empty if nothing yet
        std::string LastSaid(std::uint32_t objectId) const;
        // The object's variables as they are now, in the order they were made
        std::vector<std::pair<std::string, double>> Variables(std::uint32_t objectId) const;
        // What every object said in the last maxAge seconds, newest first
        std::vector<Speech> Speeches(double maxAge) const;
        // The variables on the screen ("show variable"), by object, each in the order it was shown
        // Whether the object has a "when clicked on screen" script (its Screen Element can be clicked)
        bool HasScreenClickScript(std::uint32_t objectId) const;
        // The variable of the object called objectName as it is now (a Screen Element bar's); nullopt if neither is
        std::optional<double> VariableOf(std::string_view objectName, std::string_view variable) const;
        // "lock the mouse" (true) or "free the mouse" (false) since the last call, for the game's window to do
        std::optional<bool> TakeMouseRequest();
        // The scene "go to scene" asked for since the last call (its name), for the game to start
        std::optional<std::string> TakeSceneRequest();
        // A sound "play sound" asked for: its name (or path), and where the object that played it was. An object with
        // no place in the world (a button on the screen) is heard as if at the listener.
        struct SoundRequest
        {
            std::string name;
            std::optional<glm::vec3> position;

            std::uint32_t object = 0;
            std::uint32_t block = 0;
        };
        // The sounds asked for since the last call, oldest first, for the game to play
        std::vector<SoundRequest> TakeSoundRequests();
        // A sound asked for by something other than a block (a Lua script), played the same way
        void RequestSound(std::string name, std::optional<glm::vec3> position, std::uint32_t object)
        {
            soundRequests_.push_back({.name = std::move(name), .position = position, .object = object});
        }
        // What the Game blocks asked of the game's window since the last call, for the game to do
        struct GameRequest
        {
            enum class Kind : std::uint8_t
            {
                Fullscreen, // a: 1 on, 0 off
                WindowSize, // a by b pixels
                Quit,
            };
            Kind kind = Kind::Quit;
            float a = 0.0f;
            float b = 0.0f;
        };
        std::vector<GameRequest> TakeGameRequests();
        // The player's settings ("sensitivity", "master volume", "music volume", "sfx volume", "fullscreen", "width",
        // "height"): the game hands them over when a scene starts (they outlast it) and reads them back each frame
        void SetSettings(std::map<std::string, double> settings) { settings_ = std::move(settings); }
        const std::map<std::string, double>& Settings() const { return settings_; }
        // A variable of the object with this id, for a screen text that shows it ("{ammo}")
        std::optional<double> VariableOfObject(std::uint32_t objectId, std::string_view variable) const;
        // The object's Screen Element was clicked: its "when clicked on screen" scripts start on the next update
        void ScreenClicked(std::uint32_t objectId);

        // The event bus (nullptr: none). The runner does not own it. Each Broadcast block is also sent on it, as an
        // Events::MessageBroadcast from blocks, delivered at the start of the next frame (its receivers in the blocks
        // still hear it the usual way).
        void SetEventBus(Core::EventBus* events) { events_ = events; }
        // A message from outside the blocks (engine code, through the event bus): its "when I receive" scripts start on
        // the next update, as a Broadcast block's do, and read value as "message value"
        void Receive(std::string message, double value = 0.0);
        // The object started touching another (by name): its "when I touch" scripts for that object, or for anything,
        // start on the next update. GameRuntime passes these on from the event bus (Events::CollisionBegan).
        void Touched(std::uint32_t objectId, std::string otherName);
        // A timer ran out: the "when timer ... ends" scripts for it start on the next update (Events::TimerElapsed)
        void TimerEnded(std::string name);
        // The last time a block wrote one of the object's values
        struct Write
        {
            std::uint32_t block = 0;  // the block that wrote it
            std::uint64_t serial = 0; // rises with every write, so a new write shows even with the same value

            bool operator==(const Write&) const = default;
        };
        // The block that last wrote the variable or property since the object's logic was Set; nullopt if none has
        // (a block that could not write, e.g. no Health, does not count)
        std::optional<Write> LastWrite(std::uint32_t objectId, std::string_view variable) const;
        std::optional<Write> LastWrite(std::uint32_t objectId, Property property) const;

        // ---- Debugging (T16)

        // Stops every script before its next block. The running script of preferObject (else the first running one)
        // is the one Step runs. Nothing if already stopped.
        void Pause(std::uint32_t preferObject = 0);
        // Runs freely again; the block it stopped at runs without its breakpoint stopping it again
        void Resume();
        // While paused: runs one block of the script it stopped in, with no time passing (a wait or glide finishes at
        // once, in the same step that starts it), then stops before the next. When that script ends, the next running
        // script (of the same object first) is the one Step runs. Stopped in no script: the game runs until a block
        // runs, and stops after it. Nothing if not paused.
        void StepOnce();
        // StepOnce, but a My Block runs as one step instead of being stepped into. On any other block, the same.
        void StepOver();
        // While paused: runs one frame like Scratch's single step: every script goes on until its next wait or loop
        // end, kFrameSeconds pass (a wait counts them down), then everything stops again. The block it stopped at
        // runs (its breakpoint does not stop it again); a breakpoint further on still does. Nothing if not paused.
        void StepFrame();
        // The last Update ran a frame step: the world moves on by kFrameSeconds although the scripts stopped again
        bool FrameStepped() const { return frameStepped_; }
        // How long the player moves in a frame of deltaSeconds that has fixedSteps steps of the world to run, or
        // nullopt when it stands still. Paused, or about to run a block step: no time passes. A frame step lets one
        // step pass (kFrameSeconds), in the frame that has a step to run it in
        std::optional<double> PlayerSeconds(int fixedSteps, double deltaSeconds) const;
        // While paused: the running script holding the object's block becomes the one Step runs (like Scratch's
        // debugger, clicking a block picks its script). False (nothing changes) if not paused or that script is not
        // running.
        bool FocusScript(std::uint32_t objectId, std::uint32_t blockId);
        // Stopped (not while a step is under way)
        bool IsPaused() const { return debug_.mode == RunMode::Paused; }
        const DebugState& Debug() const { return debug_; }

        // A breakpoint stops every script just before its block runs (a hat's: as its script starts). Breakpoints
        // stay through Set, Remove and Clear.
        void ToggleBreakpoint(std::uint32_t objectId, std::uint32_t blockId);
        bool HasBreakpoint(std::uint32_t objectId, std::uint32_t blockId) const;
        void SetBreakpoints(std::uint32_t objectId, std::set<std::uint32_t> blocks);
        void ClearBreakpoints();
        // Whether a breakpoint stops: switched off, or only while its condition holds. A condition that cannot be read
        // or a variable that is not there stops anyway, so a mistake does not look like a block that never ran.
        void SetBreakpointRule(std::uint32_t objectId, std::uint32_t blockId, BreakpointRule rule);

        // On: every block is logged as it is about to run, so a block that ran but changed nothing can be told from
        // one that never ran
        void SetTrace(bool on) { traceBlocks_ = on; }
        bool Trace() const { return traceBlocks_; }

        std::uint64_t DiagnosticSession() const { return diagnosticSession_; }

        void ReportProblem(std::uint32_t objectId, std::uint32_t blockId, std::string message);

    private:
        struct ObjectLogic;
        struct Thread;

        // Update's work: starts the scripts that start now and moves every running one on
        void Run(Scene::Scene& ecs, double deltaSeconds, const Keys& keys);
        // Runs one script until it yields or ends
        void Step(ObjectLogic& object, Thread& thread, Scene::Scene& ecs, double deltaSeconds, const Keys& keys);
        // Does what a stack block does (not the control blocks). Returns true if it wrote the value it writes (so
        // the caller records the write), false if it had nothing to write to or does not write.
        bool Act(ObjectLogic& object, const Block& block, Scene::Scene& ecs, const Keys& keys);
        bool Evaluate(ObjectLogic& object, const Block& condition, Scene::Scene& ecs, const Keys& keys);
        // The block's number field i: its value block's value if it has one, else the number typed in
        double Number(ObjectLogic& object, const Block& block, std::size_t i, Scene::Scene& ecs, const Keys& keys);
        double Value(ObjectLogic& object, const Block& value, Scene::Scene& ecs, const Keys& keys);
        // The variable a block names as "Object.variable", "hit.variable" or just "variable" (its own). nullptr if
        // there is none; reported unless quiet.
        double* VariableNamed(ObjectLogic& object, const Block& block, bool quiet);
        // Where the object is: its local Transform for a child, or nullptr (reported) if it has none
        Scene::TransformComponent* Place(ObjectLogic& object, const Block& block, Scene::Scene& ecs, bool report);
        // The entity of the object called name, or 0 (reported) if there is none
        Scene::Entity::Id Named(ObjectLogic& object, const Block& block);
        void Report(ObjectLogic& object, const Block& block, std::string message);
        void ReportAt(ObjectLogic& object, std::uint32_t shownBlock, std::string message);
        // Notes that block just wrote the value it writes (a variable or a Property), for LastWrite
        void Record(ObjectLogic& object, const Block& block);

        // Before each block a script runs: false if it must not run now (stopped, a breakpoint, another script's step)
        bool BeforeBlock(ObjectLogic& object, Thread& thread, const Block& block);
        // Stops every script, in thread, before block (nullptr: none)
        void PauseAt(const ObjectLogic& object, const Thread& thread, const Block* block, PauseReason reason);
        // The block thread runs next: the wait or glide it is on, the next in its list, the loop or If holding the
        // list it finished; nullptr when it has ended
        static const Block* NextBlockOf(const Thread& thread);
        // The id a block of thread goes by in its object's script: its own, or inside a My Block that My Block's (the
        // blocks in a My Block have ids of their own, which mean other blocks in the script). 0 for no block.
        static std::uint32_t ScriptIdOf(const Thread& thread, const Block* block);
        // The block thread runs once the wait, glide or "wait until" it is on is over: the next in its list, the loop
        // holding the list when it goes round again, else the block after the loop or If; nullptr when it then ends
        static const Block* BlockAfterWait(const Thread& thread);
        // The script the game stopped in, if it is still running (else both nullptr)
        std::pair<ObjectLogic*, Thread*> PausedThread();
        // StepOnce while stopped in a script: runs just that script, one block, with no time passing
        void StepPausedThread(ObjectLogic& object, Thread& thread, Scene::Scene& ecs, const Keys& keys);
        // The step's block has run and its script yielded: a wait or glide it is on finishes at once (part of the
        // step), then it stops before its next block. A script that has ended stops there, with no next block: the next
        // Step goes on in the next running script (PauseAtNextRunning)
        void FinishStep(ObjectLogic& object, Thread& thread, Scene::Scene& ecs, const Keys& keys);
        // Stops before the next block of the next running script after ended: the same object's other scripts first,
        // then the other objects'. False if no script is running
        bool PauseAtNextRunning(const ObjectLogic& object, const Thread& ended);

        // Marks a block as just run. Inside a library block it is the library block in the object's script that is
        // marked (and reported, and waited on): the saved blocks have ids of their own.
        void Stamp(ObjectLogic& object, std::uint32_t block);
        static std::uint32_t Shown(const ObjectLogic& object, std::uint32_t block);

        std::map<std::uint32_t, std::unique_ptr<ObjectLogic>> objects_;
        std::vector<NamedEntity> named_;
        PhysicsAccess* physics_ = nullptr;
        Core::EventBus* events_ = nullptr;                           // not owned
        std::vector<std::pair<std::uint32_t, std::string>> touches_; // object, what it touched: until the next update
        std::vector<std::string> timersEnded_;                       // until the next update
        // The numbers sent with this update's messages ("broadcast ... with"), by message, for "message value"
        std::vector<std::pair<std::string, double>> messageValues_;
        // The id the object was Set with (0 if it is not one of this runner's)
        std::uint32_t IdOf(const ObjectLogic& object) const;
        std::optional<bool> mouseRequest_;
        std::optional<std::string> sceneRequest_;
        std::vector<SoundRequest> soundRequests_;
        std::vector<GameRequest> gameRequests_;
        std::map<std::string, double> settings_;
        // Messages "tell what the ray hit" sent this update, and to whom: their receivers start on the next
        std::vector<std::pair<Scene::Entity::Id, std::string>> told_;
        std::vector<std::uint32_t> clicked_; // objects clicked on the screen, until the next update
        std::set<int> notedReleases_;
        std::set<Scene::Entity::Id> collisionBegins_;
        std::set<int> notedPresses_; // keys pressed since the last update (NoteKeyPresses)
        Library library_;
        std::vector<std::string> broadcasts_; // sent this update: their receivers start on the next
        std::mt19937 random_{std::random_device{}()};
        double time_ = 0.0;        // seconds of updates so far, for ActiveBlocks
        std::uint64_t update_ = 0; // updates that ran blocks so far (a frame, or one Step), for ScriptState::justEnded
        double timerStart_ = 0.0;  // the timer block counts from here
        DebugState debug_;
        bool endShown_ =
            true; // the Step began at a script's end that was already shown (not just after its last block)
        bool BreakpointFires(std::uint32_t objectId, std::uint32_t blockId) const;
        BreakpointRules breakpointRules_;
        bool stepOver_ = false;            // the step about to run is a Step Over
        std::size_t stepOverDepth_ = 0;    // above 0: stepping over a My Block, called from this many frames deep
        const Thread* stepping_ = nullptr; // the script running a step's block (only ever set inside an Update)
        bool traceBlocks_ = false;
        bool frameStepped_ = false;
        std::uint64_t writes_ = 0; // the serial of the last Write
        std::uint64_t diagnosticSession_ = 0;
    };
}
