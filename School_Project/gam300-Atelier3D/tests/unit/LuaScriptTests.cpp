// Scripting::LuaScript: running scripts, what they can do in the game, their errors, hot reload and the compile cache.

#include "engine/scripting/LuaScript.hpp"
#include "engine/scripting/ScriptSource.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using Engine::Scripting::LuaCompileCache;
using Engine::Scripting::LuaHost;
using Engine::Scripting::LuaScript;
using Engine::Scripting::ScriptSource;

namespace
{
    // A game for scripts to act on: it writes down what they ask of it
    struct FakeGame
    {
        std::array<double, 3> moved{0.0, 0.0, 0.0};
        std::vector<std::string> logged;
        std::vector<std::string> sounds;
        std::vector<std::pair<std::string, bool>> clips;
        std::vector<std::pair<std::string, double>> sent;
        std::map<std::string, std::string> modules;
        std::vector<std::string> required;

        LuaHost Host()
        {
            LuaHost host;
            host.move = [this](double x, double y, double z)
            {
                moved[0] += x;
                moved[1] += y;
                moved[2] += z;
            };
            host.position = [this]() { return std::optional(moved); };
            host.playSound = [this](const std::string& name) { sounds.push_back(name); };
            host.animate = [this](const std::string& clip, bool loop) { clips.emplace_back(clip, loop); };
            host.send = [this](const std::string& message, double value) { sent.emplace_back(message, value); };
            host.log = [this](const std::string& text) { logged.push_back(text); };
            host.require = [this](const std::string& name) -> ScriptSource
            {
                required.push_back(name);
                const auto found = modules.find(name);
                return found != modules.end() ? ScriptSource{found->second, {}}
                                              : ScriptSource{{}, "Script file not found: " + name};
            };
            return host;
        }
    };

    bool Has(const std::string& text, const std::string& part)
    {
        return text.find(part) != std::string::npos;
    }
}

// ---------------------------------------------------------------- Running scripts

TEST(LuaScript, RunsVariablesFunctionsLoopsConditionalsAndTables)
{
    FakeGame game;
    LuaScript script("@sample.lua",
                     "local function double(n) return n * 2 end\n"
                     "total = 0\n"
                     "for i = 1, 4 do total = total + double(i) end\n"
                     "if total > 10 then size = 'big' else size = 'small' end\n"
                     "items = {3, 1, 2}\n"
                     "table.sort(items)\n"
                     "first = items[1]\n"
                     "log(size .. ' ' .. total)\n",
                     game.Host());
    ASSERT_TRUE(script.Error().empty()) << script.Error();

    script.Update(0.016);

    EXPECT_TRUE(script.Error().empty()) << script.Error();
    EXPECT_EQ(script.Variables().at("total").text, "20");
    EXPECT_EQ(script.Variables().at("first").text, "1");
    ASSERT_EQ(game.logged.size(), 1u);
    EXPECT_EQ(game.logged[0], "big 20");
}

TEST(LuaScript, CallsTheEngine)
{
    FakeGame game;
    LuaScript script("@sample.lua",
                     "move(1, 2, 3)\n"
                     "x, y, z = position()\n"
                     "play_sound('jump')\n"
                     "animate('run')\n"
                     "animate('wave', false)\n"
                     "send('door open', 4)\n",
                     game.Host());
    script.Update(0.016);

    ASSERT_TRUE(script.Error().empty()) << script.Error();
    EXPECT_EQ(game.moved, (std::array<double, 3>{1.0, 2.0, 3.0}));
    EXPECT_EQ(script.Variables().at("z").type, "number");
    EXPECT_EQ(game.sounds, std::vector<std::string>{"jump"});
    EXPECT_EQ(game.clips, (std::vector<std::pair<std::string, bool>>{{"run", true}, {"wave", false}}));
    EXPECT_EQ(game.sent, (std::vector<std::pair<std::string, double>>{{"door open", 4.0}}));
}

TEST(LuaScript, AScriptWithoutAGameDoesNothing)
{
    LuaScript script("@sample.lua", "move(1, 0, 0)\nplay_sound('a')\nx = position()\n", LuaHost{});
    script.Update(0.016);
    EXPECT_TRUE(script.Error().empty()) << script.Error();
}

