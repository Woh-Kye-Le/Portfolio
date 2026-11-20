/* !
@file    	    BlockPreview.hpp
@author  	    j.cai@digipen.edu [100%]
@date    		02/11/2026
@brief
    Declares the block previews: the little 3D animation in a block's tooltip
    that shows what the block does ("jump 2" jumps 2 high, "3 + 4" works out
    7). The object itself (Atti, a yellow cube, in the palette) acts out what
    an object does; sums and conditions are worked out on a stage of their
    own, and what goes on the game's screen on a little screen. The poses are
    worked out here, header-only so the tests can check them; BlockPreview.cpp
    draws them (BlockPreviewer). Hats and loops hop here because a palette
    block has nothing under it to run; on the canvas ScriptPreview.hpp shows
    the blocks they really run.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/logic/Blocks.hpp"
#include "engine/scene/Hierarchy.hpp"
#include "engine/scene/SceneDescription.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>

namespace Editor::Panels
{
    struct ScriptPreview; // ScriptPreview.hpp

    // How a block's preview is seen: from above (moving and turning on the floor), from the side (jumping, looks,
    // health, loops, events, sensing), as the game's screen (texts, bars, the pointer, scenes) or as a sum worked
    // out (math, comparisons, variables); None: no preview, only its words (a saved block, which lists its own)
    enum class PreviewStage
    {
        None,
        Top,
        Side,
        Screen,
        Value
    };

    // What a preview shows at one moment. Units are the game's. From above, x is +X (right) and y is +Z (down the
    // screen: -Z, where an object faces at direction 0, is up); from the side, x is forward and y is height.
    struct PreviewPose
    {
        // ---- Atti
        float x = 0.0f;
        float y = 0.0f;
        float facing = 0.0f; // from above: its direction in degrees, as "point in direction" (0 up, 90 left)
        float size = 1.0f;
        float alpha = 1.0f;
        std::array<float, 3> tint{}; // the colour it is tinted towards...
        float tintAmount = 0.0f;     // ...and how far (0: its own colours)
        float health = -1.0f;        // its health bar, 0 to 1; below 0: no bar
        float healthAlpha = 1.0f;
        bool speaking = false;
        std::optional<float> tilt;  // from the side: an arrow out of its front, tipped up this many degrees
        std::string mark;           // a word over it: an animation's or a sound's name, or who it stands for
        std::string clip;           // the animation its model plays; empty: the one it starts with...
        float clipTime = 0.0f;      // ...this many seconds in, going round...
        float clipFraction = -1.0f; // ...or played once: this far through it, 0 to 1
        bool sound = false;         // sound waves coming out of it

        // ---- Around it
        std::optional<std::array<float, 2>> target; // from above: the place it goes to or faces, as x / y
        std::optional<float> other;                 // from the side: the object the block names, at this x
        std::optional<float> envelope;              // from the side: a message flying, at this x
        std::string key;                            // a key on the keyboard (or "mouse") in the corner...
        bool keyDown = false;                       // ...pressed
        bool runPressed = false;                    // the Run button in the corner, pressed
        float progress = -1.0f;                     // a bar filling up, 0 to 1 (waiting); below 0: none
        int skyBand = -1;                           // the sky behind, with one band changing: 0 top, 1 horizon,
        std::array<float, 3> sky{};                 // 2 ground, 3 a picture; below 0: no sky. Towards this colour...
        float skyAmount = 0.0f;                     // ...this far
        float haze = -1.0f;                         // fog over the far hills, 0 to 1; below 0: no hills

        // ---- The game's screen (Screen stage): one thing on it
        float screenX = 50.0f; // its middle, in percent of the screen from the top left
        float screenY = 30.0f;
        float screenSize = 22.0f; // its height, in percent of the screen's
        float screenAlpha = 1.0f;
        float fill = 0.6f; // a bar's fill, 0 to 1
        bool clicked = false;
        float wipe = -1.0f; // going to another scene: how far the curtain is across, 0 to 1; below 0: none

        // ---- Words: a reading ("x = 2"), a sum ("3 + 4 = 7") and a condition's answer
        std::string readout;
        std::optional<bool> answer;
        bool changed = false; // the readout has just changed: it is lit up
    };

    // A preview goes round every kPreviewSeconds: a still start, the block's action, a still end
    constexpr double kPreviewSeconds = 3.5;
    // How big a preview is, at 100%
    constexpr float kPreviewWidth = 340.0f;
    constexpr float kPreviewHeight = 210.0f;
    constexpr float kPreviewStart = 0.15f;
    constexpr float kPreviewAction = 0.6f;
    constexpr float kPreviewEnd = kPreviewStart + kPreviewAction;

    inline PreviewStage StageOf(Engine::Logic::Op op)
    {
        using enum Engine::Logic::Op;
        switch (op)
        {
        case MoveForward:
        case FlyForward:
        case MoveBack:
        case MoveRight:
        case TurnBy:
        case PointInDirection:
        case GoTo:
        case MoveBy:
        case GlideTo:
        case GoToObject:
        case PointTowards:
        case PositionX:
        case PositionZ:
        case Direction:
        case DirectionOf: return PreviewStage::Top;
        case ShowScreenText:
        case ShowScreenPicture:
        case WhenScreenClicked:
        case LockMouse:
        case FreeMouse:
        case GoToScene: return PreviewStage::Screen;
        case Equal:
        case Less:
        case Greater:
        case And:
        case Or:
        case Not:
        case Add:
        case Subtract:
        case Multiply:
        case Divide:
        case Mod:
        case Random:
        case Round:
        case Abs:
        case SetVariable:
        case ChangeVariable:
        case VariableValue:
        case Timer:
        case ResetTimer: return PreviewStage::Value;
        case Call: return PreviewStage::None;
        default: return PreviewStage::Side;
        }
    }

    // A palette block whose numbers are all 0 ("0 + 0", "0 < 0") shows nothing happening: example numbers instead.
    // Only for the palette: a block on the canvas is shown with the numbers typed into it, whatever they are.
    inline Engine::Logic::Block WithExamples(Engine::Logic::Block block)
    {
        using enum Engine::Logic::Op;
        const bool zeros = std::all_of(block.numbers.begin(), block.numbers.end(), [](float n) { return n == 0.0f; });
        if (zeros)
        {
            switch (block.op)
            {
            case Add:
            case Multiply: block.numbers = {3.0f, 4.0f}; break;
            case Subtract: block.numbers = {7.0f, 2.0f}; break;
            case Divide: block.numbers = {8.0f, 2.0f}; break;
            case Mod: block.numbers = {7.0f, 3.0f}; break;
            case Equal: block.numbers = {2.0f, 2.0f}; break;
            case Less:
            case Greater: block.numbers = {2.0f, 5.0f}; break;
            case Round: block.numbers = {2.6f}; break;
            case Abs: block.numbers = {-3.0f}; break;
            case SetVariable: block.numbers = {10.0f}; break;
            default: break;
            }
        }
        return block;
    }

    // Whether a value block fills the block's number field i (its value is only known while the game runs)
    inline bool IsComputed(const Engine::Logic::Block& block, std::size_t i)
    {
        return i < block.inputs.size() && block.inputs[i].id != 0;
    }

    // The block's number field i as the preview uses it: what is typed in, or the palette's default when a value
    // block fills it
    inline float PreviewNumber(const Engine::Logic::Block& block, std::size_t i)
    {
        return IsComputed(block, i) ? Engine::Logic::InfoOf(block.op).numbers[i] : block.numbers[i];
    }

    // A number as the previews write it: at most two decimals, no trailing zeros
    inline std::string PreviewText(double value)
    {
        const double rounded = std::round(value * 100.0) / 100.0;
        return std::format("{:g}", rounded == 0.0 ? 0.0 : rounded);
    }

    // The preview at phase t (0 to 1, the start of one round to the start of the next)
    inline PreviewPose PoseAt(const Engine::Logic::Block& block, float t)
    {
        using enum Engine::Logic::Op;
        const auto n = [&block](std::size_t i) { return PreviewNumber(block, i); };
        const auto num = [&n](std::size_t i) { return PreviewText(n(i)); };
        // A number field in a sum: its number, or the value block in it in brackets ("(distance to Player)"), whose
        // value, and so the answer, is only known while the game runs
        const auto operand = [&](std::size_t i)
        { return IsComputed(block, i) ? "(" + Engine::Logic::Describe(block.inputs[i]) + ")" : num(i); };
        const bool computed = IsComputed(block, 0) || IsComputed(block, 1);
        // How far t is from a to b (0 before, 1 after), and the action's own: how far through, and eased in and out
        const auto between = [t](float a, float b) { return std::clamp((t - a) / (b - a), 0.0f, 1.0f); };
        const float p = between(kPreviewStart, kPreviewEnd);
        const float e = p * p * (3.0f - 2.0f * p);
        const auto lerp = [e](float from, float to) { return from + (to - from) * e; };
        // A hop q of the way through it (0 to 1), up to height
        const auto hop = [](float q, float height) { return height * 4.0f * q * (1.0f - q); };
        // hops hops in the time from a to b
        const auto hops = [&](float a, float b, float count, float height)
        {
            const float q = between(a, b) * count;
            return q >= count ? 0.0f : hop(q - std::floor(q), height);
        };
        const auto name = [&block](const char* otherwise) { return block.text.empty() ? otherwise : block.text; };
        // Where "go to" and "glide" start from, and where "go to / point towards an object" finds it
        constexpr std::array<float, 2> kAway{-2.0f, 1.0f};
        constexpr std::array<float, 2> kOther{3.0f, -1.0f};
        constexpr float kPi = 3.14159265f;

        PreviewPose pose;
        switch (block.op)
        {
        // ---- Events: something happens, then Atti's script runs (a hop)
        case WhenPlayStarts:
            pose.runPressed = t > kPreviewStart && t < 0.35f;
            pose.y = hops(0.4f, 0.8f, 1.0f, 1.0f);
            break;
        case WhenKeyPressed:
            pose.key = name("Space");
            pose.keyDown = t > kPreviewStart && t < 0.35f;
            pose.y = hops(0.4f, 0.8f, 1.0f, 1.0f);
            break;
        case WhenKeyReleased:
            pose.key = name("Space");
            pose.keyDown = t < 0.35f;
            pose.y = hops(0.4f, 0.8f, 1.0f, 1.0f);
            break;
        case WhenKeyHeld:
            pose.key = name("Space");
            pose.keyDown = t > kPreviewStart;
            pose.y = hops(0.4f, 1.0f, 2.0f, 1.0f);
            break;
        case WhenCollision:
            pose.readout = "bumps into something";
            pose.y = hops(0.4f, 0.8f, 1.0f, 1.0f);
            break;
        case WhenTimer:
            pose.readout = "every " + num(0) + (n(0) == 1.0f ? " second" : " seconds");
            pose.y = hops(0.4f, 0.8f, 1.0f, 1.0f);
            break;
        case WhenIReceive:
            if (t < 0.45f)
            {
                pose.envelope = -4.0f + 4.0f * between(kPreviewStart, 0.45f);
            }
            pose.y = hops(0.45f, 0.85f, 1.0f, 1.0f);
            break;
        case Broadcast:
            if (p > 0.0f && p < 1.0f)
            {
                pose.envelope = 4.5f * p;
            }
            pose.readout = "to every object";
            break;

        // ---- Motion
        case MoveForward:
        case FlyForward: pose.y = lerp(0.0f, -n(0)); break; // flying level: the preview has no tilt
        case MoveBack: pose.y = lerp(0.0f, n(0)); break;
        case MoveRight: pose.x = lerp(0.0f, n(0)); break;
        case TurnBy:
        case PointInDirection: pose.facing = lerp(0.0f, n(0)); break;
        case MoveBy:
            pose.x = lerp(0.0f, n(0));
            pose.y = lerp(0.0f, n(2)); // ponytail: from above, a change in y (height) does not show
            break;
        case GoTo:
        case GlideTo:
        {
            const std::size_t first = block.op == GlideTo ? 1 : 0; // glide's first number is its seconds
            pose.target = std::array{n(first), n(first + 2)};
            const float k = block.op == GoTo ? (p >= 0.5f ? 1.0f : 0.0f) : e; // "go to" is there at once
            pose.x = kAway[0] + ((*pose.target)[0] - kAway[0]) * k;
            pose.y = kAway[1] + ((*pose.target)[1] - kAway[1]) * k;
            break;
        }
        case GoToObject:
            pose.target = kOther;
            pose.x = p >= 0.5f ? kOther[0] : 0.0f;
            pose.y = p >= 0.5f ? kOther[1] : 0.0f;
            break;
        case PointTowards:
        {
            pose.target = kOther;
            // Direction 0 faces up (-y) and 90 faces left (-x)
            const float towards = std::atan2(-kOther[0], -kOther[1]) * 180.0f / kPi;
            pose.facing = lerp(0.0f, towards);
            break;
        }
        case SetTilt:
            pose.tilt = lerp(0.0f, std::clamp(n(0), -90.0f, 90.0f));
            pose.readout = "tilt " + PreviewText(*pose.tilt);
            break;

        // ---- Looks
        case Show: pose.alpha = lerp(0.15f, 1.0f); break;
        case Hide: pose.alpha = lerp(1.0f, 0.15f); break; // faint, not gone: it is still there
        case SetColor:
            pose.tint = {std::clamp(n(0), 0.0f, 1.0f), std::clamp(n(1), 0.0f, 1.0f), std::clamp(n(2), 0.0f, 1.0f)};
            pose.tintAmount = lerp(0.0f, 0.75f);
            break;
        case SetSize: pose.size = lerp(1.0f, std::max(0.0f, n(0))); break;
        case ChangeSizeBy: pose.size = lerp(1.0f, std::max(0.0f, 1.0f + n(0))); break;
        case Say: pose.speaking = p > 0.0f; break;
        case PlayAnimation:
            pose.mark = name("Walk");
            pose.clip = block.text;
            pose.clipTime = t * static_cast<float>(kPreviewSeconds);
            pose.y = std::abs(std::sin(t * 2.0f * kPi * 4.0f)) * 0.25f; // it keeps going
            break;
        case PlayAnimationOnce:
            pose.mark = name("Jump") + " (once)";
            pose.clip = block.text;
            pose.clipFraction = p;
            pose.y = hops(kPreviewStart, 0.45f, 1.0f, 0.4f);
            break;
        case StopAnimation:
            pose.y = t < 0.45f ? std::abs(std::sin(t * 2.0f * kPi * 4.0f)) * 0.25f : 0.0f;
            pose.readout = t < 0.45f ? "playing" : "stopped";
            pose.changed = t >= 0.45f && t < 0.6f;
            break;
        case SetAnimationSpeed:
        {
            const float speed = std::clamp(lerp(1.0f, n(0)), 0.0f, 4.0f);
            pose.y = std::abs(std::sin(t * 2.0f * kPi * 2.0f * speed)) * 0.25f;
            pose.readout = "speed " + PreviewText(n(0));
            break;
        }

        // ---- Loops
        case Repeat:
        {
            const float times = std::max(0.0f, std::round(n(0)));
            const float shown = std::min(times, 5.0f); // at most five hops; the count still goes all the way
            pose.y = hops(kPreviewStart, kPreviewEnd, shown, 0.7f);
            pose.readout = std::format("{} / {}", static_cast<int>(std::ceil(p * times)), static_cast<int>(times));
            break;
        }
        case Forever:
            pose.y = hops(0.0f, 1.0f, 3.0f, 0.7f);
            pose.readout = "again and again";
            break;
        case RepeatUntil:
            pose.answer = t >= 0.65f;
            pose.y = *pose.answer ? 0.0f : hops(0.0f, 0.65f, 3.0f, 0.7f);
            pose.readout = *pose.answer ? "true: done" : "until...";
            break;
        case Wait:
            pose.progress = p;
            pose.readout = "wait " + num(0) + (n(0) == 1.0f ? " second" : " seconds");
            pose.y = hops(kPreviewEnd, 1.0f, 1.0f, 0.7f); // then the next block
            break;
        case WaitUntil:
            pose.answer = t >= 0.55f;
            pose.readout = *pose.answer ? "true: go on" : "waiting...";
            pose.y = hops(0.6f, 0.95f, 1.0f, 0.7f);
            break;
        case Stop:
            pose.y = t < 0.45f ? hops(0.0f, 0.45f, 2.0f, 0.7f) : 0.0f;
            pose.readout = t < 0.45f ? "running" : "stopped";
            pose.changed = t >= 0.45f && t < 0.6f;
            break;

        // ---- Conditions: if runs its inside on the first half (true), skips it (or runs else) on the second
        case If:
        case IfElse:
        {
            const bool first = t < 0.5f;
            pose.answer = first;
            const float q = first ? between(0.1f, 0.45f) : between(0.6f, 0.95f);
            if (first)
            {
                pose.y = hop(q, 0.8f);
                pose.readout = "true: runs the inside";
            }
            else if (block.op == IfElse)
            {
                pose.x = 1.2f * std::sin(q * kPi);
                pose.readout = "false: runs else";
            }
            else
            {
                pose.readout = "false: skips it";
            }
            break;
        }
        case Equal:
        case Less:
        case Greater:
        {
            const char* sign = block.op == Equal ? " = " : block.op == Less ? " < " : " > ";
            pose.readout = operand(0) + sign + operand(1);
            if (t >= 0.45f && !computed)
            {
                pose.answer = block.op == Equal ? n(0) == n(1) : block.op == Less ? n(0) < n(1) : n(0) > n(1);
                pose.changed = t < 0.6f;
            }
            break;
        }
        case And:
        case Or:
        {
            // Every pair in turn: true and true, true and false, false and true, false and false
            const int which = std::clamp(static_cast<int>(t * 4.0f), 0, 3);
            const bool a = which < 2;
            const bool b = which % 2 == 0;
            pose.readout =
                std::format("{} {} {}", a ? "true" : "false", block.op == And ? "and" : "or", b ? "true" : "false");
            pose.answer = block.op == And ? a && b : a || b;
            break;
        }
        case Not:
        {
            const bool a = t < 0.5f;
            pose.readout = a ? "not true" : "not false";
            pose.answer = !a;
            break;
        }

        // ---- Math: the sum, then its answer
        case Add:
        case Subtract:
        case Multiply:
        case Divide:
        case Mod:
        {
            const char* sign = block.op == Add        ? " + "
                               : block.op == Subtract ? " - "
                               : block.op == Multiply ? " * "
                               : block.op == Divide   ? " / "
                                                      : " mod ";
            pose.readout = operand(0) + sign + operand(1);
            if (t >= 0.45f && computed)
            {
                pose.readout += " = ?";
            }
            else if (t >= 0.45f)
            {
                const double a = n(0);
                const double b = n(1);
                if ((block.op == Divide || block.op == Mod) && b == 0.0)
                {
                    pose.readout += " = 0 (no answer: a problem)"; // as the game does
                }
                else
                {
                    const double answer = block.op == Add        ? a + b
                                          : block.op == Subtract ? a - b
                                          : block.op == Multiply ? a * b
                                          : block.op == Divide   ? a / b
                                                                 : a - b * std::floor(a / b);
                    pose.readout += " = " + PreviewText(answer);
                }
                pose.changed = t < 0.6f;
            }
            break;
        }
        case Random:
        {
            const double low = std::min(n(0), n(1));
            const double high = std::max(n(0), n(1));
            const bool whole = low == std::floor(low) && high == std::floor(high);
            // Rolls a new number every few frames, then settles on one
            const int roll = static_cast<int>(std::min(t, 0.55f) * 25.0f);
            const double k = std::fmod(std::abs(std::sin(roll * 12.9898 + 78.233)) * 43758.5453, 1.0);
            const double picked = whole ? low + std::floor(k * (high - low + 1.0)) : low + k * (high - low);
            pose.readout = "pick random " + num(0) + " to " + num(1) + " = " + PreviewText(picked);
            pose.changed = t >= 0.55f && t < 0.7f;
            break;
        }
        case Round:
        case Abs:
            pose.readout = (block.op == Round ? "round " : "abs of ") + operand(0);
            if (t >= 0.45f)
            {
                pose.readout += " = " + (computed ? std::string("?")
                                                  : PreviewText(block.op == Round ? std::round(n(0)) : std::abs(n(0))));
                pose.changed = t < 0.6f;
            }
            break;

        // ---- Variables
        case SetVariable:
        case ChangeVariable:
        {
            const float before = block.op == SetVariable && n(0) == 0.0f ? 5.0f : 0.0f;
            const float after = block.op == SetVariable ? n(0) : before + n(0);
            pose.readout = name("score") + " = " + PreviewText(t >= 0.45f ? after : before);
            pose.changed = t >= 0.45f && t < 0.6f;
            break;
        }
        case VariableValue:
            pose.readout = name("score") + (t >= 0.45f ? " gives its value, e.g. 7" : "");
            pose.changed = t >= 0.45f && t < 0.6f;
            break;

        // ---- Sensing
        case KeyDown:
            pose.key = name("Space");
            pose.keyDown = t > 0.3f && t < 0.7f;
            pose.answer = pose.keyDown;
            break;
        case Timer: pose.readout = "timer = " + PreviewText(std::floor(t * kPreviewSeconds * 10.0) / 10.0); break;
        case ResetTimer:
        {
            const double seconds = t < 0.45f ? 3.0 + t * kPreviewSeconds : (t - 0.45f) * kPreviewSeconds;
            pose.readout = "timer = " + PreviewText(std::floor(seconds * 10.0) / 10.0);
            pose.changed = t >= 0.45f && t < 0.6f;
            break;
        }
        case PositionX:
        case PositionZ:
        {
            const float along = lerp(-1.5f, 1.5f);
            (block.op == PositionX ? pose.x : pose.y) = along;
            pose.readout = (block.op == PositionX ? "x = " : "z = ") + PreviewText(along);
            break;
        }
        case PositionY:
            pose.y = hop(p, 1.5f);
            pose.readout = "y = " + PreviewText(pose.y);
            break;
        case Direction:
        case DirectionOf:
            pose.facing = lerp(0.0f, 90.0f);
            pose.mark = block.op == DirectionOf ? name("Player") : "";
            pose.readout = "direction = " + PreviewText(std::round(pose.facing));
            break;
        case TiltOf:
            pose.mark = name("Player");
            pose.tilt = lerp(0.0f, 30.0f);
            pose.readout = "tilt = " + PreviewText(std::round(*pose.tilt));
            break;
        case DistanceTo:
            pose.other = lerp(4.0f, 1.5f);
            pose.readout = "distance = " + PreviewText(*pose.other);
            break;

        // ---- Physics
        case Jump: pose.y = std::max(0.0f, n(0)) * 4.0f * p * (1.0f - p); break;
        case PushForward: pose.x = n(0) * 0.4f * (1.0f - (1.0f - p) * (1.0f - p)); break; // a kick that slows down
        case PushBy:
            // Thrown: sideways by x, up by y and falling back (from the side, z does not show)
            pose.x = n(0) * 0.4f * p;
            pose.y = std::max(0.0f, n(1)) * 0.3f * 4.0f * p * (1.0f - p);
            break;
        case ApplyForce:
            // As a push, but a force is gentler
            pose.x = n(0) * 0.1f * p;
            pose.y = std::max(0.0f, n(1)) * 0.1f * 4.0f * p * (1.0f - p);
            break;
        case OnGround:
            pose.y = hop(p, 1.5f);
            pose.answer = pose.y < 0.05f;
            break;
        case Touching:
            pose.other = 3.0f;
            pose.x = lerp(0.0f, 1.8f);
            pose.answer = *pose.other - pose.x <= 1.3f; // Atti's half and the other's
            break;

        // ---- Sound
        case PlaySound:
            pose.mark = name("a sound");
            pose.sound = p > 0.0f && p < 1.0f;
            break;
        case PlayFootstep:
            pose.mark = "step";
            pose.x = lerp(0.0f, 0.6f);
            pose.y = hop(p, 0.2f);
            pose.sound = p > 0.3f && p < 1.0f;
            break;

        // ---- The game's screen
        case ShowScreenText:
        case ShowScreenPicture: pose.screenAlpha = e; break;
        case WhenScreenClicked:
            pose.clicked = t > 0.3f && t < 0.45f;
            pose.readout = t >= 0.45f ? "its script runs" : "";
            pose.changed = t >= 0.45f && t < 0.6f;
            break;
        case LockMouse:
        case FreeMouse:
            pose.screenAlpha = block.op == LockMouse ? 1.0f - e : e; // the pointer
            pose.readout = block.op == LockMouse ? "the mouse turns the player" : "the mouse moves the pointer";
            break;
        case GoToScene:
            pose.wipe = p;
            pose.readout = name("main");
            break;
        default: break;
        }
        return pose;
    }

    // The phase to hold still on when Windows' "Show animations" is off: a jump at its top, a condition or sum with
    // its answer, anything else done
    inline float StillPhase(Engine::Logic::Op op)
    {
        using enum Engine::Logic::Op;
        switch (op)
        {
        case Jump:
        case PushBy:
        case OnGround:
        case PositionY: return kPreviewStart + kPreviewAction * 0.5f;
        case If:
        case IfElse:
        case Not: return 0.3f;
        default: return kPreviewEnd;
        }
    }

    // What acts an object's blocks out in their previews: its own look, or, when it draws nothing itself (its model
    // is on an object under it, as a player's often is), the first object under it that draws something; nullptr:
    // nothing does, and Atti acts them out
    inline const Engine::Scene::RenderableDesc* LookOf(const Engine::Scene::SceneDescription& scene,
                                                       const Engine::Scene::GameObject& object)
    {
        if (object.renderable)
        {
            return &*object.renderable;
        }
        for (const std::uint32_t id : Engine::Scene::SubtreeOf(scene, object.id))
        {
            if (const Engine::Scene::GameObject* each = Engine::Scene::FindObject(scene, id); each && each->renderable)
            {
                return &*each->renderable;
            }
        }
        return nullptr;
    }

    // The animator of whatever LookOf draws: its own, or, when it draws nothing itself, that of the object under it
    // that does, or else its own again; nullptr: none
    inline const Engine::Scene::AnimatorComponent* AnimatorOf(const Engine::Scene::SceneDescription& scene,
                                                              const Engine::Scene::GameObject& object)
    {
        const auto own = [](const Engine::Scene::GameObject& each)
        { return each.animator ? &*each.animator : nullptr; };
        if (!object.renderable)
        {
            for (const std::uint32_t id : Engine::Scene::SubtreeOf(scene, object.id))
            {
                const Engine::Scene::GameObject* each = Engine::Scene::FindObject(scene, id);
                if (each && each->renderable && each->animator)
                {
                    return own(*each);
                }
                if (each && each->renderable)
                {
                    break;
                }
            }
        }
        return own(object);
    }

    /*!
    \brief  Draws the block previews in 3D: the object acts the block out on a little floor, rendered into a
            framebuffer of its own, with the words, keys and bars drawn over it. Its shape or model is read the first
            time it is shown and kept until another is asked for. Needs the editor's OpenGL context: call Clear
            before the context goes away.
    */
    class BlockPreviewer
    {
    public:
        BlockPreviewer();
        ~BlockPreviewer();
        BlockPreviewer(const BlockPreviewer&) = delete;
        BlockPreviewer& operator=(const BlockPreviewer&) = delete;

        /*!
        \brief   Draws the block's preview in the window being drawn (a tooltip) at the cursor, and moves the cursor
                 past it. Does nothing for a block with no preview (StageOf is None).
        \param   block The block.
        \param   projectDir The game folder look's files are relative to.
        \param   look What acts the block out: the object's shape, mesh or model; nullptr: Atti, a yellow cube.
        \param   animator What its model plays, and the files of further clips (nullptr: only its own clips).
        */
        void Draw(const Engine::Logic::Block& block, const std::filesystem::path& projectDir,
                  const Engine::Scene::RenderableDesc* look,
                  const Engine::Scene::AnimatorComponent* animator = nullptr);

        /*!
        \brief   Draws a script's preview the same way, from its first step each time the mouse comes to its block:
                 the object acting it out, or its variables changing when nothing touches the object. It shows one
                 step at a time, played over and over; E goes to the next step and Q to the one before, without
                 the game running.
        \param   script The script (ScriptPreview.hpp).
        \param   blockId The block being shown.
        \param   projectDir The game folder look's files are relative to.
        \param   look What acts it out: the object's shape, mesh or model; nullptr: Atti, a yellow cube.
        \param   animator What its model plays, and the files of further clips (nullptr: only its own clips).
        */
        void DrawScript(const ScriptPreview& script, std::uint32_t blockId, const std::filesystem::path& projectDir,
                        const Engine::Scene::RenderableDesc* look,
                        const Engine::Scene::AnimatorComponent* animator = nullptr);

        // The block the script preview is showing now, to be highlighted where it is drawn (nullptr: none). Only
        // compared with, never read: it may be gone once the blocks are edited.
        const Engine::Logic::Block* ShownBlock() const;

        // Frees the framebuffer, shader and meshes (before the GL context goes)
        void Clear();

        struct Gpu; // its OpenGL side, in BlockPreview.cpp

    private:
        std::unique_ptr<Gpu> gpu_;                    // made on the first Draw
        std::uint32_t scriptBlock_ = 0;               // the block whose script is shown...
        std::size_t scriptStep_ = 0;                  // ...at this step (E: the next one, Q: the one before)...
        double scriptStarted_ = 0.0;                  // ...since this time...
        double scriptSeen_ = 0.0;                     // ...last drawn at this time
        const Engine::Logic::Block* shown_ = nullptr; // the step's block
    };
}
