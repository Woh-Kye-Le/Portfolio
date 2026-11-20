/* !
@file    	    ScriptPreview.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		05/11/2026
@brief
    Declares the script previews: what a "when" block, loop or if on the canvas
    shows in its tooltip. The blocks it runs are walked through as the game
    would run them (variables worked out, loops counted, library blocks opened)
    into steps, played one after another: the object acts them out, or, when
    none of them touches the object, the variables are shown changing with no
    object at all. What only the game can answer ("touching?") can go either
    way, so every way it can go is played in turn, as labelled outcomes.
    Header-only so the tests can check it; BlockPreview.cpp
    draws it (BlockPreviewer::DrawScript).

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "editor/panels/blocks/BlockPreview.hpp"
#include "engine/logic/Library.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Editor::Panels
{
    // ---- How long things take in a preview, in seconds
    constexpr float kScriptTrigger = 1.2f;  // the key going down, Run being pressed
    constexpr float kScriptAction = 1.3f;   // a block acting
    constexpr float kScriptDecision = 1.2f; // an if or loop giving its answer
    constexpr float kScriptChange = 1.4f;   // a variable changing
    constexpr float kScriptHold = 1.5f;     // the end of an outcome, before the next
    constexpr float kScriptLongest = 20.0f; // a longer outcome is played faster, to fit

    // ---- How much of a script is shown
    constexpr int kScriptAllRounds = 6;           // a loop of more rounds shows its first and last and skips the rest
    constexpr int kScriptFirstRounds = 2;         // ...this many first
    constexpr int kScriptForeverRounds = 3;       // "forever" then says it goes on
    constexpr int kScriptGuessRounds = 2;         // "repeat until" something only Play can answer, when it comes true
    constexpr std::size_t kScriptMaxOutcomes = 6; // ways a script can go that are played; it says so if there are more
    constexpr int kScriptMaxDepth = 16;           // library blocks in library blocks
    constexpr std::size_t kScriptMaxSteps = 64;
    constexpr int kScriptMaxBlocks = 5000; // blocks walked through, however many rounds are skipped

    // Where an object named by a block stands: its place is only known in the game
    constexpr std::array<float, 2> kScriptOther{3.0f, -1.0f};

    // Whether a block on the canvas runs other blocks (or waits for them), so its tooltip shows them running. Other
    // blocks do one thing, which their own preview shows.
    inline bool ShowsScript(Engine::Logic::Op op)
    {
        using Engine::Logic::Op;
        const Engine::Logic::BlockInfo& info = Engine::Logic::InfoOf(op);
        return info.shape == Engine::Logic::BlockShape::Hat || info.hasBody || op == Op::Wait || op == Op::WaitUntil ||
               op == Op::Stop;
    }

    // Whether a block changes the object itself, so that the object has to be seen. A script with none of these has
    // nothing for the object to do: its variables are shown instead.
    inline bool ActsOnObject(Engine::Logic::Op op)
    {
        using Engine::Logic::Category;
        using Engine::Logic::Op;
        if (op == Op::SetSpeed || op == Op::SetViewAngle || op == Op::SetCameraHeight)
        {
            return false; // they change how it moves or sees, which does not show by itself
        }
        const Engine::Logic::BlockInfo& info = Engine::Logic::InfoOf(op);
        return info.shape == Engine::Logic::BlockShape::Stack &&
               (info.category == Category::Motion || info.category == Category::Looks ||
                info.category == Category::Physics || info.category == Category::Sound);
    }

    // How the object stands, on the floor: x is +X and z is +Z, from where it started
    struct ScriptActor
    {
        float x = 0.0f;
        float z = 0.0f;
        float facing = 0.0f; // degrees, as "point in direction"
        float tilt = 0.0f;
        float size = 1.0f;
        float alpha = 1.0f;
        std::array<float, 3> tint{};
        float tintAmount = 0.0f;
        std::string clip; // the animation it keeps playing; empty: the one it starts with
        float clipSpeed = 1.0f;

        bool operator==(const ScriptActor&) const = default;
    };

    // A variable as a script has left it; no value: only the game can know it ("set score to pick random")
    struct ScriptVariable
    {
        std::string name;
        std::optional<double> value;
    };

    // One thing a script does
    struct ScriptStep
    {
        const Engine::Logic::Block* block = nullptr; // the block doing it
        float seconds = kScriptAction;
        ScriptActor from;   // how the object stands as it starts...
        ScriptActor to;     // ...and ends
        bool snap = false;  // the object is there at once, halfway ("go to"), rather than going there
        bool lifts = false; // the block's own preview takes the object off the floor (a jump)
        // The block's own preview shows its words, keys and bars, played from phaseFrom to phaseTo. Not for ifs and
        // loops: their own previews stand for blocks they do not have
        bool ownPose = false;
        float phaseFrom = kPreviewStart;
        float phaseTo = kPreviewEnd;
        std::optional<std::array<float, 2>> other; // the other object it goes to or faces, as x / z
        std::string outcome;                       // which of the ways the script can go this is, and why
        std::string note;                          // which round of which loop
        std::string words;                         // what it says about itself, over its own preview's
        std::optional<bool> answer;                // what an if or "repeat until" found
        std::vector<ScriptVariable> variables;     // as this step leaves them
        int changed = -1;                          // which of them it changes, or -1...
        std::optional<double> before;              // ...from this
    };

    struct ScriptPreview
    {
        std::vector<ScriptStep> steps;
        double seconds = kScriptHold; // once round every outcome, with the hold at the end of each
        bool actsOnObject = false;    // false: nothing touches the object, so it is not shown
    };

    namespace ScriptDetail
    {
        // What a walk through a script takes the answer to be, for blocks asking what only the game can answer
        using Answers = std::vector<std::pair<const Engine::Logic::Block*, bool>>;

        // Runs blocks as the game does, only as far as can be known without the game: what is read from the world
        // (timers, keys, touching, random numbers) has no value here
        class Walker
        {
        public:
            Walker(std::span<const Engine::Logic::Variable> variables, const Engine::Logic::Library* library,
                   const Answers& answers)
                : library_(library), answers_(answers)
            {
                for (const Engine::Logic::Variable& variable : variables)
                {
                    values_.push_back({variable.name, static_cast<double>(variable.value)});
                }
            }

            // Runs blocks without showing them: what comes before a block sets its variables, but it is that block
            // the preview is about, so the object starts where it is
            void Prepare(std::span<const Engine::Logic::Block> blocks)
            {
                ++quiet_;
                preparing_ = true;
                Run(blocks, 0);
                preparing_ = false;
                --quiet_;
                actor_ = ScriptActor{};
                done_ = false;
                cutShort_ = false;
                walked_ = 0;
            }

            void Trigger(const Engine::Logic::Block& hat)
            {
                ScriptStep step{.block = &hat, .seconds = kScriptTrigger, .ownPose = true};
                step.phaseFrom = 0.0f;
                step.phaseTo = 0.4f; // up to where its own preview goes on to a hop
                Emit(step, actor_);
            }

            void Run(std::span<const Engine::Logic::Block> blocks, int depth)
            {
                for (const Engine::Logic::Block& block : blocks)
                {
                    if (done_)
                    {
                        return;
                    }
                    RunBlock(block, depth);
                }
            }

            // The blocks that asked what only the game can answer, and had no answer given: they took it to be true
            const std::vector<const Engine::Logic::Block*>& Unanswered() const { return unanswered_; }

            // Runs the block the mouse is on
            void RunHovered(const Engine::Logic::Block& block)
            {
                hovered_ = &block;
                RunBlock(block, 0);
            }

            void RunBlock(const Engine::Logic::Block& block, int depth)
            {
                if (++walked_ > kScriptMaxBlocks)
                {
                    done_ = true;
                    cutShort_ = true;
                    return;
                }
                switch (block.op)
                {
                case Op::Repeat: RunRepeat(block, depth); break;
                case Op::RepeatUntil: RunRepeatUntil(block, depth); break;
                case Op::Forever: RunForever(block, depth); break;
                case Op::If:
                case Op::IfElse: RunIf(block, depth); break;
                case Op::Wait: RunWait(block); break;
                case Op::WaitUntil: RunWaitUntil(block); break;
                case Op::Stop:
                {
                    ScriptStep step{.block = &block, .seconds = kScriptDecision, .ownPose = true};
                    step.phaseFrom = 0.45f; // its own preview's "stopped"
                    step.phaseTo = 0.6f;
                    Emit(step, actor_);
                    done_ = true;
                    break;
                }
                case Op::Call: RunCall(block, depth); break;
                case Op::SetVariable:
                case Op::ChangeVariable: RunVariable(block); break;
                default: Act(block); break;
                }
            }

            // Says something about a block that does not act: it has no blocks inside, it was cut short
            void Say(const Engine::Logic::Block& block, std::string words)
            {
                ScriptStep step{.block = &block, .seconds = kScriptDecision};
                step.words = std::move(words);
                Emit(step, actor_);
            }

            ScriptPreview Finish(const Engine::Logic::Block& first)
            {
                if (cutShort_)
                {
                    if (steps_.size() >= kScriptMaxSteps)
                    {
                        steps_.pop_back(); // room to say so
                    }
                    Say(first, "and so on");
                }
                // Every step lists every variable, those it has not come to yet as they start
                for (ScriptStep& step : steps_)
                {
                    for (std::size_t i = step.variables.size(); i < shown_.size(); ++i)
                    {
                        step.variables.push_back({shown_[i].first, shown_[i].second});
                    }
                }

                ScriptPreview script;
                script.actsOnObject = acts_;
                script.steps = std::move(steps_);
                double seconds = 0.0;
                for (const ScriptStep& step : script.steps)
                {
                    seconds += step.seconds;
                }
                const float pace = seconds > kScriptLongest ? static_cast<float>(kScriptLongest / seconds) : 1.0f;
                for (ScriptStep& step : script.steps)
                {
                    step.seconds *= pace;
                }
                script.seconds = seconds * pace + kScriptHold;
                return script;
            }

        private:
            using Op = Engine::Logic::Op;
            using Number = std::optional<double>;

            std::optional<double>* Find(const std::string& name)
            {
                for (auto& [each, value] : values_)
                {
                    if (each == name)
                    {
                        return &value;
                    }
                }
                return nullptr;
            }

            void Set(const std::string& name, Number value)
            {
                if (std::find(written_.begin(), written_.end(), name) == written_.end())
                {
                    written_.push_back(name);
                }
                if (Number* found = Find(name))
                {
                    *found = value;
                }
                else
                {
                    values_.push_back({name, value});
                }
            }

            // The answer this walk takes for a block only the game can answer. Each block has one answer for the
            // whole walk, so a loop's rounds agree.
            bool Assumed(const Engine::Logic::Block& block)
            {
                const auto given = std::find_if(answers_.begin(), answers_.end(),
                                                [&](const auto& each) { return each.first == &block; });
                if (given != answers_.end())
                {
                    return given->second;
                }
                if (!preparing_ && std::find(unanswered_.begin(), unanswered_.end(), &block) == unanswered_.end())
                {
                    unanswered_.push_back(&block);
                }
                return true;
            }

            // ---- Values and conditions, as LogicRunner works them out

            Number NumberOf(const Engine::Logic::Block& block, std::size_t i)
            {
                return IsComputed(block, i) ? ValueOf(block.inputs[i]) : Number(block.numbers[i]);
            }

            // A number field the game can only fill in itself is acted out with the palette's default, as in the
            // blocks' own previews
            float Acted(const Engine::Logic::Block& block, std::size_t i)
            {
                return static_cast<float>(NumberOf(block, i).value_or(Engine::Logic::InfoOf(block.op).numbers[i]));
            }

            Number ValueOf(const Engine::Logic::Block& value)
            {
                switch (value.op)
                {
                case Op::VariableValue:
                {
                    // What a variable starts at says nothing about a condition: other scripts, other objects and
                    // earlier runs of this one change it. Only a value this walk set can be relied on.
                    if (asking_ && std::find(written_.begin(), written_.end(), value.text) == written_.end())
                    {
                        return std::nullopt;
                    }
                    const Number* found = Find(value.text);
                    return found ? *found : std::nullopt;
                }
                case Op::Round:
                case Op::Abs:
                {
                    const Number a = NumberOf(value, 0);
                    if (!a)
                    {
                        return std::nullopt;
                    }
                    return value.op == Op::Round ? std::round(*a) : std::abs(*a);
                }
                case Op::Add:
                case Op::Subtract:
                case Op::Multiply:
                case Op::Divide:
                case Op::Mod:
                {
                    const Number a = NumberOf(value, 0);
                    const Number b = NumberOf(value, 1);
                    if (!a || !b)
                    {
                        return std::nullopt;
                    }
                    switch (value.op)
                    {
                    case Op::Add: return *a + *b;
                    case Op::Subtract: return *a - *b;
                    case Op::Multiply: return *a * *b;
                    case Op::Divide: return *b == 0.0 ? 0.0 : *a / *b;
                    default: return *b == 0.0 ? 0.0 : *a - *b * std::floor(*a / *b);
                    }
                }
                default: return std::nullopt;
                }
            }

            std::optional<bool> Answer(const Engine::Logic::Block& block, std::size_t slot)
            {
                if (slot >= block.slots.size())
                {
                    return std::nullopt;
                }
                const bool outer = asking_;
                asking_ = true;
                const std::optional<bool> answer = ConditionOf(block.slots[slot]);
                asking_ = outer;
                return answer;
            }

            // Whether a condition holds; nullopt: only the game can say
            std::optional<bool> ConditionOf(const Engine::Logic::Block& condition)
            {
                if (condition.id == 0)
                {
                    return std::nullopt; // an empty slot
                }
                switch (condition.op)
                {
                case Op::Equal:
                case Op::Less:
                case Op::Greater:
                {
                    const Number a = NumberOf(condition, 0);
                    const Number b = NumberOf(condition, 1);
                    if (!a || !b)
                    {
                        return std::nullopt;
                    }
                    return condition.op == Op::Equal  ? std::abs(*a - *b) < 1e-6
                           : condition.op == Op::Less ? *a < *b
                                                      : *a > *b;
                }
                case Op::Not:
                {
                    const std::optional<bool> a = Answer(condition, 0);
                    return a ? std::optional<bool>(!*a) : std::nullopt;
                }
                case Op::And:
                case Op::Or:
                {
                    // One side can settle it without the other: false and anything is false
                    const bool settles = condition.op == Op::Or;
                    const std::optional<bool> a = Answer(condition, 0);
                    const std::optional<bool> b = Answer(condition, 1);
                    if (a == settles || b == settles)
                    {
                        return settles;
                    }
                    return a && b ? std::optional<bool>(!settles) : std::nullopt;
                }
                default: return std::nullopt;
                }
            }

            // ---- The steps

            // Adds a step ending as things stand now, unless it is being skipped
            void Emit(ScriptStep& step, const ScriptActor& from)
            {
                if (quiet_ > 0)
                {
                    return;
                }
                if (steps_.size() >= kScriptMaxSteps)
                {
                    done_ = true;
                    cutShort_ = true;
                    return;
                }
                acts_ = acts_ || ActsOnObject(step.block->op);
                step.from = from;
                step.to = actor_;
                for (const std::string& note : notes_)
                {
                    if (!note.empty())
                    {
                        step.note += (step.note.empty() ? "" : " \xC2\xB7 ") + note;
                    }
                }
                for (const auto& [name, start] : shown_)
                {
                    const Number* now = Find(name);
                    step.variables.push_back({name, now ? *now : start});
                }
                steps_.push_back(std::move(step));
            }

            void BeginSkip()
            {
                if (quiet_++ == 0)
                {
                    skippedFrom_ = actor_;
                }
            }

            // The rounds skipped go by as one step, so everything they changed changes smoothly
            void EndSkip(const Engine::Logic::Block& block, std::string words)
            {
                notes_.back().clear(); // its words say which rounds it is, not the last of them
                if (--quiet_ == 0)
                {
                    ScriptStep step{.block = &block, .seconds = kScriptDecision};
                    step.words = std::move(words);
                    Emit(step, skippedFrom_);
                }
            }

            void Decide(const Engine::Logic::Block& block, bool answer, std::string words)
            {
                ScriptStep step{.block = &block, .seconds = kScriptDecision};
                step.answer = answer;
                step.words = std::move(words);
                Emit(step, actor_);
            }

            void RunRepeat(const Engine::Logic::Block& block, int depth)
            {
                const double times = std::round(Acted(block, 0));
                if (block.body.empty())
                {
                    Say(block, "nothing inside yet");
                    return;
                }
                if (times <= 0.0)
                {
                    Say(block, "runs 0 times");
                    return;
                }
                const int rounds = static_cast<int>(std::min(times, 1000000.0));
                bool skipping = false;
                notes_.emplace_back();
                for (int round = 1; round <= rounds && !done_; ++round)
                {
                    const bool skip = rounds > kScriptAllRounds && round > kScriptFirstRounds && round < rounds;
                    if (skip && !skipping)
                    {
                        BeginSkip();
                        skipping = true;
                    }
                    if (!skip && skipping)
                    {
                        EndSkip(block, std::format("rounds {} to {}", kScriptFirstRounds + 1, rounds - 1));
                        skipping = false;
                    }
                    notes_.back() = std::format("round {} of {}", round, rounds);
                    Run(block.body, depth);
                }
                if (skipping)
                {
                    EndSkip(block, "more rounds");
                }
                notes_.pop_back();
            }

            void RunRepeatUntil(const Engine::Logic::Block& block, int depth)
            {
                bool skipping = false;
                bool never = false;
                notes_.emplace_back();
                std::optional<bool> answer;
                for (int round = 1; !done_; ++round)
                {
                    answer = Answer(block, 0);
                    const bool known = answer.has_value();
                    if (!known && !block.body.empty())
                    {
                        // Only the game can say: it comes true after a few rounds, or it never does
                        const bool comes = Assumed(block);
                        answer = comes && round > kScriptGuessRounds;
                        never = !comes && round > kScriptForeverRounds;
                    }
                    if (never || answer.value_or(false) || block.body.empty())
                    {
                        break;
                    }
                    if (known && round > kScriptFirstRounds && !skipping)
                    {
                        BeginSkip();
                        skipping = true;
                    }
                    notes_.back() = std::format("round {}", round);
                    Run(block.body, depth);
                }
                if (skipping)
                {
                    EndSkip(block, "more rounds");
                }
                notes_.pop_back();
                if (never)
                {
                    Say(block, "never comes true: it goes on");
                    done_ = true;
                }
                else if (block.body.empty() && !answer.value_or(false))
                {
                    Say(block, "nothing inside yet");
                }
                else if (!done_)
                {
                    Decide(block, true, "true: done");
                }
            }

            void RunForever(const Engine::Logic::Block& block, int depth)
            {
                if (block.body.empty())
                {
                    Say(block, "nothing inside yet");
                    done_ = true;
                    return;
                }
                notes_.emplace_back();
                for (int round = 1; round <= kScriptForeverRounds && !done_; ++round)
                {
                    notes_.back() = std::format("round {}", round);
                    Run(block.body, depth);
                }
                notes_.pop_back();
                if (!done_)
                {
                    Say(block, "and so on, forever");
                    done_ = true; // nothing under a "forever" ever runs
                }
            }

            void RunIf(const Engine::Logic::Block& block, int depth)
            {
                // The if the mouse is on is there to show what each answer does, so it always shows both
                const std::optional<bool> answer = &block == hovered_ ? std::nullopt : Answer(block, 0);
                if (answer ? *answer : Assumed(block))
                {
                    Decide(block, true, "true: runs the inside");
                    Run(block.body, depth);
                }
                else
                {
                    Decide(block, false, block.op == Op::IfElse ? "false: runs else" : "false: skips it");
                    Run(block.elseBody, depth);
                }
            }

            void RunWait(const Engine::Logic::Block& block)
            {
                ScriptStep step{.block = &block, .ownPose = true};
                step.seconds = std::clamp(Acted(block, 0), 1.0f, 2.5f);
                Emit(step, actor_);
            }

            void RunWaitUntil(const Engine::Logic::Block& block)
            {
                ScriptStep step{.block = &block, .ownPose = true};
                step.phaseFrom = 0.2f; // its own preview's "waiting..."
                step.phaseTo = 0.5f;
                const std::optional<bool> answer = Answer(block, 0);
                if (!(answer ? *answer : Assumed(block)))
                {
                    step.words = answer ? "waits here: nothing changes it" : "never comes true: it waits";
                    Emit(step, actor_);
                    done_ = true;
                    return;
                }
                Emit(step, actor_);
                Decide(block, true, "true: goes on");
            }

            void RunCall(const Engine::Logic::Block& block, int depth)
            {
                const Engine::Logic::LibraryBlock* saved = library_ ? library_->Find(block.text) : nullptr;
                if (!saved || depth >= kScriptMaxDepth)
                {
                    Say(block, saved ? "runs itself again" : "no such block in My Blocks");
                    return;
                }
                for (std::size_t i = 0; i < saved->inputs.size(); ++i)
                {
                    Set(saved->inputs[i], i < block.numbers.size() ? NumberOf(block, i) : Number(0.0));
                }
                Run(saved->blocks, depth + 1);
            }

            void RunVariable(const Engine::Logic::Block& block)
            {
                const Number* found = Find(block.text);
                const Number before = found ? *found : Number(0.0);
                const auto listed = std::find_if(shown_.begin(), shown_.end(),
                                                 [&](const auto& each) { return each.first == block.text; });
                const auto index = static_cast<int>(listed - shown_.begin());
                if (quiet_ == 0 && listed == shown_.end())
                {
                    shown_.push_back({block.text, before});
                }

                const Number given = NumberOf(block, 0);
                Number after = given;
                if (block.op == Op::ChangeVariable)
                {
                    after = given && before ? Number(*before + *given) : std::nullopt;
                }
                Set(block.text, after);

                ScriptStep step{.block = &block, .seconds = kScriptChange};
                step.changed = index;
                step.before = before;
                Emit(step, actor_);
            }

            // Everything else: the object does what the block says, or, if it does not touch the object, the block
            // is named while it runs
            void Act(const Engine::Logic::Block& block)
            {
                const ScriptActor from = actor_;
                ScriptStep step{.block = &block,
                                .ownPose = ActsOnObject(block.op) || StageOf(block.op) == PreviewStage::Screen};
                const auto n = [&](std::size_t i) { return Acted(block, i); };
                const float turned = actor_.facing * std::numbers::pi_v<float> / 180.0f;
                // The object's front and right, as LogicRunner turns them
                const auto walk = [&](float forward, float right)
                {
                    actor_.x += -std::sin(turned) * forward + std::cos(turned) * right;
                    actor_.z += -std::cos(turned) * forward - std::sin(turned) * right;
                };
                const auto towards = [&](const std::array<float, 2>& at)
                { return std::atan2(actor_.x - at[0], actor_.z - at[1]) * 180.0f / std::numbers::pi_v<float>; };
                switch (block.op)
                {
                case Op::MoveForward:
                case Op::FlyForward: walk(n(0) * speed_, 0.0f); break;
                case Op::MoveBack: walk(-n(0) * speed_, 0.0f); break;
                case Op::MoveRight: walk(0.0f, n(0) * speed_); break;
                case Op::MoveBy:
                    actor_.x += n(0);
                    actor_.z += n(2);
                    break;
                // ponytail: where the object starts in the game is not used, so a place reads as from where it stands
                case Op::GoTo:
                    actor_.x = n(0);
                    actor_.z = n(2);
                    step.snap = true;
                    break;
                case Op::GlideTo:
                    actor_.x = n(1);
                    actor_.z = n(3);
                    step.seconds = std::clamp(n(0), kScriptAction, 2.5f);
                    break;
                case Op::GoToObject:
                    actor_.x = kScriptOther[0];
                    actor_.z = kScriptOther[1];
                    step.snap = true;
                    step.other = kScriptOther;
                    break;
                case Op::PointTowards:
                    actor_.facing = towards(kScriptOther);
                    step.other = kScriptOther;
                    break;
                case Op::TurnBy: actor_.facing += n(0); break;
                case Op::PointInDirection: actor_.facing = n(0); break;
                case Op::SetTilt: actor_.tilt = std::clamp(n(0), -90.0f, 90.0f); break;
                case Op::Show: actor_.alpha = 1.0f; break;
                case Op::Hide: actor_.alpha = 0.15f; break; // faint, not gone: it is still there
                case Op::SetColor:
                    actor_.tint = {std::clamp(n(0), 0.0f, 1.0f), std::clamp(n(1), 0.0f, 1.0f),
                                   std::clamp(n(2), 0.0f, 1.0f)};
                    actor_.tintAmount = 0.75f;
                    break;
                case Op::SetSize: actor_.size = std::max(0.0f, n(0)); break;
                case Op::ChangeSizeBy: actor_.size = std::max(0.0f, actor_.size + n(0)); break;
                case Op::SetSpeed: speed_ = n(0); break;
                case Op::PushForward: walk(n(0) * 0.4f, 0.0f); break;
                case Op::PushBy:
                    actor_.x += n(0) * 0.4f;
                    actor_.z += n(2) * 0.4f;
                    step.lifts = true;
                    break;
                case Op::ApplyForce:
                    actor_.x += n(0) * 0.1f;
                    actor_.z += n(2) * 0.1f;
                    step.lifts = true;
                    break;
                // Lifted only by an object with no such clip to play, which bobs instead
                case Op::PlayAnimation:
                    actor_.clip = block.text;
                    step.lifts = true;
                    break;
                case Op::PlayAnimationOnce:
                    step.seconds = kScriptAction * 2.0f;
                    step.lifts = true;
                    break;
                case Op::StopAnimation:
                    actor_.clip.clear();
                    step.lifts = true;
                    break;
                case Op::SetAnimationSpeed:
                    actor_.clipSpeed = n(0);
                    step.lifts = true;
                    break;
                case Op::Jump:
                case Op::PlayFootstep: step.lifts = true; break;
                default: break;
                }
                Emit(step, from);
            }

            const Engine::Logic::Library* library_;
            const Answers& answers_;
            std::vector<const Engine::Logic::Block*> unanswered_;
            const Engine::Logic::Block* hovered_ = nullptr;
            std::vector<std::string> written_;                   // the variables this walk has set
            std::vector<std::pair<std::string, Number>> values_; // every variable, as things stand
            std::vector<std::pair<std::string, Number>> shown_;  // those the script changes, as they start
            std::vector<std::string> notes_;                     // the rounds of the loops it is in
            std::vector<ScriptStep> steps_;
            ScriptActor actor_;
            ScriptActor skippedFrom_;
            float speed_ = 1.0f;
            int quiet_ = 0; // above 0: going through blocks without showing them
            int walked_ = 0;
            bool preparing_ = false; // what comes before the block: not part of what can go either way
            bool asking_ = false;    // working out a condition, not a number
            bool acts_ = false;
            bool done_ = false; // it stopped, or will never get any further
            bool cutShort_ = false;
        };

        // What decides an outcome, in words: "touching "Enemy" = false"
        inline std::string WhyOf(const Engine::Logic::Block& asking, bool answer)
        {
            const bool hasCondition = !asking.slots.empty() && asking.slots.front().id != 0;
            const std::string condition =
                hasCondition ? Engine::Logic::Describe(asking.slots.front()) : std::string("no condition yet");
            return condition + (answer ? " = true" : " = false");
        }
    }

    /*!
    \brief   Walks through what a block on the canvas runs, every way it can go.
    \param   block A block ShowsScript is true of.
    \param   before The blocks above it in its stack: they run first, so they set the variables it finds.
    \param   after The blocks under it. A "when" block runs them; others are about themselves and leave them out.
    \param   variables The object's variables, with what they start at.
    \param   library The game's library blocks, to open those it runs (nullptr: none).
    \return  Its steps, outcome after outcome. The blocks they point to are block's, after's and library's: they must
             outlive it.
    */
    inline ScriptPreview BuildScriptPreview(const Engine::Logic::Block& block,
                                            std::span<const Engine::Logic::Block> before,
                                            std::span<const Engine::Logic::Block> after,
                                            std::span<const Engine::Logic::Variable> variables,
                                            const Engine::Logic::Library* library)
    {
        using ScriptDetail::Answers;
        const auto walk = [&](const Answers& answers, std::vector<const Engine::Logic::Block*>& unanswered)
        {
            ScriptDetail::Walker walker(variables, library, answers);
            walker.Prepare(before);
            if (Engine::Logic::InfoOf(block.op).shape == Engine::Logic::BlockShape::Hat)
            {
                walker.Trigger(block);
                if (after.empty())
                {
                    walker.Say(block, "no blocks under it yet");
                }
                walker.Run(after, 0);
            }
            else
            {
                walker.RunHovered(block);
            }
            unanswered = walker.Unanswered();
            return walker.Finish(block);
        };

        // Each walk takes what it was not told to be true, and leaves the opposite of each of those for a later walk:
        // every path through the script, once, the likely one first
        std::vector<ScriptPreview> outcomes;
        std::vector<std::string> reasons;
        std::vector<Answers> waiting{Answers{}};
        const auto sameSteps = [](const ScriptPreview& a, const ScriptPreview& b)
        {
            return std::equal(a.steps.begin(), a.steps.end(), b.steps.begin(), b.steps.end(),
                              [](const ScriptStep& x, const ScriptStep& y)
                              { return x.block == y.block && x.answer == y.answer && x.words == y.words; });
        };
        constexpr std::size_t kMostWalks = 32; // some of them come out the same as another
        std::size_t next = 0;
        for (; next < waiting.size() && next < kMostWalks && outcomes.size() < kScriptMaxOutcomes; ++next)
        {
            const Answers answers = waiting[next];
            std::vector<const Engine::Logic::Block*> unanswered;
            ScriptPreview walked = walk(answers, unanswered);
            const bool seen = std::any_of(outcomes.begin(), outcomes.end(),
                                          [&](const ScriptPreview& each) { return sameSteps(each, walked); });
            Answers taken = answers;
            for (const Engine::Logic::Block* asking : unanswered)
            {
                Answers other = taken;
                other.push_back({asking, false});
                waiting.push_back(std::move(other));
                taken.push_back({asking, true});
            }
            if (seen)
            {
                continue; // that answer made no difference
            }
            outcomes.push_back(std::move(walked));
            reasons.push_back(!answers.empty()     ? ScriptDetail::WhyOf(*answers.back().first, answers.back().second)
                              : unanswered.empty() ? std::string()
                                                   : ScriptDetail::WhyOf(*unanswered.front(), true));
        }

        // One after another, each held at its end
        const bool more = next < waiting.size();
        ScriptPreview script;
        script.seconds = 0.0;
        for (std::size_t i = 0; i < outcomes.size(); ++i)
        {
            ScriptPreview& outcome = outcomes[i];
            script.actsOnObject = script.actsOnObject || outcome.actsOnObject;
            script.seconds += outcome.seconds;
            if (outcomes.size() > 1)
            {
                const std::string label =
                    std::format("Outcome {} of {}{}: {}", i + 1, outcomes.size(), more ? "+" : "", reasons[i]);
                for (ScriptStep& step : outcome.steps)
                {
                    step.outcome = label;
                }
            }
            if (i + 1 < outcomes.size() && !outcome.steps.empty())
            {
                ScriptStep hold = outcome.steps.back();
                hold.seconds = kScriptHold;
                hold.from = hold.to;
                hold.snap = false;
                hold.lifts = false;
                hold.ownPose = false;
                hold.changed = -1;
                outcome.steps.push_back(std::move(hold));
            }
            script.steps.insert(script.steps.end(), std::make_move_iterator(outcome.steps.begin()),
                                std::make_move_iterator(outcome.steps.end()));
        }
        return script;
    }

    // A script's preview at one moment
    struct ScriptFrame
    {
        const Engine::Logic::Block* block = nullptr; // the block running; nullptr: the script has no steps
        ScriptActor actor;
        float lift = 0.0f;   // how far off the floor it is
        PreviewPose pose;    // its words, keys and bars (its x and y are not used: actor and lift say where it is)
        bool screen = false; // on the game's screen: the block is seen there rather than on the floor
        std::optional<std::array<float, 2>> other;
        std::string outcome;                // which way the script is going, when it can go more than one
        std::vector<std::string> variables; // "score = 3"
        int lit = -1;                       // which of them has just changed
    };

    /*!
    \brief   What a script's preview shows some time after it started.
    \param   script The script.
    \param   seconds How long since it started; from its end on, how it ended.
    */
    inline ScriptFrame FrameAt(const ScriptPreview& script, double seconds)
    {
        ScriptFrame frame;
        if (script.steps.empty())
        {
            return frame;
        }
        const ScriptStep* step = &script.steps.back();
        float through = 1.0f;
        double start = 0.0;
        for (const ScriptStep& each : script.steps)
        {
            if (seconds < start + each.seconds)
            {
                step = &each;
                through = static_cast<float>((seconds - start) / each.seconds);
                break;
            }
            start += each.seconds;
        }

        frame.block = step->block;
        frame.screen = StageOf(step->block->op) == PreviewStage::Screen;
        frame.other = step->other;
        frame.outcome = step->outcome;
        const float eased = through * through * (3.0f - 2.0f * through);
        const float k = step->snap ? (through >= 0.5f ? 1.0f : 0.0f) : eased;
        const auto mix = [k](float from, float to) { return from + (to - from) * k; };
        frame.actor = ScriptActor{.x = mix(step->from.x, step->to.x),
                                  .z = mix(step->from.z, step->to.z),
                                  .facing = mix(step->from.facing, step->to.facing),
                                  .tilt = mix(step->from.tilt, step->to.tilt),
                                  .size = mix(step->from.size, step->to.size),
                                  .alpha = mix(step->from.alpha, step->to.alpha),
                                  .tint = step->to.tint,
                                  .tintAmount = mix(step->from.tintAmount, step->to.tintAmount),
                                  .clip = step->to.clip,
                                  .clipSpeed = step->to.clipSpeed};

        if (step->ownPose)
        {
            frame.pose = PoseAt(*step->block, step->phaseFrom + (step->phaseTo - step->phaseFrom) * through);
            frame.lift = step->lifts ? frame.pose.y : 0.0f;
        }
        // The clip it keeps playing, through whatever it does next, unless a block is playing one once
        if (frame.pose.clipFraction < 0.0f)
        {
            frame.pose.clip = step->to.clip;
            frame.pose.clipTime = static_cast<float>(seconds) * step->to.clipSpeed;
        }
        if (!step->words.empty())
        {
            frame.pose.readout = step->words;
        }
        if (!step->note.empty())
        {
            frame.pose.readout =
                frame.pose.readout.empty() ? step->note : step->note + " \xC2\xB7 " + frame.pose.readout;
        }
        frame.pose.answer = step->answer ? step->answer : frame.pose.answer;

        // A variable changes partway through its step, and stays lit to the end of it
        constexpr float kChanges = 0.4f;
        const bool changing = step->changed >= 0;
        frame.lit = changing && through >= kChanges ? step->changed : -1;
        for (std::size_t i = 0; i < step->variables.size(); ++i)
        {
            const bool old = changing && through < kChanges && static_cast<int>(i) == step->changed;
            const std::optional<double> value = old ? step->before : step->variables[i].value;
            frame.variables.push_back(step->variables[i].name + " = " + (value ? PreviewText(*value) : "?"));
        }
        return frame;
    }
}