TEST(LuaScript, HooksRunInOrder)
{
    FakeGame game;
    LuaScript script("@sample.lua",
                     "log('top')\n"
                     "function OnInit() log('init') end\n"
                     "function OnUpdate(dt) log('update') end\n"
                     "function OnDestroy() log('destroy') end\n",
                     game.Host());
    script.Update(0.016);
    script.Update(0.016);
    script.Destroy();
    script.Destroy();

    EXPECT_EQ(game.logged, (std::vector<std::string>{"top", "init", "update", "update", "destroy"}));
    EXPECT_EQ(script.Calls(), 2u);
}

TEST(LuaScript, TheOldUpdateNameStillWorks)
{
    FakeGame game;
    LuaScript script("@sample.lua", "function update(dt) log('update') end\n", game.Host());
    script.Update(0.016);
    EXPECT_EQ(game.logged, std::vector<std::string>{"update"});
}

TEST(LuaScript, ANeverStartedScriptIsNotDestroyed)
{
    FakeGame game;
    LuaScript script("@sample.lua", "function OnDestroy() log('destroy') end\n", game.Host());
    script.Destroy();
    EXPECT_TRUE(game.logged.empty());
}

// ---------------------------------------------------------------- What the debugger sees

TEST(LuaScript, VariablesHaveTypesAndTablesAreWrittenOut)
{
    LuaScript script("@sample.lua",
                     "speed = 2\n"
                     "name = 'Bo'\n"
                     "alive = true\n"
                     "stats = {hp = 10, pos = {x = 1}}\n",
                     LuaHost{});
    script.Update(0.016);

    const auto& variables = script.Variables();
    EXPECT_EQ(variables.at("speed").type, "number");
    EXPECT_EQ(variables.at("name").type, "string");
    EXPECT_EQ(variables.at("alive").type, "boolean");
    EXPECT_EQ(variables.at("stats").type, "table");
    EXPECT_TRUE(Has(variables.at("stats").text, "hp = 10")) << variables.at("stats").text;
    EXPECT_TRUE(Has(variables.at("stats").text, "x = 1")) << variables.at("stats").text;
    EXPECT_EQ(variables.count("math"), 0u) << "the libraries are not the script's variables";
}

TEST(LuaScript, TheTraceHasLinesLocalsAndTheCallStack)
{
    LuaScript script("@sample.lua",
                     "local function inner(n)\n"
                     "  local doubled = n * 2\n"
                     "  return doubled\n"
                     "end\n"
                     "function OnUpdate(dt)\n"
                     "  local result = inner(4)\n"
                     "end\n",
                     LuaHost{});
    script.Update(0.016);
    ASSERT_TRUE(script.Error().empty()) << script.Error();

    const auto& trace = script.Trace();
    const auto inInner = std::find_if(trace.begin(), trace.end(), [](const auto& each) { return each.line == 3; });
    ASSERT_NE(inInner, trace.end());
    EXPECT_EQ(inInner->variables.at("doubled"), "8");
    ASSERT_GE(inInner->stack.size(), 2u);
    EXPECT_TRUE(Has(inInner->stack[0], "inner:3")) << inInner->stack[0];
    EXPECT_TRUE(Has(inInner->stack[1], "6")) << inInner->stack[1];
}

// ---------------------------------------------------------------- Errors

TEST(LuaScript, AMissingEndIsReportedWithItsLine)
{
    LuaScript script("@bad.lua", "function OnUpdate(dt)\n  move(1, 0, 0)\n", LuaHost{});
    EXPECT_TRUE(Has(script.Error(), "bad.lua:")) << script.Error();
    EXPECT_TRUE(Has(script.Error(), "'end' expected")) << script.Error();
}

TEST(LuaScript, ASyntaxErrorIsReportedWithItsLine)
{
    LuaScript script("@bad.lua", "x = 1\ny = = 2\n", LuaHost{});
    EXPECT_TRUE(Has(script.Error(), "bad.lua:2:")) << script.Error();
}

