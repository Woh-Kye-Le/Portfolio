/* !
@file    	    LessonGoals.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		05/11/2026
@brief
    Implements the checks behind a lesson's steps: reads a check's words, then
    looks through the scene's objects and scripts, or asks the running game.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/editor/LessonGoals.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <string>
#include <vector>

namespace Engine::Editor
{
    namespace
    {
        using Logic::Block;
        using Logic::Op;

        constexpr double kLandingSeconds = 2.5; // long enough for what was dropped to have reached the ground
        constexpr double kWatchSeconds = 1.0;   // a script ended, and the learner has had time to see it
        constexpr float kBouncy = 0.3f;

        std::vector<std::string_view> WordsOf(std::string_view text)
        {
            std::vector<std::string_view> words;
            std::size_t at = 0;
            while (at < text.size())
            {
                const std::size_t end = std::min(text.find(' ', at), text.size());
                if (end > at)
                {
                    words.push_back(text.substr(at, end - at));
                }
                at = end + 1;
            }
            return words;
        }

        bool SameName(std::string_view a, std::string_view b)
        {
            return std::equal(
                a.begin(), a.end(), b.begin(), b.end(), [](char x, char y)
                { return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y)); });
        }

        std::optional<double> NumberIn(std::string_view word)
        {
            double value = 0.0;
            const auto [end, error] = std::from_chars(word.data(), word.data() + word.size(), value);
            return error == std::errc() && end == word.data() + word.size() ? std::optional(value) : std::nullopt;
        }

        // A check's words are checked before it runs, so these find what they look for
        double Number(std::string_view word)
        {
            return NumberIn(word).value_or(0.0);
        }

        Op OpIn(std::string_view word)
        {
            return Logic::OpNamed(word).value_or(Op{});
        }

        const Scene::GameObject* ObjectNamed(const Scene::SceneDescription& scene, std::string_view name)
        {
            const auto found =
                std::find_if(scene.objects.begin(), scene.objects.end(),
                             [name](const Scene::GameObject& each) { return SameName(each.name, name); });
            return found != scene.objects.end() ? &*found : nullptr;
        }

        bool StartsWithHat(const Logic::Script& script)
        {
            return !script.blocks.empty() && Logic::InfoOf(script.blocks.front().op).shape == Logic::BlockShape::Hat;
        }

        // In a check "if" also means "if / else": they are the same idea
        bool IsOp(const Block& block, Op op)
        {
            return block.op == op || (op == Op::If && block.op == Op::IfElse);
        }

        // Calls visit for blocks and every block run inside them, in the order they are written
        template <typename Visit> void Walk(const std::vector<Block>& blocks, Visit&& visit)
        {
            for (const Block& block : blocks)
            {
                visit(block);
                Walk(block.body, visit);
                Walk(block.elseBody, visit);
            }
        }

        // The same over every script of the object that runs
        template <typename Visit> void WalkRunning(const Scene::GameObject& object, Visit&& visit)
        {
            if (!object.logic)
            {
                return;
            }
            for (const Logic::Script& script : object.logic->scripts)
            {
                if (StartsWithHat(script))
                {
                    Walk(script.blocks, visit);
                }
            }
        }

        bool Holds(const std::vector<Block>& blocks, Op op)
        {
            bool found = false;
            Walk(blocks, [&](const Block& each) { found = found || IsOp(each, op); });
            return found;
        }

        // Whether a condition or number of block, or one inside those, is op
        bool InSlots(const Block& block, Op op)
        {
            const auto holds = [op](const std::vector<Block>& list)
            {
                return std::any_of(list.begin(), list.end(), [op](const Block& each)
                                   { return each.id != 0 && (IsOp(each, op) || InSlots(each, op)); });
            };
            return holds(block.slots) || holds(block.inputs);
        }

        bool ScriptHas(const Logic::Script& script, const std::vector<Op>& wanted)
        {
            std::size_t next = 0;
            Walk(script.blocks,
                 [&](const Block& each)
                 {
                     if (next < wanted.size() && IsOp(each, wanted[next]))
                     {
                         ++next;
                     }
                 });
            return next == wanted.size();
        }

        std::optional<Game::Shape> ShapeNamed(std::string_view word)
        {
            using Game::Shape;
            return word == "cube"      ? std::optional(Shape::Cube)
                   : word == "sphere"  ? std::optional(Shape::Sphere)
                   : word == "pyramid" ? std::optional(Shape::Pyramid)
                   : word == "plane"   ? std::optional(Shape::Plane)
                                       : std::nullopt;
        }

        std::optional<Scene::BodyType> BodyNamed(std::string_view word)
        {
            using Scene::BodyType;
            return word == "static"      ? std::optional(BodyType::Static)
                   : word == "dynamic"   ? std::optional(BodyType::Dynamic)
                   : word == "kinematic" ? std::optional(BodyType::Kinematic)
                                         : std::nullopt;
        }

        std::optional<bool> HasPart(const Scene::GameObject& object, std::string_view part)
        {
            return part == "body"       ? std::optional(object.rigidBody.has_value())
                   : part == "light"    ? std::optional(object.light.has_value())
                   : part == "sky"      ? std::optional(object.sky.has_value())
                   : part == "camera"   ? std::optional(object.camera.has_value())
                   : part == "player"   ? std::optional(object.player.has_value())
                   : part == "sound"    ? std::optional(object.audioSource.has_value())
                   : part == "screen"   ? std::optional(object.screen.has_value())
                   : part == "animator" ? std::optional(object.animator.has_value())
                                        : std::nullopt;
        }

        bool IsColored(const Scene::GameObject& object)
        {
            const auto white = [](float r, float g, float b) { return r == 1.0f && g == 1.0f && b == 1.0f; };
            return (object.renderable &&
                    !white(object.renderable->albedoR, object.renderable->albedoG, object.renderable->albedoB)) ||
                   (object.light && !white(object.light->colorR, object.light->colorG, object.light->colorB));
        }

        float Along(float ax, float az, float bx, float bz)
        {
            return std::hypot(ax - bx, az - bz);
        }

        // Whether the block has the number or text the check's words say
        bool FieldIs(const Block& block, const std::vector<std::string_view>& words)
        {
            if (words[0] == "text")
            {
                return SameName(block.text, words[3]);
            }
            const auto i = static_cast<std::size_t>(Number(words[3]));
            return i < block.numbers.size() &&
                   std::abs(static_cast<double>(block.numbers[i]) - Number(words[4])) < 1e-4;
        }

        // The same of block, or of a block in its conditions and numbers: "distance to Flag" is not in a stack
        bool FieldIn(const Block& block, Op op, const std::vector<std::string_view>& words)
        {
            const auto inside = [&](const std::vector<Block>& list)
            {
                return std::any_of(list.begin(), list.end(),
                                   [&](const Block& each) { return each.id != 0 && FieldIn(each, op, words); });
            };
            return (IsOp(block, op) && FieldIs(block, words)) || inside(block.slots) || inside(block.inputs);
        }

        // What is checked about an object's blocks with a number or word at the end
        bool BlockFieldIs(const Scene::GameObject& object, Op op, const std::vector<std::string_view>& words)
        {
            bool found = false;
            WalkRunning(object, [&](const Block& each) { found = found || FieldIn(each, op, words); });
            return found;
        }
    }

    bool IsValidGoal(std::string_view check)
    {
        const auto words = WordsOf(check);
        if (words.empty())
        {
            return false;
        }
        const std::string_view verb = words[0];
        const std::size_t count = words.size();
        const auto op = [&](std::size_t i) { return Logic::OpNamed(words[i]).has_value(); };
        const auto number = [&](std::size_t i) { return NumberIn(words[i]).has_value(); };
        if (verb == "object" || verb == "bouncy" || verb == "moved" || verb == "sized" || verb == "colored" ||
            verb == "finished")
        {
            return count == 2;
        }
        if (verb == "shape")
        {
            return count == 3 && ShapeNamed(words[2]).has_value();
        }
        if (verb == "body")
        {
            return count == 3 && BodyNamed(words[2]).has_value();
        }
        if (verb == "has")
        {
            return count == 3 && HasPart(Scene::GameObject{}, words[2]).has_value();
        }
        if (verb == "variable" || verb == "landed")
        {
            return count == 3;
        }
        if (verb == "above" || verb == "apart" || verb == "near" || verb == "reaches")
        {
            return count == 4 && number(3);
        }
        if (verb == "travelled" || verb == "dropped")
        {
            return count == 3 && number(2);
        }
        if (verb == "script")
        {
            bool blocks = count >= 3;
            for (std::size_t i = 2; i < count; ++i)
            {
                blocks = blocks && op(i);
            }
            return blocks;
        }
        if (verb == "uses" || verb == "gone" || verb == "ran")
        {
            return count == 3 && op(2);
        }
        if (verb == "inside" || verb == "slot")
        {
            return count == 4 && op(2) && op(3);
        }
        if (verb == "number")
        {
            return count == 5 && op(2) && number(3) && number(4);
        }
        if (verb == "text")
        {
            return count == 4 && op(2);
        }
        return false;
    }

    bool CheckGoal(const Scene::SceneDescription& scene, std::string_view check, const GoalWorld& world)
    {
        if (!IsValidGoal(check))
        {
            return false;
        }
        const auto words = WordsOf(check);
        const std::string_view verb = words[0];
        const auto placeOf = [&](std::string_view name)
        { return world.place ? world.place(name) : std::optional<std::array<float, 3>>{}; };

        // ---- What happens in the running game
        if (verb == "reaches")
        {
            const auto value = world.variable ? world.variable(words[1], words[2]) : std::nullopt;
            return value && *value >= Number(words[3]);
        }
        if (verb == "near")
        {
            const auto a = placeOf(words[1]);
            const auto b = placeOf(words[2]);
            return a && b && Along((*a)[0], (*a)[2], (*b)[0], (*b)[2]) <= Number(words[3]);
        }
        if (verb == "finished")
        {
            return world.finished && world.seconds >= kWatchSeconds && world.finished(words[1]);
        }
        if (verb == "ran")
        {
            return world.ran && world.ran(words[1], OpIn(words[2]));
        }
        if (verb == "landed")
        {
            const auto a = placeOf(words[1]);
            const auto b = placeOf(words[2]);
            return a && b && world.seconds >= kLandingSeconds && (*a)[1] >= (*b)[1];
        }

        const Scene::GameObject* object = ObjectNamed(scene, words[1]);
        if (verb == "object" || !object)
        {
            return object != nullptr;
        }
        if (verb == "travelled" || verb == "dropped")
        {
            const auto now = placeOf(words[1]);
            if (!now || !object->transform)
            {
                return false;
            }
            const Scene::TransformComponent& start = *object->transform;
            const double wanted = Number(words[2]);
            return verb == "travelled" ? Along((*now)[0], (*now)[2], start.x, start.z) >= wanted
                                       : start.y - (*now)[1] >= wanted;
        }

        // ---- What is in the scene
        if (verb == "shape")
        {
            return object->renderable && object->renderable->shape == ShapeNamed(words[2]);
        }
        if (verb == "has")
        {
            return HasPart(*object, words[2]).value_or(false);
        }
        if (verb == "body")
        {
            return object->rigidBody && object->rigidBody->type == BodyNamed(words[2]);
        }
        if (verb == "bouncy")
        {
            return object->rigidBody && object->rigidBody->bounciness >= kBouncy;
        }
        if (verb == "colored")
        {
            return IsColored(*object);
        }
        if (verb == "moved")
        {
            return object->transform &&
                   (object->transform->x != 0.0f || object->transform->y != 0.0f || object->transform->z != 0.0f);
        }
        if (verb == "sized")
        {
            const auto& t = object->transform;
            return t && (t->scale != 1.0f || t->scaleX != 1.0f || t->scaleY != 1.0f || t->scaleZ != 1.0f);
        }
        if (verb == "above" || verb == "apart")
        {
            const Scene::GameObject* other = ObjectNamed(scene, words[2]);
            if (!other || !object->transform || !other->transform)
            {
                return false;
            }
            const Scene::TransformComponent& a = *object->transform;
            const Scene::TransformComponent& b = *other->transform;
            const double wanted = Number(words[3]);
            return verb == "above" ? a.y - b.y >= wanted : Along(a.x, a.z, b.x, b.z) >= wanted;
        }

        // ---- What its blocks are
        if (verb == "variable")
        {
            return object->logic &&
                   std::any_of(object->logic->variables.begin(), object->logic->variables.end(),
                               [&](const Logic::Variable& each) { return SameName(each.name, words[2]); });
        }
        if (verb == "script")
        {
            std::vector<Op> wanted;
            for (std::size_t i = 2; i < words.size(); ++i)
            {
                wanted.push_back(OpIn(words[i]));
            }
            return object->logic && std::any_of(object->logic->scripts.begin(), object->logic->scripts.end(),
                                                [&](const Logic::Script& each)
                                                { return StartsWithHat(each) && ScriptHas(each, wanted); });
        }

        const Op first = OpIn(words[2]);
        if (verb == "number" || verb == "text")
        {
            return BlockFieldIs(*object, first, words);
        }
        if (verb == "gone")
        {
            return !object->logic ||
                   std::none_of(object->logic->scripts.begin(), object->logic->scripts.end(),
                                [first](const Logic::Script& each) { return Holds(each.blocks, first); });
        }
        bool found = false;
        if (verb == "uses")
        {
            WalkRunning(*object, [&](const Block& each) { found = found || IsOp(each, first); });
            return found;
        }
        const Op second = OpIn(words[3]);
        WalkRunning(*object,
                    [&](const Block& each)
                    {
                        if (IsOp(each, first))
                        {
                            found =
                                found || (verb == "inside" ? Holds(each.body, second) || Holds(each.elseBody, second)
                                                           : InSlots(each, second));
                        }
                    });
        return found;
    }
}
