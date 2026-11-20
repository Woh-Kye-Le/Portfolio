/* !
@file    	    LogicRunner.cpp
@author  	    c.kojat@digipen.edu [80%]
         	    zhihenghenrey.hu@digipen.edu [20%]
@date    		27/10/2026
@brief
    Implements LogicRunner: one thread per script with a hat, stepped each
    frame until it yields; values, conditions, variables and messages; and
    the blocks' effects on ECS components; and the debugger (T16): pause,
    step one block and breakpoints.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/logic/LogicRunner.hpp"

#include "engine/scene/systems/AnimationSystem.hpp"

#include "engine/core/Log.hpp"
#include "engine/input/InputActions.hpp"
#include "engine/messenger/EventBus.hpp"
#include "engine/messenger/Events.hpp"
#include "engine/scene/Components.hpp"
#include "engine/scene/Scene.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <optional>

namespace Engine::Logic
{
    namespace
    {
        constexpr double kDegrees = 180.0 / 3.14159265358979;
        // How far a "shoot" reaches
        constexpr float kShotRange = 250.0f;

        std::atomic<std::uint64_t> nextDiagnosticSession{1};

        // A setting's name as the settings are kept: lower case, and a bare bus ("music") is its volume
        std::string SettingName(std::string_view text)
        {
            std::string name;
            for (const unsigned char c : text)
            {
                name.push_back(static_cast<char>(std::tolower(c)));
            }
            while (!name.empty() && name.back() == ' ')
            {
                name.pop_back();
            }
            if (name == "master" || name == "music" || name == "sfx")
            {
                name += " volume";
            }
            if (name == "sound volume" || name == "effects volume" || name == "sounds volume")
            {
                name = "sfx volume";
            }
            return name;
        }

        // The key a block's key field names, or -1 if it names none
        int KeyCodeOf(const Block& block)
        {
            const auto binding = Input::ParseBinding(block.text);
            return binding && binding->device == Input::InputBinding::Device::Key ? binding->code : -1;
        }

        // A number for the key or mouse button a block names, to remember a press by (-1: it names neither)
        constexpr int kMouseCodes = 100000;
        int PressCode(const Block& block)
        {
            const auto binding = Input::ParseBinding(block.text);
            if (!binding)
            {
                return -1;
            }
            return binding->device == Input::InputBinding::Device::Mouse ? kMouseCodes + binding->code : binding->code;
        }

        // Whether the key or mouse button a "when key pressed" names went down
        bool PressedNow(const Block& block, const LogicRunner::Keys& keys, const std::set<int>& noted)
        {
            const int code = PressCode(block);
            if (code < 0)
            {
                return false;
            }
            if (noted.contains(code))
            {
                return true;
            }
            return code >= kMouseCodes ? keys.mousePressed && keys.mousePressed(code - kMouseCodes)
                                       : keys.pressed && keys.pressed(code);
        }

        bool ReleasedNow(const Block& block, const LogicRunner::Keys& keys, const std::set<int>& noted)
        {
            const int code = PressCode(block);

            return code >= 0 && (noted.contains(code) ||
                                 (code >= kMouseCodes ? keys.mouseReleased && keys.mouseReleased(code - kMouseCodes)
                                                      : keys.released && keys.released(code)));
        }

        bool HeldNow(const Block& block, const LogicRunner::Keys& keys)
        {
            const int code = PressCode(block);

            return code >= 0 && (code >= kMouseCodes ? keys.mouseButton && keys.mouseButton(code - kMouseCodes)
                                                     : keys.down && keys.down(code));
        }

        // What is in a block's condition slot i (an empty block, id 0, if nothing)
        const Block& SlotOf(const Block& block, std::size_t i)
        {
            static const Block empty;
            return i < block.slots.size() ? block.slots[i] : empty;
        }

        bool SameWords(std::string_view a, std::string_view b)
        {
            return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
                                                      [](char x, char y) {
                                                          return std::tolower(static_cast<unsigned char>(x)) ==
                                                                 std::tolower(static_cast<unsigned char>(y));
                                                      });
        }

        bool IsWhole(double value)
        {
            return value == std::round(value);
        }

        // The entity and everything under it (its group's members, and theirs), by HierarchyComponent
        std::vector<Scene::Entity::Id> GroupOf(Scene::Scene& ecs, Scene::Entity::Id root)
        {
            std::vector<std::pair<Scene::Entity::Id, Scene::Entity::Id>> links; // child, parent
            ecs.ForEach<const Scene::HierarchyComponent>(
                [&links](Scene::Entity child, const Scene::HierarchyComponent& link)
                { links.emplace_back(child.GetId(), link.parent); });
            std::vector<Scene::Entity::Id> group{root};
            for (std::size_t i = 0; i < group.size(); ++i) // grows as members are found
            {
                for (const auto& [child, parent] : links)
                {
                    if (parent == group[i] && std::find(group.begin(), group.end(), child) == group.end())
                    {
                        group.push_back(child);
                    }
                }
            }
            return group;
        }
    }

    // A place in a list of blocks: the script's own list, or the inside of a C block
    struct LogicRunner::Thread
    {
        struct Frame
        {
            const std::vector<Block>* list = nullptr;
            std::size_t index = 0;
            int repeatsLeft = 0;    // for a Repeat at index: rounds still to go
            std::uint32_t call = 0; // the library block in the script these blocks run for (0: the script's own)
            bool isCall = false;    // the list is a library block's blocks: when it ends, go on after the call
        };

        // A "glide" in progress: from where the object was to where it goes, over duration seconds
        struct Glide
        {
            std::uint32_t block = 0; // 0: not gliding
            std::array<float, 3> from{};
            std::array<float, 3> to{};
            double duration = 0.0;
            double elapsed = 0.0;
        };

        const Script* script = nullptr; // starts with its hat
        std::vector<Frame> frames;      // empty: not running
        double timerElapsed = 0.0;      //fyi do not reset inside thread beg, must retain timing between runs
        double waitLeft = 0.0;          // seconds of a "wait" still to go
        Glide glide;
        double messageValue = 0.0;   // the number sent with the message that last started it ("message value")
        std::uint32_t waitingOn = 0; // the block it yielded on (a wait, a loop), for highlighting
        std::uint32_t lastBlock = 0; // the block it ran last (the hat, if none yet), for Scripts
        std::vector<std::uint32_t>
            trail;                     // the other blocks it ran since the script started, once each, for highlighting
        std::uint64_t lastUpdate = 0;  // the update lastBlock ran in
        bool started = false;          // a "when Play starts" script runs once
        std::uint32_t skipBreakAt = 0; // the breakpoint it stopped at: that block runs once without stopping again

        // From the top, under the hat
        void Begin()
        {
            frames.assign(1, Frame{.list = &script->blocks, .index = 1});
            waitLeft = 0.0;
            glide = {};
            started = true;
        }
    };

    struct LogicRunner::ObjectLogic
    {
        float movementSpeed = 1.0f;
        double stepSeconds = 0.0;
        std::uint32_t id = 0;
        std::string name;
        Scene::Entity::Id entity = 0;
        LogicDesc logic; // the threads point into it, so it is never changed (Set makes a new ObjectLogic)
        std::vector<Thread> threads;
        std::vector<std::pair<std::string, double>> variables;
        std::map<std::uint32_t, double> lastRun; // block id -> time it last ran
        std::vector<Problem> problems;
        std::string said;
        std::map<std::string, Write, std::less<>> writes;                // variable name -> its last write
        std::array<std::optional<Write>, kPropertyCount> propertyWrites; // by Property
        double saidAt = 0.0;                                             // time_ when it said it
        Library library;                                                 // a copy: threads point into it
        std::uint32_t inCall = 0;     // while a library block's blocks run: that library block (see Stamp)
        double messageValue = 0.0;    // while a script runs: its "message value"
        Scene::Entity::Id rayHit = 0; // what its last ray reached (0: nothing)
        float rayDistance = 0.0f;     // and how far off
    };

    LogicRunner::LogicRunner() : diagnosticSession_(nextDiagnosticSession.fetch_add(1)) {}
    LogicRunner::~LogicRunner() = default;
    LogicRunner::LogicRunner(LogicRunner&&) noexcept = default;
    LogicRunner& LogicRunner::operator=(LogicRunner&&) noexcept = default;

    void LogicRunner::Set(std::uint32_t objectId, std::string objectName, Scene::Entity::Id entity,
                          const LogicDesc& logic)
    {
        auto object = std::make_unique<ObjectLogic>();
        object->name = std::move(objectName);
        object->entity = entity;
        object->id = objectId;
        object->logic = logic;
        for (const Script& script : object->logic.scripts)
        {
            if (!script.blocks.empty() && InfoOf(script.blocks.front().op).shape == BlockShape::Hat)
            {
                object->threads.push_back(Thread{.script = &script});
            }
        }
        for (const Variable& variable : logic.variables)
        {
            object->variables.emplace_back(variable.name, variable.value);
        }
        // A script the game stopped in is replaced (the object's scripts start again): still paused, in no script
        if (debug_.pausedObject == objectId)
        {
            debug_.ClearPlace();
        }
        object->library = library_;
        objects_[objectId] = std::move(object);
    }

    void LogicRunner::Remove(std::uint32_t objectId)
    {
        if (debug_.pausedObject == objectId)
        {
            debug_.ClearPlace();
        }
        objects_.erase(objectId);
    }

    void LogicRunner::Clear()
    {
        diagnosticSession_ = nextDiagnosticSession.fetch_add(1);
        objects_.clear();
        broadcasts_.clear();
        notedPresses_.clear();
        notedReleases_.clear();
        collisionBegins_.clear();
        time_ = 0.0;
        mouseRequest_.reset();
        sceneRequest_.reset();
        soundRequests_.clear();
        gameRequests_.clear();
        told_.clear();
        clicked_.clear();
        timerStart_ = 0.0;
        touches_.clear();
        timersEnded_.clear();
        messageValues_.clear();
        // A new game runs freely; the breakpoints stay (the editor sets its own when Play starts)
        debug_.mode = RunMode::Running;
        debug_.reason = PauseReason::None;
        debug_.ClearPlace();
        stepping_ = nullptr;
    }

    bool LogicRunner::Has(std::uint32_t objectId) const
    {
        return objects_.contains(objectId);
    }

    void LogicRunner::SetObjects(std::vector<NamedEntity> objects)
    {
        named_ = std::move(objects);
    }

    void LogicRunner::SetLibrary(Library library)
    {
        library_ = std::move(library);
    }

    std::uint32_t LogicRunner::Shown(const ObjectLogic& object, std::uint32_t block)
    {
        return object.inCall != 0 ? object.inCall : block;
    }

    void LogicRunner::Stamp(ObjectLogic& object, std::uint32_t block)
    {
        object.lastRun[Shown(object, block)] = time_;
    }

    void LogicRunner::Update(Scene::Scene& ecs, double deltaSeconds, const Keys& keys)
    {
        frameStepped_ = false;
        if (debug_.mode == RunMode::Paused)
        {
            return; // nothing runs and time stands still, so the running blocks' glow stays where it is
        }
        ++update_;
        if (debug_.mode == RunMode::StepFrame)
        {
            // One frame of the free-running game: the block it stopped at runs, then the scripts stop again below
            const std::uint32_t object = debug_.pausedObject;
            const int index = debug_.pausedThread;
            if (Thread* thread = PausedThread().second)
            {
                thread->skipBreakAt = debug_.pausedBlock;
            }
            debug_.mode = RunMode::Running;
            debug_.reason = PauseReason::None;
            frameStepped_ = true;
            Run(ecs, kFrameSeconds, keys);
            if (debug_.mode == RunMode::Running)
            {
                // Stopped again, in the script it was in if that still runs, else the next running one
                const auto found = objects_.find(object);
                if (found != objects_.end() && index >= 0 &&
                    static_cast<std::size_t>(index) < found->second->threads.size() &&
                    !found->second->threads[static_cast<std::size_t>(index)].frames.empty())
                {
                    const Thread& thread = found->second->threads[static_cast<std::size_t>(index)];
                    PauseAt(*found->second, thread, NextBlockOf(thread), PauseReason::Frame);
                }
                else
                {
                    Pause(object);
                    debug_.reason = PauseReason::Frame;
                }
            }
            return;
        }
        Run(ecs, deltaSeconds, keys);
    }

    void LogicRunner::Run(Scene::Scene& ecs, double deltaSeconds, const Keys& keys)
    {
        if (debug_.mode == RunMode::StepOnce)
        {
            if (const auto [object, thread] = PausedThread(); thread)
            {
                StepPausedThread(*object, *thread, ecs, keys);
                return;
            }
            // Stopped at the end of a script: Step shows it as ended, then goes on with the next running script
            if (const auto found = objects_.find(debug_.pausedObject);
                found != objects_.end() && debug_.pausedThread >= 0 &&
                static_cast<std::size_t>(debug_.pausedThread) < found->second->threads.size())
            {
                const Thread& ended = found->second->threads[static_cast<std::size_t>(debug_.pausedThread)];
                if (!endShown_)
                {
                    PauseAt(*found->second, ended, nullptr, PauseReason::Step);
                    return;
                }
                if (PauseAtNextRunning(*found->second, ended))
                {
                    return;
                }
            }
            // Stopped in no script: the game goes on (below) until a block runs, and stops before the next
        }
        time_ += deltaSeconds;
        // Messages broadcast in the last update start their receivers now (so every object hears them the same frame)
        std::vector<std::string> messages;
        messages.swap(broadcasts_);
        std::vector<std::uint32_t> clicked;
        clicked.swap(clicked_);
        std::vector<std::pair<std::uint32_t, std::string>> touches;
        touches.swap(touches_);
        std::vector<std::string> timersEnded;
        timersEnded.swap(timersEnded_);
        std::vector<std::pair<std::string, double>> values;
        values.swap(messageValues_);
        std::vector<std::pair<Scene::Entity::Id, std::string>> told;
        told.swap(told_);
        std::set<Scene::Entity::Id> contacts;
        contacts.swap(collisionBegins_);

        for (auto& [id, object] : objects_)
        {
            for (Thread& thread : object->threads)
            {
                const Block& hat = thread.script->blocks.front();
                bool timerDue = false;

                if (hat.op == Op::WhenTimer)
                {
                    const double interval = Number(*object, hat, 0, ecs, keys);

                    if (!std::isfinite(interval) || interval <= 0.0)
                    {
                        Report(*object, hat, "Timer interval must be greater than zero");

                        thread.timerElapsed = 0.0;
                    }
                    else
                    {
                        thread.timerElapsed += deltaSeconds;
                        timerDue = thread.timerElapsed + 1e-9 >= interval;

                        // Skip missed ticks rather than starting overlapping runs.
                        if (timerDue)
                        {
                            thread.timerElapsed =
                                thread.timerElapsed < interval ? 0.0 : std::fmod(thread.timerElapsed, interval);
                        }
                    }
                }
                bool start = false;
                if (hat.op == Op::WhenIReceive)
                {
                    start = std::any_of(messages.begin(), messages.end(),
                                        [&hat](const std::string& message) { return SameWords(message, hat.text); }) ||
                            std::any_of(told.begin(), told.end(), [&hat, &object](const auto& each)
                                        { return each.first == object->entity && SameWords(each.second, hat.text); });
                }
                else if (thread.frames.empty())
                {
                    start = hat.op == Op::WhenPlayStarts ? !thread.started
                            : hat.op == Op::WhenScreenClicked
                                ? std::find(clicked.begin(), clicked.end(), id) != clicked.end()
                            : hat.op == Op::WhenKeyPressed  ? PressedNow(hat, keys, notedPresses_)
                            : hat.op == Op::WhenKeyReleased ? ReleasedNow(hat, keys, notedReleases_)
                            : hat.op == Op::WhenKeyHeld     ? HeldNow(hat, keys)
                            : hat.op == Op::WhenCollision   ? contacts.contains(object->entity)
                                                            : hat.op == Op::WhenTimer && timerDue;
                }
                // Hats started through the event bus: touching another object, a timer running out
                if (!start && thread.frames.empty())
                {
                    if (hat.op == Op::WhenITouch)
                    {
                        const bool anything = hat.text.empty() || SameWords(hat.text, "anything");
                        start = std::any_of(
                            touches.begin(), touches.end(), [&](const std::pair<std::uint32_t, std::string>& touch)
                            { return touch.first == id && (anything || SameWords(touch.second, hat.text)); });
                    }
                    else if (hat.op == Op::WhenTimerEnds)
                    {
                        start = std::any_of(timersEnded.begin(), timersEnded.end(),
                                            [&hat](const std::string& name) { return SameWords(name, hat.text); });
                    }
                }
                if (start)
                {
                    thread.Begin();
                    thread.trail.clear();
                    thread.lastBlock = hat.id;
                    thread.lastUpdate = update_;
                    object->lastRun[hat.id] = time_;
                    // A breakpoint on the hat stops the script as it starts, before its first block
                    if (debug_.mode == RunMode::Running && HasBreakpoint(id, hat.id))
                    {
                        PauseAt(*object, thread, &hat, PauseReason::Breakpoint);
                    }
                }
                if (start && hat.op == Op::WhenIReceive)
                {
                    thread.messageValue = 0.0;
                    for (const auto& [message, value] : values)
                    {
                        if (SameWords(message, hat.text))
                        {
                            thread.messageValue = value; // the last one sent wins
                        }
                    }
                }
                // Once stopped, the other scripts still start (so none misses a message) but run no blocks
                if (!thread.frames.empty() && debug_.mode != RunMode::Paused)
                {
                    Step(*object, thread, ecs, deltaSeconds, keys);
                    // The step's block ran, then its script yielded (a wait, a loop going round, the end) before
                    // reaching another block: stop where it goes on
                    if (debug_.mode == RunMode::StepOnce && stepping_ == &thread)
                    {
                        FinishStep(*object, thread, ecs, keys);
                    }
                }
            }
        }
        notedPresses_.clear();
        notedReleases_.clear();
    }

    void LogicRunner::NoteKeyPresses(const Keys& keys)
    {
        for (const auto& [id, object] : objects_)
        {
            for (const Thread& thread : object->threads)
            {
                const Block& hat = thread.script->blocks.front();

                if (hat.op == Op::WhenKeyPressed && PressedNow(hat, keys, {}))
                {
                    notedPresses_.insert(PressCode(hat));
                }

                if (hat.op == Op::WhenKeyReleased && ReleasedNow(hat, keys, {}))
                {
                    notedReleases_.insert(PressCode(hat));
                }
            }
        }
    }

    void LogicRunner::NoteCollision(Scene::Entity::Id a, Scene::Entity::Id b)
    {
        if (a != 0)
        {
            collisionBegins_.insert(a);
        }

        if (b != 0)
        {
            collisionBegins_.insert(b);
        }
    }

    void LogicRunner::Receive(std::string message, double value)
    {
        messageValues_.emplace_back(message, value);
        broadcasts_.push_back(std::move(message));
    }

    void LogicRunner::Touched(std::uint32_t objectId, std::string otherName)
    {
        touches_.emplace_back(objectId, std::move(otherName));
    }

    void LogicRunner::TimerEnded(std::string name)
    {
        timersEnded_.push_back(std::move(name));
    }

    std::uint32_t LogicRunner::IdOf(const ObjectLogic& object) const
    {
        for (const auto& [id, other] : objects_)
        {
            if (other.get() == &object)
            {
                return id;
            }
        }
        return 0;
    }

    void LogicRunner::Step(ObjectLogic& object, Thread& thread, Scene::Scene& ecs, double deltaSeconds,
                           const Keys& keys)
    {
        object.inCall = 0;
        object.stepSeconds = deltaSeconds;
        object.messageValue = thread.messageValue;
        if (thread.glide.block != 0)
        {
            Thread::Glide& glide = thread.glide;
            glide.elapsed += deltaSeconds;
            const float t = static_cast<float>(std::min(1.0, glide.elapsed / glide.duration));
            if (Scene::TransformComponent* place = Place(object, Block{}, ecs, false))
            {
                place->x = glide.from[0] + (glide.to[0] - glide.from[0]) * t;
                place->y = glide.from[1] + (glide.to[1] - glide.from[1]) * t;
                place->z = glide.from[2] + (glide.to[2] - glide.from[2]) * t;
            }
            object.lastRun[glide.block] = time_; // already the block shown (see GlideTo)
            if (t < 1.0f)
            {
                thread.waitingOn = glide.block;
                return;
            }
            glide = {};
        }
        if (thread.waitLeft > 0.0)
        {
            thread.waitLeft -= deltaSeconds;
            if (thread.waitLeft > 0.0)
            {
                return;
            }
            thread.waitLeft = 0.0;
        }
        thread.waitingOn = 0;

        std::vector<Thread::Frame>& frames = thread.frames;
        int executed = 0;
        for (int step = 0; step < kMaxSteps * 4 && !frames.empty(); ++step)
        {
            Thread::Frame& frame = frames.back();
            object.inCall = frame.call;
            if (frame.index >= frame.list->size())
            {
                // The end of a list: of the script (done), of a library block's blocks (on after the call), or of
                // the inside of a C block
                const bool wasCall = frame.isCall;
                frames.pop_back();
                if (frames.empty())
                {
                    return;
                }
                if (wasCall)
                {
                    continue;
                }
                Thread::Frame& parent = frames.back();
                object.inCall = parent.call;
                const Block& owner = (*parent.list)[parent.index];
                if (owner.op == Op::RepeatUntil)
                {
                    // One round per frame: next frame the "repeat until" checks its condition again, and goes on
                    // to the next block only then
                    thread.waitingOn = Shown(object, owner.id);
                    return;
                }
                if (owner.op == Op::Forever)
                {
                    // One round per frame: next frame the loop block runs again, then its body
                    return;
                }
                if (owner.op == Op::Repeat)
                {
                    if (--parent.repeatsLeft > 0)
                    {
                        return; // the same: the loop block runs again next frame
                    }
                    parent.repeatsLeft = 0;
                }
                ++parent.index; // the loop is done, or the If's blocks have run
                if (owner.op == Op::Repeat || owner.op == Op::RepeatUntil)
                {
                    // Its last round is shown for a frame too: the block after it runs on the next update
                    thread.waitingOn = Shown(object, owner.id);
                    return;
                }
                continue;
            }

            const Block& block = (*frame.list)[frame.index];

            if (executed >= kMaxSteps)
            {
                Report(object, block, "Possible infinite loop detected");
                PauseAt(object, thread, &block, PauseReason::InfiniteLoop);
                frameStepped_ = false;
                return;
            }

            if (!BeforeBlock(object, thread, block))
            {
                return;
            }

            ++executed;

            const std::uint32_t call = frame.call; // what the blocks pushed below run for
            const std::uint32_t shown = Shown(object, block.id);
            Stamp(object, block.id);
            // Every block that ran since the script started stays in the trail, once, also when a loop goes round
            if (shown != thread.lastBlock && thread.lastBlock != 0 &&
                std::find(thread.trail.begin(), thread.trail.end(), thread.lastBlock) == thread.trail.end())
            {
                thread.trail.push_back(thread.lastBlock);
            }
            thread.lastBlock = shown;
            thread.lastUpdate = update_;
            switch (block.op)
            {
            case Op::Call:
            {
                const LibraryBlock* saved = object.library.Find(block.text);
                if (!saved)
                {
                    ++frame.index; // carries on under it once the library block's blocks have run
                    Report(object, block,
                           "There is no block called \"" + block.text +
                               "\" in My Blocks. Save one under that name (right-click a block > Save as My "
                               "Block).");
                    break;
                }
                if (frames.size() >= kMaxCallDepth)
                {
                    Report(object, block, "Possible infinite loop detected");
                    PauseAt(object, thread, &block, PauseReason::InfiniteLoop);
                    frameStepped_ = false;
                    return;
                }
                ++frame.index;
                // Its inputs are variables its blocks read
                for (std::size_t i = 0; i < saved->inputs.size(); ++i)
                {
                    const double given = i < block.numbers.size() ? Number(object, block, i, ecs, keys) : 0.0;
                    const auto found = std::find_if(object.variables.begin(), object.variables.end(),
                                                    [&](const auto& each) { return each.first == saved->inputs[i]; });
                    if (found != object.variables.end())
                    {
                        found->second = given;
                    }
                    else
                    {
                        object.variables.emplace_back(saved->inputs[i], given);
                    }
                }
                frames.push_back({.list = &saved->blocks, .call = call != 0 ? call : block.id, .isCall = true});
                break;
            }
            case Op::Wait:
                ++frame.index;
                thread.waitLeft = std::max(0.0, Number(object, block, 0, ecs, keys));
                if (thread.waitLeft > 0.0)
                {
                    thread.waitingOn = Shown(object, block.id);
                    return;
                }
                break;
            case Op::GlideTo:
            {
                ++frame.index;
                const double seconds = Number(object, block, 0, ecs, keys);
                const std::array to{static_cast<float>(Number(object, block, 1, ecs, keys)),
                                    static_cast<float>(Number(object, block, 2, ecs, keys)),
                                    static_cast<float>(Number(object, block, 3, ecs, keys))};
                Scene::TransformComponent* place = Place(object, block, ecs, true);
                if (!place)
                {
                    break;
                }
                Record(object, block);
                if (seconds <= 0.0)
                {
                    place->x = to[0];
                    place->y = to[1];
                    place->z = to[2];
                    break;
                }
                thread.glide = Thread::Glide{.block = Shown(object, block.id),
                                             .from = {place->x, place->y, place->z},
                                             .to = to,
                                             .duration = seconds};
                thread.waitingOn = thread.glide.block;
                return;
            }
            case Op::Repeat:
            {
                if (frame.repeatsLeft == 0)
                {
                    // The first time here: count the rounds. Coming back for the next round keeps the count
                    const double times = std::round(Number(object, block, 0, ecs, keys));
                    if (times <= 0.0)
                    {
                        ++frame.index;
                        break;
                    }
                    frame.repeatsLeft = static_cast<int>(std::min(times, 1000000.0));
                }
                frames.push_back({.list = &block.body, .call = call}); // frame is not used after this
                break;
            }
            case Op::RepeatUntil:
                if (Evaluate(object, SlotOf(block, 0), ecs, keys))
                {
                    ++frame.index;
                }
                else
                {
                    frames.push_back({.list = &block.body, .call = call});
                }
                break;
            case Op::Forever: frames.push_back({.list = &block.body, .call = call}); break;
            case Op::If:
                if (Evaluate(object, SlotOf(block, 0), ecs, keys))
                {
                    frames.push_back({.list = &block.body, .call = call});
                }
                else
                {
                    ++frame.index;
                }
                break;
            case Op::IfElse:
            {
                const bool yes = Evaluate(object, SlotOf(block, 0), ecs, keys);
                frames.push_back({.list = yes ? &block.body : &block.elseBody, .call = call});
                break;
            }
            case Op::WaitUntil:
                if (!Evaluate(object, SlotOf(block, 0), ecs, keys))
                {
                    thread.waitingOn = Shown(object, block.id);
                    return;
                }
                ++frame.index;
                break;
            case Op::Stop: frames.clear(); return;
            default:
                if (Act(object, block, ecs, keys))
                {
                    Record(object, block); // what it wrote (its table row says what)
                }
                ++frame.index;
                break;
            }
        }
        if (const Block* block = NextBlockOf(thread))
        {
            Report(object, *block, "Possible infinite loop detected");
            PauseAt(object, thread, block, PauseReason::InfiniteLoop);
            frameStepped_ = false;
        }
    }

    Scene::TransformComponent* LogicRunner::Place(ObjectLogic& object, const Block& block, Scene::Scene& ecs,
                                                  bool report)
    {
        const Scene::Entity entity = ecs.FindEntity(object.entity);
        Scene::TransformComponent* found = nullptr;
        if (entity)
        {
            auto* link = ecs.TryGetComponent<Scene::HierarchyComponent>(entity);
            found = link ? &link->local : ecs.TryGetComponent<Scene::TransformComponent>(entity);
        }
        if (!found && report)
        {
            Report(object, block,
                   object.name + " has no Transform, so it has no place to move or be. Add a Transform in the "
                                 "Inspector.");
        }
        return found;
    }

    Scene::Entity::Id LogicRunner::Named(ObjectLogic& object, const Block& block)
    {
        const auto exact = std::find_if(named_.begin(), named_.end(),
                                        [&block](const NamedEntity& each) { return each.name == block.text; });
        const auto found = exact != named_.end()
                               ? exact
                               : std::find_if(named_.begin(), named_.end(), [&block](const NamedEntity& each)
                                              { return SameWords(each.name, block.text); });
        if (found == named_.end())
        {
            Report(object, block, "There is no object called \"" + block.text + "\" in the scene.");
            return 0;
        }
        return found->entity;
    }

    double* LogicRunner::VariableNamed(ObjectLogic& object, const Block& block, bool quiet)
    {
        const std::size_t dot = block.text.rfind('.');
        const std::string owner = dot == std::string::npos ? std::string() : block.text.substr(0, dot);
        const std::string name = dot == std::string::npos ? block.text : block.text.substr(dot + 1);
        const bool ofHit = SameWords(owner, "hit");
        ObjectLogic* holder = owner.empty() ? &object : nullptr;
        for (auto& [id, each] : objects_)
        {
            if (holder)
            {
                break;
            }
            if (ofHit ? (object.rayHit != 0 && each->entity == object.rayHit) : SameWords(each->name, owner))
            {
                holder = each.get();
            }
        }
        if (holder)
        {
            for (auto& [variable, value] : holder->variables)
            {
                if (SameWords(variable, name))
                {
                    return &value;
                }
            }
        }
        // What a ray hits often has no such variable (a wall has no health): that is not a mistake in the blocks
        if (!quiet && !ofHit)
        {
            Report(object, block, "There is no variable \"" + block.text + "\". Write it as Object.variable.");
        }
        return nullptr;
    }

    double LogicRunner::Number(ObjectLogic& object, const Block& block, std::size_t i, Scene::Scene& ecs,
                               const Keys& keys)
    {
        if (i < block.inputs.size() && block.inputs[i].id != 0)
        {
            return Value(object, block.inputs[i], ecs, keys);
        }
        return i < block.numbers.size() ? block.numbers[i] : 0.0;
    }

    double LogicRunner::Value(ObjectLogic& object, const Block& value, Scene::Scene& ecs, const Keys& keys)
    {
        Stamp(object, value.id);
        const auto n = [&](std::size_t i) { return Number(object, value, i, ecs, keys); };
        const Scene::Entity self = ecs.FindEntity(object.entity);
        switch (value.op)
        {
        case Op::Add: return n(0) + n(1);
        case Op::Subtract: return n(0) - n(1);
        case Op::Multiply: return n(0) * n(1);
        case Op::Divide:
        case Op::Mod:
        {
            const double a = n(0);
            const double b = n(1);
            if (b == 0.0)
            {
                Report(object, value, "Dividing by zero has no answer, so this gives 0.");
                return 0.0;
            }
            return value.op == Op::Divide ? a / b : a - b * std::floor(a / b);
        }
        case Op::Random:
        {
            const double a = n(0);
            const double b = n(1);
            const double low = std::min(a, b);
            const double high = std::max(a, b);
            if (IsWhole(low) && IsWhole(high))
            {
                return static_cast<double>(std::uniform_int_distribution<long long>(
                    static_cast<long long>(low), static_cast<long long>(high))(random_));
            }
            return std::uniform_real_distribution<double>(low, high)(random_);
        }
        case Op::Round: return std::round(n(0));
        case Op::Abs: return std::abs(n(0));
        case Op::VariableValue:
        {
            for (const auto& [name, current] : object.variables)
            {
                if (name == value.text)
                {
                    return current;
                }
            }
            Report(object, value,
                   "There is no variable called \"" + value.text +
                       "\". Make it with Make a Variable in the "
                       "Variables blocks.");
            return 0.0;
        }
        case Op::Timer: return time_ - timerStart_;
        case Op::MessageValue: return object.messageValue;
        case Op::PositionX:
        case Op::PositionY:
        case Op::PositionZ:
        case Op::Direction:
        {
            const Scene::TransformComponent* place = Place(object, value, ecs, true);
            if (!place)
            {
                return 0.0;
            }
            return value.op == Op::PositionX   ? place->x
                   : value.op == Op::PositionY ? place->y
                   : value.op == Op::PositionZ ? place->z
                                               : place->rotation;
        }
        case Op::RayDistance: return object.rayDistance;
        case Op::ValueOf:
        {
            const double* named = VariableNamed(object, value, false);
            return named ? *named : 0.0;
        }
        case Op::SettingOf:
        {
            const auto found = settings_.find(SettingName(value.text));
            return found != settings_.end() ? found->second : 0.0;
        }
        case Op::DistanceTo:
        {
            const Scene::Entity::Id target = Named(object, value);
            const auto from = self ? ecs.GetTransform(self) : std::nullopt;
            const auto to = target != 0 ? ecs.GetTransform(ecs.FindEntity(target)) : std::nullopt;
            if (!from || !to)
            {
                return 0.0;
            }
            const double dx = to->x - from->x;
            const double dy = to->y - from->y;
            const double dz = to->z - from->z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }
        case Op::DirectionOf:
        {
            const Scene::Entity::Id target = Named(object, value);
            const auto there = target != 0 ? ecs.GetTransform(ecs.FindEntity(target)) : std::nullopt;
            return there ? there->rotation : 0.0;
        }
        case Op::TiltOf:
        {
            const Scene::Entity::Id target = Named(object, value);
            const Scene::Entity found = target != 0 ? ecs.FindEntity(target) : Scene::Entity{};
            if (!found)
            {
                return 0.0;
            }
            // A player the camera follows aims where its camera looks; anything else is tipped by its tilt
            const auto* player = ecs.TryGetComponent<Scene::PlayerControllerComponent>(found);
            if (player && player->followCamera)
            {
                return player->cameraPitch;
            }
            const auto there = ecs.GetTransform(found);
            return there ? there->rotationX : 0.0;
        }
        default: return 0.0;
        }
    }

    bool LogicRunner::Act(ObjectLogic& object, const Block& block, Scene::Scene& ecs, const Keys& keys)
    {
        // A broadcast also goes on the event bus. Queued rather than sent now: a handler must not change the objects
        // while this update walks them.
        if (block.op == Op::Broadcast && events_ != nullptr)
        {
            events_->Enqueue(Events::MessageBroadcast{block.text, IdOf(object), true});
        }
        // "broadcast ... with ...": heard like a Broadcast, with a number the receivers read as "message value"
        if (block.op == Op::BroadcastWithValue)
        {
            const double value = Number(object, block, 0, ecs, keys);
            messageValues_.emplace_back(block.text, value);
            broadcasts_.push_back(block.text);
            if (events_ != nullptr)
            {
                events_->Enqueue(Events::MessageBroadcast{block.text, IdOf(object), true, value});
            }
            return false;
        }
        // "start timer": the game's TimerSystem counts it down (it hears TimerStarted on the bus)
        if (block.op == Op::StartTimer)
        {
            if (events_ != nullptr)
            {
                events_->Enqueue(Events::TimerStarted{block.text, Number(object, block, 0, ecs, keys), IdOf(object)});
            }
            else
            {
                Report(object, block, "Timers only count down while the game is running.");
            }
            return false;
        }
        const Scene::Entity entity = ecs.FindEntity(object.entity);
        const auto n = [&](std::size_t i) { return static_cast<float>(Number(object, block, i, ecs, keys)); };
        // What the object draws, and what everything in it draws if it is a group: Looks blocks on a group change
        // all of its members
        const auto renderers = [&]() -> std::vector<Scene::MeshRendererComponent*>
        {
            std::vector<Scene::MeshRendererComponent*> found;
            for (const Scene::Entity::Id member : GroupOf(ecs, object.entity))
            {
                const Scene::Entity each = ecs.FindEntity(member);
                if (auto* renderer = each ? ecs.TryGetComponent<Scene::MeshRendererComponent>(each) : nullptr)
                {
                    found.push_back(renderer);
                }
            }
            // What it shows on the screen counts as drawing something
            if (found.empty() && !(entity && ecs.TryGetComponent<Scene::ScreenElementComponent>(entity)))
            {
                Report(object, block,
                       object.name + " has nothing to draw (and nothing in it has), so it cannot change how it looks. "
                                     "Add a Renderable in the Inspector.");
            }
            return found;
        };
        const auto variable = [&]() -> double*
        {
            for (auto& [name, value] : object.variables)
            {
                if (name == block.text)
                {
                    return &value;
                }
            }
            Report(object, block,
                   "There is no variable called \"" + block.text +
                       "\". Make it with Make a Variable in the "
                       "Variables blocks.");
            return nullptr;
        };
        // Moves the object by an offset: a Dynamic body walks there by its velocity, so walls stop it and it pushes
        // what it walks into; anything else (and a child, which moves with its parent) is put there
        const auto walk = [&](Scene::TransformComponent& t, float dx, float dy, float dz)
        {
            const bool child = entity && ecs.TryGetComponent<Scene::HierarchyComponent>(entity) != nullptr;
            if (physics_ && entity && !child && physics_->Walk(entity, glm::vec3(dx, dy, dz)))
            {
                return;
            }
            t.x += dx;
            t.y += dy;
            t.z += dz;
        };
        const auto noBody = [&]
        {
            Report(object, block,
                   object.name + " needs a Dynamic Rigid Body to jump or be pushed. Give it a Rigid Body with Type: "
                                 "Dynamic.");
        };
        const auto push = [&](const glm::vec3& velocityChange)
        {
            if (!(physics_ && entity && physics_->Push(entity, velocityChange)))
            {
                noBody();
            }
        };
        // The object's thing on the screen: its Screen Element, given one (not shown yet) if it has none
        const auto element = [&]() -> Scene::ScreenElementComponent*
        {
            if (!entity)
            {
                return nullptr;
            }
            if (!ecs.TryGetComponent<Scene::ScreenElementComponent>(entity))
            {
                ecs.AddComponent(entity, Scene::ScreenElementComponent{.visible = false});
            }
            return ecs.TryGetComponent<Scene::ScreenElementComponent>(entity);
        };
        // Where the object called by the block's text is in the world
        const auto target = [&]() -> std::optional<Scene::TransformComponent>
        {
            const Scene::Entity::Id id = Named(object, block);
            return id != 0 ? ecs.GetTransform(ecs.FindEntity(id)) : std::nullopt;
        };

        switch (block.op)
        {
        case Op::MoveBack:
        case Op::MoveForward:
            if (auto* t = Place(object, block, ecs, true))
            {
                // A rotation of r degrees turns the object's front (-Z) to (-sin r, 0, -cos r)
                const float r = static_cast<float>(t->rotation / kDegrees);
                const float distance = n(0) * object.movementSpeed * (block.op == Op::MoveBack ? -1.0f : 1.0f);
                walk(*t, -std::sin(r) * distance, 0.0f, -std::cos(r) * distance);
                return true;
            }
            break;
        case Op::FlyForward:
            if (auto* t = Place(object, block, ecs, true))
            {
                // A tilt of p as well lifts the front to (-sin r cos p, sin p, -cos r cos p)
                const float r = static_cast<float>(t->rotation / kDegrees);
                const float p = static_cast<float>(t->rotationX / kDegrees);
                const float level = n(0) * object.movementSpeed * std::cos(p);
                walk(*t, -std::sin(r) * level, n(0) * object.movementSpeed * std::sin(p), -std::cos(r) * level);
                return true;
            }
            break;
        case Op::MoveRight:
            if (auto* t = Place(object, block, ecs, true))
            {
                // The object's right is its front turned a quarter: (cos r, 0, -sin r)
                const float r = static_cast<float>(t->rotation / kDegrees);
                const float distance = n(0) * object.movementSpeed;
                walk(*t, std::cos(r) * distance, 0.0f, -std::sin(r) * distance);
                return true;
            }
            break;
        case Op::TurnBy:
            if (auto* t = Place(object, block, ecs, true))
            {
                t->rotation = std::fmod(t->rotation + n(0), 360.0f);
                return true;
            }
            break;
        case Op::GoTo:
            if (auto* t = Place(object, block, ecs, true))
            {
                t->x = n(0);
                t->y = n(1);
                t->z = n(2);
                return true;
            }
            break;
        case Op::MoveBy:
            if (auto* t = Place(object, block, ecs, true))
            {
                walk(*t, n(0) * object.movementSpeed, n(1) * object.movementSpeed, n(2) * object.movementSpeed);
                return true;
            }
            break;
        case Op::GoToObject:
            if (auto* t = Place(object, block, ecs, true))
            {
                if (const auto there = target())
                {
                    t->x = there->x;
                    t->y = there->y;
                    t->z = there->z;
                    return true;
                }
            }
            break;
        case Op::PointTowards:
            if (auto* t = Place(object, block, ecs, true))
            {
                const auto here = entity ? ecs.GetTransform(entity) : std::nullopt;
                const auto there = target();
                if (here && there && (there->x != here->x || there->z != here->z))
                {
                    // The front is (-sin r, -cos r) on the ground: r = atan2(-dx, -dz)
                    t->rotation =
                        static_cast<float>(std::atan2(-(there->x - here->x), -(there->z - here->z)) * kDegrees);
                    return true;
                }
            }
            break;
        case Op::Show:
        case Op::Hide:
        {
            const std::vector<Scene::MeshRendererComponent*> drawn = renderers();
            for (Scene::MeshRendererComponent* r : drawn)
            {
                r->visible = block.op == Op::Show;
            }
            if (auto* e = entity ? ecs.TryGetComponent<Scene::ScreenElementComponent>(entity) : nullptr)
            {
                e->visible = block.op == Op::Show;
                return true;
            }
            return !drawn.empty();
        }
        case Op::SetColor:
        {
            const float red = std::clamp(n(0), 0.0f, 1.0f);
            const float green = std::clamp(n(1), 0.0f, 1.0f);
            const float blue = std::clamp(n(2), 0.0f, 1.0f);
            const std::vector<Scene::MeshRendererComponent*> drawn = renderers();
            for (Scene::MeshRendererComponent* r : drawn)
            {
                r->albedoR = red;
                r->albedoG = green;
                r->albedoB = blue;
            }
            if (auto* e = entity ? ecs.TryGetComponent<Scene::ScreenElementComponent>(entity) : nullptr)
            {
                e->colorR = red;
                e->colorG = green;
                e->colorB = blue;
                return true;
            }
            return !drawn.empty();
        }
        case Op::SetSize:
            if (auto* t = Place(object, block, ecs, true))
            {
                t->scale = std::max(n(0), 0.0f);
                return true;
            }
            break;
        case Op::ChangeSizeBy:
            if (auto* t = Place(object, block, ecs, true))
            {
                t->scale = std::max(t->scale + n(0), 0.0f);
                return true;
            }
            break;
        case Op::Say:
            object.said = block.text;
            object.saidAt = time_;
            LOG_INFO(Logic, "{} says: {}", object.name, block.text);
            return true;
        case Op::Broadcast: broadcasts_.push_back(block.text); break;
        case Op::TellRayHit:
            if (object.rayHit != 0)
            {
                told_.emplace_back(object.rayHit, block.text);
            }
            break;
        case Op::RayAhead:
        case Op::RayAt:
        {
            const bool ahead = block.op == Op::RayAhead;
            Scene::Entity aim;
            if (!ahead)
            {
                aim = ecs.FindEntity(Named(object, block));
            }
            const float spread = ahead ? 0.0f : std::max(n(2), 0.0f);
            std::uniform_real_distribution<float> off(-spread, spread);
            const auto found =
                physics_ && entity && (ahead || aim)
                    ? physics_->CastRay(entity, aim, n(1), std::max(n(0), 0.0f), off(random_), off(random_))
                    : std::nullopt;
            object.rayHit = found ? found->entity : 0;
            object.rayDistance = found ? found->distance : 0.0f;
            break;
        }
        case Op::SetValueOf:
            if (double* named = VariableNamed(object, block, false))
            {
                *named = n(0);
            }
            break;
        case Op::ChangeValueOf:
            if (double* named = VariableNamed(object, block, false))
            {
                *named += n(0);
            }
            break;
        case Op::SetVariable:
            if (double* value = variable())
            {
                *value = n(0);
                return true;
            }
            break;
        case Op::ChangeVariable:
            if (double* value = variable())
            {
                *value += n(0);
                return true;
            }
            break;
        case Op::SetSpeed:
        {
            const float speed = n(0);

            if (!std::isfinite(speed) || speed < 0.0f || speed > 1000.0f)
            {
                Report(object, block, "Speed multiplier must be between 0 and 1000");
            }
            else
            {
                object.movementSpeed = speed;
            }

            break;
        }

        case Op::ApplyForce:
        {
            const glm::vec3 force(n(0), n(1), n(2));

            if (!std::isfinite(force.x) || !std::isfinite(force.y) || !std::isfinite(force.z))
            {
                Report(object, block, "Force values must be finite numbers");
            }
            else if (!(physics_ && entity && physics_->ApplyForce(entity, force, object.stepSeconds)))
            {
                Report(object, block,
                       object.name + " needs a Dynamic Rigid Body to apply force. "
                                     "Add a Rigid Body with Type: Dynamic.");
            }

            break;
        }
        case Op::ResetTimer: timerStart_ = time_; break;
        case Op::Jump:
            if (!(physics_ && entity && physics_->Jump(entity, std::max(n(0), 0.0f))))
            {
                noBody();
            }
            break;
        case Op::PushForward:
            if (auto* t = Place(object, block, ecs, true))
            {
                const float r = static_cast<float>(t->rotation / kDegrees);
                push(glm::vec3(-std::sin(r) * n(0), 0.0f, -std::cos(r) * n(0)));
            }
            break;
        case Op::PushBy: push(glm::vec3(n(0), n(1), n(2))); break;
        case Op::PointInDirection:
            if (auto* t = Place(object, block, ecs, true))
            {
                t->rotation = std::fmod(n(0), 360.0f);
                return true;
            }
            break;
        case Op::SetTilt:
        {
            // A player the camera follows aims where its camera looks (what "tilt of" reads)
            auto* player = entity ? ecs.TryGetComponent<Scene::PlayerControllerComponent>(entity) : nullptr;
            if (player && player->followCamera)
            {
                player->cameraPitch = std::clamp(n(0), -89.0f, 89.0f);
            }
            else if (auto* t = Place(object, block, ecs, true))
            {
                // Straight up or straight down at most, so the front never flips over
                t->rotationX = std::clamp(n(0), -90.0f, 90.0f);
                return true;
            }
            break;
        }
        case Op::SetSetting: settings_[SettingName(block.text)] = n(0); break;
        case Op::SetViewAngle:
        {
            bool found = false;
            ecs.ForEach<Scene::CameraComponent>(
                [&](Scene::Entity, Scene::CameraComponent& camera)
                {
                    if (camera.primary)
                    {
                        camera.fieldOfView = std::clamp(n(0), 10.0f, 120.0f);
                        found = true;
                    }
                });
            if (!found)
            {
                Report(object, block, "There is no Main camera to change. Give a camera Primary in the Inspector.");
            }
            break;
        }
        case Op::SetCameraHeight:
        {
            // The player's, or (for its model or gun) its parent's
            auto* player = entity ? ecs.TryGetComponent<Scene::PlayerControllerComponent>(entity) : nullptr;
            if (const auto* link = entity && !player ? ecs.TryGetComponent<Scene::HierarchyComponent>(entity) : nullptr)
            {
                const Scene::Entity parent = ecs.FindEntity(link->parent);
                player = parent ? ecs.TryGetComponent<Scene::PlayerControllerComponent>(parent) : nullptr;
            }
            if (player)
            {
                player->cameraOffsetY = n(0);
            }
            else
            {
                Report(object, block,
                       object.name + " has no Player Controller, so it has no camera to raise or lower.");
            }
            break;
        }
        case Op::SetSensitivity: settings_["sensitivity"] = std::clamp(n(0), 0.01f, 5.0f); break;
        case Op::SetVolume:
        {
            const std::string bus = SettingName(block.text);
            if (bus == "master volume" || bus == "music volume" || bus == "sfx volume")
            {
                settings_[bus] = std::clamp(n(0), 0.0f, 100.0f);
            }
            else
            {
                Report(object, block, "There is no \"" + block.text + "\" volume: use master, music or sfx.");
            }
            break;
        }
        case Op::SetFullscreen:
        {
            const std::string word = SettingName(block.text);
            const bool on = word == "on" || word == "yes" || word == "true" || word == "1";
            settings_["fullscreen"] = on ? 1.0 : 0.0;
            gameRequests_.push_back({.kind = GameRequest::Kind::Fullscreen, .a = on ? 1.0f : 0.0f});
            break;
        }
        case Op::SetWindowSize:
        {
            const float width = std::clamp(n(0), 320.0f, 7680.0f);
            const float height = std::clamp(n(1), 240.0f, 4320.0f);
            settings_["width"] = width;
            settings_["height"] = height;
            gameRequests_.push_back({.kind = GameRequest::Kind::WindowSize, .a = width, .b = height});
            break;
        }
        case Op::QuitGame: gameRequests_.push_back({.kind = GameRequest::Kind::Quit}); break;
        case Op::GoToScene: sceneRequest_ = block.text; break;
        case Op::PlaySound:
        case Op::PlayFootstep:
        {
            std::string sound = block.text;
            if (block.op == Op::PlayFootstep)
            {
                // The ground's own step sounds; none in the air (or with no body to stand on anything)
                const auto surface = physics_ && entity ? physics_->SurfaceUnder(entity) : std::nullopt;
                sound = surface ? "step_" + (surface->empty() ? std::string("default") : *surface) : std::string();
            }
            if (!sound.empty())
            {
                const auto at = entity ? ecs.GetTransform(entity) : std::nullopt;
                soundRequests_.push_back({.name = sound,
                                          .position = at ? std::optional(glm::vec3(at->x, at->y, at->z)) : std::nullopt,
                                          .object = object.id,
                                          .block = Shown(object, block.id)});
            }
            break;
        }
        case Op::ShowScreenText:
        case Op::ShowScreenPicture:
            if (auto* e = element())
            {
                // A button shown again stays a button, with this on it
                if (block.op != Op::ShowScreenText || e->kind != Scene::ScreenElementKind::Button)
                {
                    e->kind = block.op == Op::ShowScreenText ? Scene::ScreenElementKind::Text
                                                             : Scene::ScreenElementKind::Picture;
                }
                (block.op == Op::ShowScreenText ? e->text : e->picture) = block.text;
                e->visible = true;
            }
            break;
        case Op::LockMouse:
        case Op::FreeMouse: mouseRequest_ = block.op == Op::LockMouse; break;
        case Op::PlayAnimation:
        case Op::PlayAnimationOnce:
            if (entity)
            {
                Systems::AnimationSystem::Play(ecs, entity, block.text, block.op == Op::PlayAnimation);
            }
            break;
        case Op::StopAnimation:
            if (auto* a = entity ? ecs.TryGetComponent<Scene::AnimatorComponent>(entity) : nullptr)
            {
                a->playing = false;
            }
            break;
        case Op::SetAnimationSpeed:
            if (entity)
            {
                if (!ecs.TryGetComponent<Scene::AnimatorComponent>(entity))
                {
                    ecs.AddComponent(entity, Scene::AnimatorComponent{.playing = false});
                }
                ecs.TryGetComponent<Scene::AnimatorComponent>(entity)->speed = n(0);
            }
            break;
        default: break; // hats, control blocks, conditions and values are not actions
        }
        return false;
    }

    bool LogicRunner::Evaluate(ObjectLogic& object, const Block& condition, Scene::Scene& ecs, const Keys& keys)
    {
        if (condition.id == 0)
        {
            return false; // an empty slot
        }
        Stamp(object, condition.id);
        const auto n = [&](std::size_t i) { return Number(object, condition, i, ecs, keys); };
        switch (condition.op)
        {
        case Op::KeyDown:
        {
            const auto binding = Input::ParseBinding(condition.text);
            if (binding && binding->device == Input::InputBinding::Device::Mouse)
            {
                return keys.mouseButton && keys.mouseButton(binding->code);
            }
            const int code = KeyCodeOf(condition);
            return code >= 0 && keys.down && keys.down(code);
        }
        case Op::RayHit:
        {
            if (condition.text.empty() || SameWords(condition.text, "anything"))
            {
                return object.rayHit != 0;
            }
            const Scene::Entity::Id wanted = Named(object, condition);
            return wanted != 0 && object.rayHit == wanted;
        }
        case Op::ValueExists: return VariableNamed(object, condition, true) != nullptr;
        case Op::Equal: return std::abs(n(0) - n(1)) < 1e-6;
        case Op::Less: return n(0) < n(1);
        case Op::Greater: return n(0) > n(1);
        case Op::And:
            return Evaluate(object, SlotOf(condition, 0), ecs, keys) &&
                   Evaluate(object, SlotOf(condition, 1), ecs, keys);
        case Op::Or:
            return Evaluate(object, SlotOf(condition, 0), ecs, keys) ||
                   Evaluate(object, SlotOf(condition, 1), ecs, keys);
        case Op::Not: return !Evaluate(object, SlotOf(condition, 0), ecs, keys);
        case Op::OnGround:
        {
            const Scene::Entity entity = ecs.FindEntity(object.entity);
            const std::optional<bool> ground = physics_ && entity ? physics_->OnGround(entity) : std::nullopt;
            if (!ground)
            {
                Report(object, condition, object.name + " has no Rigid Body, so it cannot stand on anything.");
                return false;
            }
            return *ground;
        }
        case Op::Touching:
        {
            const Scene::Entity entity = ecs.FindEntity(object.entity);
            if (!entity || !ecs.TryGetComponent<Scene::RigidBodyComponent>(entity))
            {
                Report(object, condition, object.name + " has no Rigid Body, so nothing can touch it.");
                return false;
            }
            // Its body is made on the first physics step: until then it touches nothing
            const auto* body = ecs.TryGetComponent<Scene::PhysicsBodyComponent>(entity);
            const Scene::Entity::Id other = Named(object, condition);
            return body && other != 0 &&
                   std::any_of(body->touching.begin(), body->touching.end(),
                               [other](const Scene::Entity& each) { return each.GetId() == other; });
        }
        default: return false;
        }
    }

    std::vector<LogicRunner::Problem> LogicRunner::ValidateVariables(const LogicDesc& logic)
    {
        std::vector<Problem> problems;

        const auto visit = [&](const auto& self, const std::vector<Block>& blocks) -> void
        {
            for (const auto& block : blocks)
            {
                if (block.id != 0 && HasVariableField(block.op) && !FindVariable(logic, block.text))
                {
                    problems.push_back({block.id, "Variable " + block.text + " does not exist on this object"});
                }

                self(self, block.inputs);
                self(self, block.slots);
                self(self, block.body);
                self(self, block.elseBody);
            }
        };

        for (const auto& script : logic.scripts)
        {
            visit(visit, script.blocks);
        }

        return problems;
    }

    void LogicRunner::ReportProblem(std::uint32_t objectId, std::uint32_t blockId, std::string message)
    {
        const auto found = objects_.find(objectId);

        if (found == objects_.end())
        {
            return;
        }

        if (FindBlock(found->second->logic, blockId))
        {
            ReportAt(*found->second, blockId, std::move(message));
        }
    }

    void LogicRunner::Report(ObjectLogic& object, const Block& block, std::string message)
    {
        ReportAt(object, Shown(object, block.id), std::move(message));
    }

    void LogicRunner::ReportAt(ObjectLogic& object, std::uint32_t shownBlock, std::string message)
    {
        const bool known = std::any_of(object.problems.begin(), object.problems.end(), [&](const Problem& problem)
                                       { return problem.block == shownBlock && problem.message == message; });

        if (known)
        {
            return;
        }

        Core::Logger::Get().Write(Core::LogLevel::Error, "Scripting", message, __FILE__, __LINE__,
                                  Core::LogSource::Engine, {object.id, shownBlock, diagnosticSession_});

        object.problems.push_back({.block = shownBlock, .message = std::move(message)});
    }

    std::vector<std::uint32_t> LogicRunner::ActiveBlocks(std::uint32_t objectId) const
    {
        std::vector<std::uint32_t> active;
        const auto found = objects_.find(objectId);
        if (found == objects_.end())
        {
            return active;
        }
        const ObjectLogic& object = *found->second;
        for (const auto& [block, at] : object.lastRun)
        {
            if (time_ - at <= kActiveSeconds)
            {
                active.push_back(block);
            }
        }
        for (const Thread& thread : object.threads)
        {
            if (!thread.frames.empty() && thread.waitingOn != 0 &&
                std::find(active.begin(), active.end(), thread.waitingOn) == active.end())
            {
                active.push_back(thread.waitingOn);
            }
        }
        std::sort(active.begin(), active.end());
        return active;
    }

    std::size_t LogicRunner::RunningScripts(std::uint32_t objectId) const
    {
        const auto found = objects_.find(objectId);
        if (found == objects_.end())
        {
            return 0;
        }
        return static_cast<std::size_t>(std::count_if(found->second->threads.begin(), found->second->threads.end(),
                                                      [](const Thread& thread) { return !thread.frames.empty(); }));
    }

    std::vector<LogicRunner::ScriptState> LogicRunner::Scripts(std::uint32_t objectId) const
    {
        std::vector<ScriptState> states;
        const auto found = objects_.find(objectId);
        if (found == objects_.end())
        {
            return states;
        }
        const ObjectLogic& object = *found->second;
        for (const Thread& thread : object.threads)
        {
            const Block* next = NextBlockOf(thread);
            // On a wait, glide or "wait until" it just ran: the "ready" block is the one after it
            if (next && next->id == thread.lastBlock &&
                (next->op == Op::Wait || next->op == Op::GlideTo || next->op == Op::WaitUntil))
            {
                next = BlockAfterWait(thread);
            }
            const bool running = !thread.frames.empty();
            states.push_back({.script = static_cast<std::size_t>(thread.script - object.logic.scripts.data()),
                              .running = running,
                              .last = thread.lastBlock,
                              .next = ScriptIdOf(thread, next),
                              .before = thread.trail,
                              .justEnded = !running && thread.lastBlock != 0 && thread.lastUpdate == update_});
        }
        return states;
    }

    std::vector<LogicRunner::Problem> LogicRunner::Problems(std::uint32_t objectId) const
    {
        const auto found = objects_.find(objectId);
        return found != objects_.end() ? found->second->problems : std::vector<Problem>{};
    }

    std::string LogicRunner::LastSaid(std::uint32_t objectId) const
    {
        const auto found = objects_.find(objectId);
        return found != objects_.end() ? found->second->said : std::string{};
    }

    std::vector<std::pair<std::string, double>> LogicRunner::Variables(std::uint32_t objectId) const
    {
        const auto found = objects_.find(objectId);
        return found != objects_.end() ? found->second->variables : std::vector<std::pair<std::string, double>>{};
    }

    std::optional<LogicRunner::Write> LogicRunner::LastWrite(std::uint32_t objectId, std::string_view variable) const
    {
        const auto found = objects_.find(objectId);
        if (found == objects_.end())
        {
            return std::nullopt;
        }
        const auto write = found->second->writes.find(variable);
        return write != found->second->writes.end() ? std::optional(write->second) : std::nullopt;
    }

    std::optional<LogicRunner::Write> LogicRunner::LastWrite(std::uint32_t objectId, Property property) const
    {
        const auto found = objects_.find(objectId);
        return found != objects_.end() ? found->second->propertyWrites.at(static_cast<std::size_t>(property))
                                       : std::nullopt;
    }

    void LogicRunner::Record(ObjectLogic& object, const Block& block)
    {
        const Write write{.block = block.id, .serial = ++writes_};
        if (const std::optional<Property> property = PropertyOf(block.op))
        {
            object.propertyWrites.at(static_cast<std::size_t>(*property)) = write;
        }
        else if (WritesVariable(block.op))
        {
            object.writes[block.text] = write;
        }
    }

    // ---------------------------------------------------------------- Debugging (T16)

    bool LogicRunner::BeforeBlock(ObjectLogic& object, Thread& thread, const Block& block)
    {
        switch (debug_.mode)
        {
        case RunMode::Paused: return false;
        case RunMode::StepOnce:
            if (!stepping_)
            {
                stepping_ = &thread; // the first block to run is the step
                stepOverDepth_ = stepOver_ && block.op == Op::Call ? thread.frames.size() : 0;
                stepOver_ = false;
                if (traceBlocks_)
                {
                    LOG_INFO(Logic, "[{}] {}", object.name, Describe(block));
                }
                return true;
            }
            if (stepping_ == &thread)
            {
                if (stepOverDepth_ != 0 && thread.frames.size() > stepOverDepth_)
                {
                    return true; // still inside the My Block being stepped over
                }
                stepOverDepth_ = 0;
                PauseAt(object, thread, &block, PauseReason::Step); // the step's block has run: stop before this one
            }
            return false;        // the other scripts wait while a step runs
        case RunMode::StepFrame: // Update turns it into Running before any block runs
        case RunMode::Running: break;
        }
        const bool resumedHere = thread.skipBreakAt == block.id;
        thread.skipBreakAt = 0;
        // Only the script's own blocks have breakpoints: one on a My Block stops before it runs
        if (!resumedHere && object.inCall == 0 && HasBreakpoint(object.id, block.id) &&
            BreakpointFires(object.id, block.id))
        {
            PauseAt(object, thread, &block, PauseReason::Breakpoint);
            return false;
        }
        if (traceBlocks_)
        {
            LOG_INFO(Logic, "[{}] {}", object.name, Describe(block));
        }
        return true;
    }

    void LogicRunner::PauseAt(const ObjectLogic& object, const Thread& thread, const Block* block, PauseReason reason)
    {
        debug_.mode = RunMode::Paused;
        debug_.reason = reason;
        debug_.pausedObject = object.id;
        debug_.pausedThread = static_cast<int>(&thread - object.threads.data());
        debug_.pausedBlock = ScriptIdOf(thread, block);
        ++debug_.pauses;
        stepping_ = nullptr;
        if (reason == PauseReason::Breakpoint && block)
        {
            LOG_INFO(Logic, "Paused at a breakpoint: '{}' on {}", Describe(*block), object.name);
        }
    }

    std::uint32_t LogicRunner::ScriptIdOf(const Thread& thread, const Block* block)
    {
        if (!block)
        {
            return 0;
        }
        for (auto frame = thread.frames.rbegin(); frame != thread.frames.rend(); ++frame)
        {
            const std::vector<Block>& list = *frame->list;
            if (!list.empty() && block >= &list.front() && block <= &list.back())
            {
                return frame->call != 0 ? frame->call : block->id;
            }
        }
        return block->id;
    }

    const Block* LogicRunner::NextBlockOf(const Thread& thread)
    {
        if (thread.frames.empty())
        {
            return nullptr; // the script has ended
        }
        const Thread::Frame& frame = thread.frames.back();
        if ((thread.glide.block != 0 || thread.waitLeft > 0.0) && frame.index > 0)
        {
            return &(*frame.list)[frame.index - 1]; // still on its wait or glide
        }
        if (frame.index < frame.list->size())
        {
            return &(*frame.list)[frame.index];
        }
        // The end of a C block's inside: the block holding it decides what comes next
        if (thread.frames.size() > 1)
        {
            const Thread::Frame& parent = thread.frames[thread.frames.size() - 2];
            return &(*parent.list)[parent.index];
        }
        return nullptr;
    }

    const Block* LogicRunner::BlockAfterWait(const Thread& thread)
    {
        if (thread.frames.empty())
        {
            return nullptr;
        }
        // A wait or glide has moved its list on already; a "wait until" moves on when its condition is true
        const Thread::Frame& top = thread.frames.back();
        const bool waitUntil = top.index < top.list->size() && (*top.list)[top.index].op == Op::WaitUntil;
        std::size_t index = waitUntil ? top.index + 1 : top.index;
        for (std::size_t level = thread.frames.size(); level-- > 0;)
        {
            const Thread::Frame& frame = thread.frames[level];
            if (index < frame.list->size())
            {
                return &(*frame.list)[index];
            }
            if (level == 0)
            {
                return nullptr; // the end of the script
            }
            // The end of a C block's inside: a loop goes round again, else the block after the loop or If
            const Thread::Frame& parent = thread.frames[level - 1];
            const Block& owner = (*parent.list)[parent.index];
            if (owner.op == Op::Forever || owner.op == Op::RepeatUntil ||
                (owner.op == Op::Repeat && parent.repeatsLeft > 1))
            {
                return &owner;
            }
            index = parent.index + 1;
        }
        return nullptr;
    }

    std::pair<LogicRunner::ObjectLogic*, LogicRunner::Thread*> LogicRunner::PausedThread()
    {
        const auto found = objects_.find(debug_.pausedObject);
        if (found == objects_.end() || debug_.pausedThread < 0 ||
            static_cast<std::size_t>(debug_.pausedThread) >= found->second->threads.size())
        {
            return {nullptr, nullptr};
        }
        Thread& thread = found->second->threads[static_cast<std::size_t>(debug_.pausedThread)];
        if (thread.frames.empty())
        {
            return {nullptr, nullptr}; // it has ended
        }
        return {found->second.get(), &thread};
    }

    void LogicRunner::StepPausedThread(ObjectLogic& object, Thread& thread, Scene::Scene& ecs, const Keys& keys)
    {
        Step(object, thread, ecs, 0.0, keys); // no time passes: the other scripts' waits stand still
        if (debug_.mode == RunMode::StepOnce)
        {
            FinishStep(object, thread, ecs, keys); // it yielded before another block
        }
    }

    void LogicRunner::FinishStep(ObjectLogic& object, Thread& thread, Scene::Scene& ecs, const Keys& keys)
    {
        if (thread.glide.block != 0 || thread.waitLeft > 0.0)
        {
            // One step for a wait or a glide: it finishes at once, and the script stops before its next block
            thread.glide.elapsed = thread.glide.duration;
            thread.waitLeft = 0.0;
            stepping_ = &thread;
            Step(object, thread, ecs, 0.0, keys);
            if (debug_.mode != RunMode::StepOnce)
            {
                return; // stopped before the next block
            }
        }
        // A script that ran its last block stops once more, so the next Step shows it as ended (all blocks done), and the
        // Step after that moves on
        const Block* next = NextBlockOf(thread);
        PauseAt(object, thread, next, next ? PauseReason::Step : PauseReason::StepLast);
    }

    bool LogicRunner::PauseAtNextRunning(const ObjectLogic& object, const Thread& ended)
    {
        const auto running = [](const Thread& each) { return !each.frames.empty(); };
        const auto index = static_cast<std::size_t>(&ended - object.threads.data());
        for (std::size_t i = 1; i < object.threads.size(); ++i)
        {
            const Thread& other = object.threads[(index + i) % object.threads.size()];
            if (running(other))
            {
                PauseAt(object, other, NextBlockOf(other), PauseReason::Step);
                return true;
            }
        }
        auto at = std::next(objects_.find(object.id));
        for (std::size_t i = 1; i < objects_.size(); ++i, ++at)
        {
            if (at == objects_.end())
            {
                at = objects_.begin();
            }
            const auto found = std::find_if(at->second->threads.begin(), at->second->threads.end(), running);
            if (found != at->second->threads.end())
            {
                PauseAt(*at->second, *found, NextBlockOf(*found), PauseReason::Step);
                return true;
            }
        }
        return false;
    }

    bool LogicRunner::FocusScript(std::uint32_t objectId, std::uint32_t blockId)
    {
        const auto found = objects_.find(objectId);
        if (debug_.mode != RunMode::Paused || found == objects_.end())
        {
            return false;
        }
        const std::vector<Thread>& threads = found->second->threads;
        const auto thread = std::find_if(threads.begin(), threads.end(), [blockId](const Thread& each)
                                         { return !each.frames.empty() && FindBlock(each.script->blocks, blockId); });
        if (thread == threads.end())
        {
            return false;
        }
        if (Thread* paused = PausedThread().second; paused != &*thread)
        {
            if (paused)
            {
                paused->skipBreakAt = debug_.pausedBlock; // on Run its breakpoint does not stop it again
            }
            PauseAt(*found->second, *thread, NextBlockOf(*thread), PauseReason::Focus);
        }
        return true;
    }

    void LogicRunner::Pause(std::uint32_t preferObject)
    {
        if (debug_.mode != RunMode::Running)
        {
            return;
        }
        // Stop in a running script so Step has one to run: the preferred object's first, else the first anywhere
        const auto stopIn = [this](const ObjectLogic& object)
        {
            for (const Thread& thread : object.threads)
            {
                if (!thread.frames.empty())
                {
                    PauseAt(object, thread, NextBlockOf(thread), PauseReason::User);
                    return true;
                }
            }
            return false;
        };
        if (const auto found = objects_.find(preferObject); found != objects_.end() && stopIn(*found->second))
        {
            return;
        }
        for (const auto& [id, object] : objects_)
        {
            if (stopIn(*object))
            {
                return;
            }
        }
        // No script is running: stopped in none (Step then runs the game until one runs a block)
        debug_.mode = RunMode::Paused;
        debug_.reason = PauseReason::User;
        debug_.ClearPlace();
        ++debug_.pauses;
    }

    void LogicRunner::Resume()
    {
        if (Thread* thread = PausedThread().second)
        {
            thread->skipBreakAt = debug_.pausedBlock;
        }
        debug_.mode = RunMode::Running;
        debug_.reason = PauseReason::None;
        debug_.ClearPlace();
        stepping_ = nullptr;
    }

    void LogicRunner::StepOnce()
    {
        if (debug_.mode != RunMode::Paused)
        {
            return;
        }
        debug_.mode = RunMode::StepOnce; // the place stays: StepPausedThread runs that script
        endShown_ = debug_.reason != PauseReason::StepLast;
        debug_.reason = PauseReason::None;
        stepping_ = nullptr;
        stepOver_ = false;
    }

    void LogicRunner::StepOver()
    {
        const bool paused = debug_.mode == RunMode::Paused;
        StepOnce();
        stepOver_ = paused;
    }

    void LogicRunner::StepFrame()
    {
        if (debug_.mode != RunMode::Paused)
        {
            return;
        }
        debug_.mode = RunMode::StepFrame; // the place stays: Update lets that script past its breakpoint
        stepping_ = nullptr;
    }

    std::optional<double> LogicRunner::PlayerSeconds(int fixedSteps, double deltaSeconds) const
    {
        switch (debug_.mode)
        {
        case RunMode::Paused:
        case RunMode::StepOnce: return std::nullopt;
        case RunMode::StepFrame: return fixedSteps > 0 ? std::optional(kFrameSeconds) : std::nullopt;
        case RunMode::Running: break;
        }
        return deltaSeconds;
    }

    void LogicRunner::SetBreakpointRule(std::uint32_t objectId, std::uint32_t blockId, BreakpointRule rule)
    {
        breakpointRules_[{objectId, blockId}] = std::move(rule);
    }

    bool LogicRunner::BreakpointFires(std::uint32_t objectId, std::uint32_t blockId) const
    {
        const auto found = breakpointRules_.find({objectId, blockId});
        if (found == breakpointRules_.end())
        {
            return true;
        }
        if (!found->second.enabled)
        {
            return false;
        }
        if (found->second.condition.empty())
        {
            return true;
        }
        const auto condition = ParseBreakpointCondition(found->second.condition);
        if (!condition)
        {
            return true;
        }
        const auto value = VariableOfObject(objectId, condition->variable);
        return !value || condition->Holds(*value);
    }

    void LogicRunner::ToggleBreakpoint(std::uint32_t objectId, std::uint32_t blockId)
    {
        std::set<std::uint32_t>& blocks = debug_.breakpoints[objectId];
        if (blocks.erase(blockId) == 0)
        {
            blocks.insert(blockId);
        }
        if (blocks.empty())
        {
            debug_.breakpoints.erase(objectId);
        }
    }

    bool LogicRunner::HasBreakpoint(std::uint32_t objectId, std::uint32_t blockId) const
    {
        const auto found = debug_.breakpoints.find(objectId);
        return found != debug_.breakpoints.end() && found->second.contains(blockId);
    }

    void LogicRunner::SetBreakpoints(std::uint32_t objectId, std::set<std::uint32_t> blocks)
    {
        if (blocks.empty())
        {
            debug_.breakpoints.erase(objectId);
            return;
        }
        debug_.breakpoints[objectId] = std::move(blocks);
    }

    void LogicRunner::ClearBreakpoints()
    {
        breakpointRules_.clear();
        debug_.breakpoints.clear();
    }

    std::vector<LogicRunner::Speech> LogicRunner::Speeches(double maxAge) const
    {
        std::vector<Speech> speeches;
        for (const auto& [id, object] : objects_)
        {
            if (!object->said.empty() && time_ - object->saidAt <= maxAge)
            {
                speeches.push_back({.objectId = id,
                                    .objectName = object->name,
                                    .entity = object->entity,
                                    .text = object->said,
                                    .age = time_ - object->saidAt});
            }
        }
        std::sort(speeches.begin(), speeches.end(), [](const Speech& a, const Speech& b) { return a.age < b.age; });
        return speeches;
    }

    bool LogicRunner::HasScreenClickScript(std::uint32_t objectId) const
    {
        const auto found = objects_.find(objectId);
        return found != objects_.end() &&
               std::any_of(found->second->logic.scripts.begin(), found->second->logic.scripts.end(),
                           [](const Script& script)
                           { return !script.blocks.empty() && script.blocks.front().op == Op::WhenScreenClicked; });
    }

    std::optional<double> LogicRunner::VariableOf(std::string_view objectName, std::string_view variable) const
    {
        for (const auto& [id, object] : objects_)
        {
            if (object->name != objectName)
            {
                continue;
            }
            for (const auto& [name, value] : object->variables)
            {
                if (name == variable)
                {
                    return value;
                }
            }
        }
        return std::nullopt;
    }

    std::optional<bool> LogicRunner::TakeMouseRequest()
    {
        return std::exchange(mouseRequest_, std::nullopt);
    }

    std::optional<std::string> LogicRunner::TakeSceneRequest()
    {
        return std::exchange(sceneRequest_, std::nullopt);
    }

    std::vector<LogicRunner::SoundRequest> LogicRunner::TakeSoundRequests()
    {
        return std::exchange(soundRequests_, {});
    }

    std::vector<LogicRunner::GameRequest> LogicRunner::TakeGameRequests()
    {
        return std::exchange(gameRequests_, {});
    }

    std::optional<double> LogicRunner::VariableOfObject(std::uint32_t objectId, std::string_view variable) const
    {
        const auto found = objects_.find(objectId);
        if (found == objects_.end())
        {
            return std::nullopt;
        }
        for (const auto& [name, value] : found->second->variables)
        {
            if (name == variable)
            {
                return value;
            }
        }
        return std::nullopt;
    }

    void LogicRunner::ScreenClicked(std::uint32_t objectId)
    {
        clicked_.push_back(objectId);
    }
}