TEST(LuaScript, AnUndefinedVariableIsReportedWithItsLineAndName)
{
    LuaScript calling("@bad.lua", "x = 1\nnothing_here()\n", LuaHost{});
    calling.Update(0.016);
    EXPECT_TRUE(Has(calling.Error(), "bad.lua:2:")) << calling.Error();
    EXPECT_TRUE(Has(calling.Error(), "nothing_here")) << calling.Error();

    LuaScript adding("@bad.lua", "x = 1\n\ny = missing + 1\n", LuaHost{});
    adding.Update(0.016);
    EXPECT_TRUE(Has(adding.Error(), "bad.lua:3:")) << adding.Error();
    EXPECT_TRUE(Has(adding.Error(), "missing")) << adding.Error();
}

TEST(LuaScript, TheWrongArgumentsAreReportedWithTheirLine)
{
    FakeGame game;
    LuaScript script("@bad.lua", "\nmove(1)\n", game.Host());
    script.Update(0.016);
    EXPECT_TRUE(Has(script.Error(), "bad.lua:2:")) << script.Error();
    EXPECT_TRUE(Has(script.Error(), "bad argument #2")) << script.Error();
    EXPECT_EQ(game.moved, (std::array<double, 3>{0.0, 0.0, 0.0}));
}

TEST(LuaScript, ALoopThatNeverEndsIsStopped)
{
    LuaScript script("@bad.lua", "while true do end\n", LuaHost{});
    script.Update(0.016);
    EXPECT_TRUE(Has(script.Error(), "budget")) << script.Error();
}

TEST(LuaScript, AScriptThatFailedStopsRunning)
{
    FakeGame game;
    LuaScript script("@bad.lua", "function OnUpdate(dt) log('tick') nothing() end\n", game.Host());
    script.Update(0.016);
    script.Update(0.016);
    EXPECT_EQ(game.logged.size(), 1u);
}

// ---------------------------------------------------------------- Hot reload

TEST(LuaScript, AReloadRunsTheNewCodeAndKeepsTheValues)
{
    FakeGame game;
    LuaScript script("@live.lua",
                     "score = 0\n"
                     "function OnInit() log('init') end\n"
                     "function OnUpdate(dt) score = score + 1 end\n",
                     game.Host());
    script.Update(0.016);
    script.Update(0.016);
    ASSERT_EQ(script.Variables().at("score").text, "2");

    std::string error;
    ASSERT_TRUE(script.Reload("score = 0\n"
                              "function OnUpdate(dt) score = score + 10 end\n",
                              error))
        << error;
    script.Update(0.016);

    EXPECT_EQ(script.Variables().at("score").text, "12") << "the value was kept, and the new code ran";
    EXPECT_EQ(std::count(game.logged.begin(), game.logged.end(), "init"), 1) << "OnInit does not run again";
}

TEST(LuaScript, AReloadThatDoesNotCompileKeepsTheOldCode)
{
    FakeGame game;
    LuaScript script("@live.lua", "function OnUpdate(dt) log('old') end\n", game.Host());
    script.Update(0.016);

    std::string error;
    EXPECT_FALSE(script.Reload("function OnUpdate(dt)\n", error));
    EXPECT_TRUE(Has(error, "'end' expected")) << error;
    EXPECT_TRUE(script.Error().empty()) << "the running script is fine";

    script.Update(0.016);
    EXPECT_EQ(game.logged, (std::vector<std::string>{"old", "old"}));
    EXPECT_TRUE(Has(script.Source(), "old"));
}

TEST(LuaScript, AReloadFixesAScriptThatFailed)
{
    FakeGame game;
    LuaScript script("@live.lua", "function OnUpdate(dt) nothing() end\n", game.Host());
    script.Update(0.016);
    ASSERT_FALSE(script.Error().empty());

    std::string error;
    ASSERT_TRUE(script.Reload("function OnUpdate(dt) log('fixed') end\n", error)) << error;
    script.Update(0.016);

    EXPECT_TRUE(script.Error().empty()) << script.Error();
    EXPECT_EQ(game.logged, std::vector<std::string>{"fixed"});
}

