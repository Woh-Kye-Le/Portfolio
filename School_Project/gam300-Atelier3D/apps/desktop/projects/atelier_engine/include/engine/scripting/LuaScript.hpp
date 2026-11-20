#pragma once
#include "engine/scripting/ScriptSource.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <map>

#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

struct lua_State;
struct lua_Debug;
namespace Engine::Scripting
{
    struct LuaTrace
    {
        int line = 0;
        std::map<std::string, std::string> variables;
        std::vector<std::string> stack; // the functions it was in, innermost first ("OnUpdate:12")
    };
    // A variable as the debugger shows it: tables are written out a few levels deep
    struct LuaValue
    {
        std::string type;
        std::string text;

        bool operator==(const LuaValue&) const = default;
    };
    // What a script can do in the game. Each function is optional: a script calling one that is not there does nothing
    // (position gives 0, 0, 0; require fails).
    struct LuaHost
    {
        std::function<void(double, double, double)> move;               // move(x, y, z): by this much
        std::function<std::optional<std::array<double, 3>>()> position; // x, y, z = position()
        std::function<void(const std::string&)> playSound;              // play_sound(name)
        std::function<void(const std::string&, bool)> animate;          // animate(clip [, loop])
        std::function<void(const std::string&, double)> send;           // send(message [, value])
        std::function<void(const std::string&)> log;                    // log(text)
        std::function<ScriptSource(const std::string&)> require;        // what require(name) reads
        std::function<void(int)> paused;                                // it stopped before this line
    };
    // Compiled scripts, by name and source: objects with the same script compile it once, and a changed source is a
    // different entry, so nothing old is ever run.
    class LuaCompileCache
    {
    public:
        const std::string* Find(const std::string& key);
        void Store(std::string key, std::string bytecode);

        std::size_t Size() const { return bytecode_.size(); }
        std::size_t Hits() const { return hits_; }

    private:
        std::unordered_map<std::string, std::string> bytecode_;
        std::size_t hits_ = 0;
    };
    // One state per object: globals never leak between scripts. All calls stay on the game thread.
    //
    // A script runs its top level once on the first Update, then OnInit() if it has one. Each Update then calls
    // OnUpdate(dt), or update(dt) in scripts written before it had that name. Destroy calls OnDestroy().
    //
    // Every call runs in a coroutine, so a script can stop part way through a line at a time: before a line with a
    // breakpoint, or the next line after PauseNextLine or StepLine. While stopped Update carries the call on, and
    // StepLine and Continue do too. What cannot be left half done runs through without stopping: a table.sort
    // comparison, a module's top level during require, a reload and OnDestroy.
    class LuaScript
    {
    public:
        LuaScript(std::string name, std::string source, LuaHost host, std::string initialError = {},
                  LuaCompileCache* cache = nullptr);
        ~LuaScript();
        LuaScript(const LuaScript&) = delete;
        LuaScript& operator=(const LuaScript&) = delete;
        void Update(double dt);
        // Calls OnDestroy() once, if the script started and has not failed
        void Destroy();
        // New code in place of the old, keeping the values of its global variables. OnInit does not run again. Source
        // that does not compile changes nothing: false, with the reason in error, and the old code goes on running.
        bool Reload(std::string source, std::string& error);
        // Stops before the next line that runs
        void PauseNextLine() { stepping_ = true; }
        // While stopped: runs the line it stopped at, and stops before the next
        void StepLine();
        // While stopped: runs on, up to the next breakpoint
        void Continue();
        void ToggleLineBreakpoint(int line);
        bool HasLineBreakpoint(int line) const { return lineBreakpoints_.contains(line); }
        bool Paused() const { return suspended_; }
        int PausedLine() const { return suspended_ ? pausedLine_ : 0; }
        const std::string& Name() const { return name_; }
        const std::string& Source() const { return source_; }
        const std::string& Error() const { return error_; }
        const std::vector<LuaTrace>& Trace() const { return trace_; }
        const std::map<std::string, LuaValue>& Variables() const { return variables_; }
        // The names it has passed to require
        const std::vector<std::string>& Dependencies() const { return dependencies_; }
        std::uint64_t Calls() const { return calls_; }

        bool Initialized() const { return initialized_; }

    private:
        static void Hook(lua_State*, lua_Debug*);
        static int MoveObject(lua_State*);
        static int Position(lua_State*);
        static int PlaySound(lua_State*);
        static int Animate(lua_State*);
        static int Send(lua_State*);
        static int Log(lua_State*);
        static int Require(lua_State*);
        static std::string Value(lua_State*, int, int depth = 0);
        bool LoadRequired(lua_State* state, const char* name);
        // Leaves the compiled chunk on the stack; false (nothing on it) with the reason in error
        bool Compile(const std::string& source, std::string& error);
        void Globals();
        bool Call(int args);
        bool CallGlobal(const char* function);
        bool CallUpdate(double dt);
        bool Resume(int args);
        void EndCall();
        // What Update does next: the top level and OnInit once, then OnUpdate each time
        enum class Stage : std::uint8_t
        {
            TopLevel,
            Init,
            Tick,
            Done,
        };
        lua_State* state_ = nullptr;
        lua_State* call_ = nullptr; // the coroutine of the call under way
        int callRef_ = -1;          // what keeps it from being collected
        Stage next_ = Stage::TopLevel;
        bool suspended_ = false;
        bool stepping_ = false;
        bool neverStop_ = false; // the call must finish: a reload or OnDestroy
        int pausedLine_ = 0;
        std::set<int> lineBreakpoints_;
        std::string name_, source_, error_;
        LuaHost host_;
        LuaCompileCache* cache_ = nullptr;
        std::vector<LuaTrace> trace_;
        std::map<std::string, LuaValue> variables_;
        std::vector<std::string> dependencies_;
        int budget_ = 0;
        bool initialized_ = false;
        bool destroyed_ = false;
        std::uint64_t calls_ = 0;
    };
}