TEST(LuaScript, AReloadBeforeTheFirstUpdateStartsTheNewCode)
{
    FakeGame game;
    LuaScript script("@live.lua", "log('old')\n", game.Host());
    std::string error;
    ASSERT_TRUE(script.Reload("log('new')\n", error)) << error;
    script.Update(0.016);
    EXPECT_EQ(game.logged, std::vector<std::string>{"new"});
}

// ---------------------------------------------------------------- The compile cache

TEST(LuaCompileCache, TheSameScriptIsCompiledOnce)
{
    LuaCompileCache cache;
    FakeGame game;
    const std::string source = "function OnInit() log('hi') end\n";
    LuaScript first("@same.lua", source, game.Host(), {}, &cache);
    EXPECT_EQ(cache.Hits(), 0u);
    LuaScript second("@same.lua", source, game.Host(), {}, &cache);
    EXPECT_EQ(cache.Hits(), 1u);
    EXPECT_EQ(cache.Size(), 1u);

    second.Update(0.016);
    EXPECT_TRUE(second.Error().empty()) << second.Error();
    EXPECT_EQ(game.logged, std::vector<std::string>{"hi"}) << "what came from the cache runs";
}

TEST(LuaCompileCache, ChangedSourceIsNotTheOldEntry)
{
    LuaCompileCache cache;
    FakeGame game;
    LuaScript first("@same.lua", "log('one')\n", game.Host(), {}, &cache);
    LuaScript second("@same.lua", "log('two')\n", game.Host(), {}, &cache);
    EXPECT_EQ(cache.Hits(), 0u);
    EXPECT_EQ(cache.Size(), 2u);

    second.Update(0.016);
    EXPECT_EQ(game.logged, std::vector<std::string>{"two"});
}

TEST(LuaCompileCache, CachedScriptsKeepTheirLineNumbers)
{
    LuaCompileCache cache;
    const std::string source = "\n\nnothing()\n";
    LuaScript first("@same.lua", source, LuaHost{}, {}, &cache);
    LuaScript second("@same.lua", source, LuaHost{}, {}, &cache);
    second.Update(0.016);
    EXPECT_TRUE(Has(second.Error(), "same.lua:3:")) << second.Error();
}

TEST(LuaCompileCache, SourceThatDoesNotCompileIsNotKept)
{
    LuaCompileCache cache;
    LuaScript script("@bad.lua", "x = = 1\n", LuaHost{}, {}, &cache);
    EXPECT_EQ(cache.Size(), 0u);
}

// ---------------------------------------------------------------- require

TEST(LuaScript, RequireRunsAModuleOnceAndRemembersIt)
{
    FakeGame game;
    game.modules["helper"] = "log('loaded')\nreturn {two = function() return 2 end}\n";
    LuaScript script("@main.lua",
                     "local helper = require('helper')\n"
                     "local again = require('helper')\n"
                     "answer = helper.two() + again.two()\n",
                     game.Host());
    script.Update(0.016);

    ASSERT_TRUE(script.Error().empty()) << script.Error();
    EXPECT_EQ(script.Variables().at("answer").text, "4");
    EXPECT_EQ(game.logged, std::vector<std::string>{"loaded"});
    EXPECT_EQ(script.Dependencies(), std::vector<std::string>{"helper"});
}

TEST(LuaScript, AMissingModuleIsAnError)
{
    FakeGame game;
    LuaScript script("@main.lua", "require('gone')\n", game.Host());
    script.Update(0.016);
    EXPECT_TRUE(Has(script.Error(), "Script file not found: gone")) << script.Error();
    EXPECT_TRUE(script.Dependencies().empty());
}

TEST(LuaScript, AReloadReadsModulesAgain)
{
    FakeGame game;
    game.modules["helper"] = "return 1\n";
    LuaScript script("@main.lua", "value = require('helper')\n", game.Host());
    script.Update(0.016);
    ASSERT_EQ(script.Variables().at("value").text, "1");

    game.modules["helper"] = "return 2\n";
    std::string error;
    ASSERT_TRUE(script.Reload("local fresh = require('helper')\nlog(fresh)\n", error)) << error;
    EXPECT_EQ(game.logged, std::vector<std::string>{"2"});
}

// ---------------------------------------------------------------- Script files

namespace
{
    class ScriptFileTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            root_ = std::filesystem::temp_directory_path() / "atelier_lua_tests";
            std::filesystem::remove_all(root_);
            std::filesystem::create_directories(root_ / Engine::Scripting::kScriptsFolder);
            std::ofstream(root_ / Engine::Scripting::kScriptsFolder / "enemy.lua", std::ios::binary) << "return 1\n";
        }
        void TearDown() override { std::filesystem::remove_all(root_); }

        std::filesystem::path root_;
    };
}

TEST_F(ScriptFileTest, ScriptsAreFoundInTheGameOrItsScriptsFolder)
{
    EXPECT_EQ(Engine::Scripting::ReadScriptSource(root_, "assets/scripts/enemy.lua").text, "return 1\n");
    EXPECT_EQ(Engine::Scripting::ReadScriptSource(root_, "enemy.lua").text, "return 1\n");
    EXPECT_FALSE(Engine::Scripting::ReadScriptSource(root_, "gone.lua").error.empty());
}

TEST_F(ScriptFileTest, RequireReadsTheScriptsFolderAndNothingOutsideIt)
{
    EXPECT_EQ(Engine::Scripting::ReadRequiredScript(root_, "enemy").text, "return 1\n");
    for (const char* name : {"../secret", "..\\secret", "/etc/passwd", "C:/secret", ""})
    {
        EXPECT_TRUE(Has(Engine::Scripting::ReadRequiredScript(root_, name).error, "not allowed")) << name;
    }
}

// ---------------------------------------------------------------- Stopping at lines

TEST(LuaStepping, ABreakpointStopsBeforeItsLineAndContinueRunsOn)
{
    FakeGame game;
    int stoppedAt = 0;
    LuaHost host = game.Host();
    host.paused = [&stoppedAt](int line) { stoppedAt = line; };
    LuaScript script("@step.lua",
                     "log('one')\n"
                     "log('two')\n"
                     "log('three')\n",
                     host);
    script.ToggleLineBreakpoint(2);

    script.Update(0.016);

    EXPECT_TRUE(script.Paused());
    EXPECT_EQ(script.PausedLine(), 2);
    EXPECT_EQ(stoppedAt, 2) << "the game is told, so it can stop too";
    EXPECT_EQ(game.logged, std::vector<std::string>{"one"}) << "line 2 has not run";

    script.Continue();

    EXPECT_FALSE(script.Paused());
    EXPECT_EQ(game.logged, (std::vector<std::string>{"one", "two", "three"}));
}

TEST(LuaStepping, StepLineRunsOneLineAtATime)
{
    FakeGame game;
    LuaScript script("@step.lua",
                     "x = 1\n"
                     "x = x + 1\n"
                     "x = x + 1\n"
                     "log('end')\n",
                     game.Host());
    script.PauseNextLine();
    script.Update(0.016);
    ASSERT_TRUE(script.Paused());
    EXPECT_EQ(script.PausedLine(), 1);

    script.StepLine();
    EXPECT_EQ(script.PausedLine(), 2);
    EXPECT_EQ(script.Variables().at("x").text, "1");

    script.StepLine();
    EXPECT_EQ(script.PausedLine(), 3);
    EXPECT_EQ(script.Variables().at("x").text, "2") << "the variables are up to date at each stop";

    script.Continue();
    EXPECT_FALSE(script.Paused());
    EXPECT_EQ(script.Variables().at("x").text, "3");
    EXPECT_EQ(game.logged, std::vector<std::string>{"end"});
}

TEST(LuaStepping, ItStopsInsideFunctionsAndShowsTheCallStack)
{
    FakeGame game;
    LuaScript script("@step.lua",
                     "local function inner(n)\n"
                     "  local doubled = n * 2\n"
                     "  return doubled\n"
                     "end\n"
                     "function OnUpdate(dt)\n"
                     "  log(inner(4))\n"
                     "end\n",
                     game.Host());
    script.ToggleLineBreakpoint(3);
    script.Update(0.016);

    ASSERT_TRUE(script.Paused());
    ASSERT_FALSE(script.Trace().empty());
    EXPECT_EQ(script.Trace().back().variables.at("doubled"), "8");
    ASSERT_GE(script.Trace().back().stack.size(), 2u);
    EXPECT_TRUE(Has(script.Trace().back().stack[0], "inner:3")) << script.Trace().back().stack[0];

    script.Continue();
    EXPECT_EQ(game.logged, std::vector<std::string>{"8"});
}

TEST(LuaStepping, AStoppedScriptGoesOnWithTheNextUpdate)
{
    FakeGame game;
    LuaScript script("@step.lua",
                     "function OnUpdate(dt)\n"
                     "  log('a')\n"
                     "  log('b')\n"
                     "end\n",
                     game.Host());
    script.ToggleLineBreakpoint(3);
    script.Update(0.016);
    ASSERT_TRUE(script.Paused());
    ASSERT_EQ(game.logged, std::vector<std::string>{"a"});
    script.ToggleLineBreakpoint(3);

    script.Update(0.016); // finishes the call it stopped in
    EXPECT_FALSE(script.Paused());
    EXPECT_EQ(game.logged, (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(script.Calls(), 1u);

    script.Update(0.016); // and the next one starts as usual
    EXPECT_EQ(game.logged, (std::vector<std::string>{"a", "b", "a", "b"}));
    EXPECT_EQ(script.Calls(), 2u);
}

TEST(LuaStepping, StoppingInTheTopLevelStillRunsOnInitAfterwards)
{
    FakeGame game;
    LuaScript script("@step.lua",
                     "function OnInit() log('init') end\n"
                     "log('top')\n",
                     game.Host());
    script.ToggleLineBreakpoint(2);
    script.Update(0.016);
    ASSERT_TRUE(script.Paused());

    script.Continue();

    EXPECT_EQ(game.logged, (std::vector<std::string>{"top", "init"}));
}

TEST(LuaStepping, ACalculationThatCannotStopRunsThrough)
{
    LuaScript script("@step.lua",
                     "items = {3, 1, 2}\n"
                     "table.sort(items, function(a, b)\n"
                     "  return a < b\n"
                     "end)\n"
                     "first = items[1]\n",
                     LuaHost{});
    script.ToggleLineBreakpoint(3);
    script.Update(0.016);

    EXPECT_FALSE(script.Paused()) << "a sort cannot be left half done";
    EXPECT_TRUE(script.Error().empty()) << script.Error();
    EXPECT_EQ(script.Variables().at("first").text, "1");
}

TEST(LuaStepping, AReloadWhileStoppedDropsTheStoppedCall)
{
    FakeGame game;
    LuaScript script("@step.lua", "function OnUpdate(dt)\n  log('old')\nend\n", game.Host());
    script.ToggleLineBreakpoint(2);
    script.Update(0.016);
    ASSERT_TRUE(script.Paused());

    std::string error;
    ASSERT_TRUE(script.Reload("function OnUpdate(dt)\n  log('new')\nend\n", error)) << error;
    EXPECT_FALSE(script.Paused());
    script.ToggleLineBreakpoint(2);

    script.Update(0.016);
    EXPECT_EQ(game.logged, std::vector<std::string>{"new"});
}

TEST(LuaStepping, OnDestroyRunsEvenWhenStoppedAndNeverStops)
{
    FakeGame game;
    LuaScript script("@step.lua",
                     "function OnUpdate(dt)\n"
                     "  log('tick')\n"
                     "end\n"
                     "function OnDestroy()\n"
                     "  log('bye')\n"
                     "end\n",
                     game.Host());
    script.ToggleLineBreakpoint(2);
    script.ToggleLineBreakpoint(5);
    script.Update(0.016);
    ASSERT_TRUE(script.Paused());

    script.Destroy();

    EXPECT_FALSE(script.Paused());
    EXPECT_EQ(game.logged, std::vector<std::string>{"bye"});
}

TEST(LuaStepping, TheInstructionBudgetStillStopsARunawayLoop)
{
    LuaScript script("@step.lua", "x = 0\nwhile true do x = x + 1 end\n", LuaHost{});
    script.PauseNextLine();
    script.Update(0.016);
    ASSERT_TRUE(script.Paused());
    script.Continue();
    EXPECT_TRUE(Has(script.Error(), "budget")) << script.Error();
}
