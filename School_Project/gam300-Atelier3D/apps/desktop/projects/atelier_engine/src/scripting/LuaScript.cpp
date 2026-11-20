#include "engine/scripting/LuaScript.hpp"
extern "C"
{
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <utility>

namespace Engine::Scripting
{
    namespace
    {
        // Where the modules a script has required are kept, in the registry
        constexpr const char* kLoaded = "atelier_loaded";
        constexpr std::size_t kMaxTableEntries = 16;
        constexpr int kMaxTableDepth = 2;
        constexpr int kMaxStackFrames = 8;

        bool IsLibrary(std::string_view name)
        {
            constexpr std::array<std::string_view, 5> kNames{"_G", "_VERSION", "math", "string", "table"};
            return std::find(kNames.begin(), kNames.end(), name) != kNames.end();
        }

        // Whether the key and value on top of the stack are a global variable of the script
        bool IsScriptVariable(lua_State* state)
        {
            const int type = lua_type(state, -1);
            return lua_type(state, -2) == LUA_TSTRING && !IsLibrary(lua_tostring(state, -2)) &&
                   (type == LUA_TSTRING || type == LUA_TNUMBER || type == LUA_TBOOLEAN || type == LUA_TTABLE);
        }

        int WriteChunk(lua_State*, const void* data, std::size_t size, void* out)
        {
            static_cast<std::string*>(out)->append(static_cast<const char*>(data), size);
            return 0;
        }

        // A key of a compiled script: its name is in the bytecode, for the messages
        std::string CacheKey(const std::string& name, const std::string& source)
        {
            std::string key = name;
            key.push_back('\0');
            key += source;
            return key;
        }

        LuaScript*& Self(lua_State* state)
        {
            void* space = lua_getextraspace(state);
            return *static_cast<LuaScript**>(space);
        }
    }

    const std::string* LuaCompileCache::Find(const std::string& key)
    {
        const auto found = bytecode_.find(key);
        if (found == bytecode_.end())
        {
            return nullptr;
        }
        ++hits_;
        return &found->second;
    }

    void LuaCompileCache::Store(std::string key, std::string bytecode)
    {
        bytecode_.insert_or_assign(std::move(key), std::move(bytecode));
    }

    LuaScript::LuaScript(std::string name, std::string source, LuaHost host, std::string initialError,
                         LuaCompileCache* cache)
        : name_(std::move(name)), source_(std::move(source)), host_(std::move(host)), cache_(cache)
    {
        if (!initialError.empty())
        {
            error_ = std::move(initialError);
            return;
        }

        state_ = luaL_newstate();
        if (!state_)
        {
            error_ = "Could not create Lua VM";
            return;
        }
        Self(state_) = this;
        // No filesystem, process, package, coroutine or debug library. Hooks belong to the engine.
        luaL_requiref(state_, "_G", luaopen_base, 1);
        lua_pop(state_, 1);
        luaL_requiref(state_, LUA_MATHLIBNAME, luaopen_math, 1);
        lua_pop(state_, 1);
        luaL_requiref(state_, LUA_STRLIBNAME, luaopen_string, 1);
        lua_pop(state_, 1);
        luaL_requiref(state_, LUA_TABLIBNAME, luaopen_table, 1);
        lua_pop(state_, 1);
        for (const char* key : {"dofile", "loadfile", "load", "pcall", "xpcall", "collectgarbage", "print"})
        {
            lua_pushnil(state_);
            lua_setglobal(state_, key);
        }
        lua_pushcfunction(state_, MoveObject);
        lua_setglobal(state_, "move");
        lua_pushcfunction(state_, Position);
        lua_setglobal(state_, "position");
        lua_pushcfunction(state_, PlaySound);
        lua_setglobal(state_, "play_sound");
        lua_pushcfunction(state_, Animate);
        lua_setglobal(state_, "animate");
        lua_pushcfunction(state_, Send);
        lua_setglobal(state_, "send");
        lua_pushcfunction(state_, Log);
        lua_setglobal(state_, "log");
        lua_pushcfunction(state_, Require);
        lua_setglobal(state_, "require");
        // Loading compiles text to a Lua function (bytecode), but does not execute it in edit mode.
        Compile(source_, error_);
    }
    LuaScript::~LuaScript()
    {
        if (state_)
            lua_close(state_);
    }
    bool LuaScript::Compile(const std::string& source, std::string& error)
    {
        const std::string key = cache_ ? CacheKey(name_, source) : std::string();
        if (const std::string* bytecode = cache_ ? cache_->Find(key) : nullptr)
        {
            if (luaL_loadbufferx(state_, bytecode->data(), bytecode->size(), name_.c_str(), "b") == LUA_OK)
            {
                return true;
            }
            lua_pop(state_, 1); // written by another Lua build: compiled again below
        }
        if (luaL_loadbufferx(state_, source.data(), source.size(), name_.c_str(), "t") != LUA_OK)
        {
            error = Value(state_, -1);
            lua_pop(state_, 1);
            return false;
        }
        if (cache_)
        {
            std::string bytecode;
            if (lua_dump(state_, WriteChunk, &bytecode, 0) == 0)
            {
                cache_->Store(key, std::move(bytecode));
            }
        }
        return true;
    }
    std::string LuaScript::Value(lua_State* state, int index, int depth)
    {
        switch (lua_type(state, index))
        {
        case LUA_TSTRING:
        {
            std::size_t size = 0;
            const char* s = lua_tolstring(state, index, &size);
            return std::string(s, size > 256 ? 256 : size);
        }
        case LUA_TNUMBER:
            return lua_isinteger(state, index) ? std::to_string(lua_tointeger(state, index))
                                               : std::to_string(lua_tonumber(state, index));
        case LUA_TBOOLEAN: return lua_toboolean(state, index) ? "true" : "false";
        case LUA_TTABLE:
        {
            if (depth >= kMaxTableDepth || !lua_checkstack(state, 4))
            {
                return "{...}";
            }
            const int table = lua_absindex(state, index);
            std::string text = "{";
            std::size_t count = 0;
            lua_pushnil(state);
            while (lua_next(state, table))
            {
                if (count == kMaxTableEntries)
                {
                    text += ", ...";
                    lua_pop(state, 2);
                    break;
                }
                text += (count++ == 0 ? "" : ", ") + Value(state, -2, depth + 1) + " = " + Value(state, -1, depth + 1);
                lua_pop(state, 1);
            }
            return text + "}";
        }
        default: return lua_typename(state, lua_type(state, index));
        }
    }
    void LuaScript::Globals()
    {
        variables_.clear();
        lua_pushglobaltable(state_);
        lua_pushnil(state_);
        while (lua_next(state_, -2))
        {
            if (IsScriptVariable(state_) && variables_.size() < 64)
                variables_[lua_tostring(state_, -2)] = {luaL_typename(state_, -1), Value(state_, -1)};
            lua_pop(state_, 1);
        }
        lua_pop(state_, 1);
    }
    void LuaScript::Hook(lua_State* state, lua_Debug* debug)
    {
        auto* self = Self(state);
        if (debug->event == LUA_HOOKCOUNT)
        {
            if (--self->budget_ <= 0)
                luaL_error(state, "Lua instruction budget exceeded (100000 per call)");
            return;
        }
        // A hook reports the line about to run; its locals are the values on entry to that line.
        if (self->trace_.size() >= 128)
            self->trace_.erase(self->trace_.begin());
        LuaTrace entry;
        entry.line = debug->currentline;
        const char* name = nullptr;
        for (int i = 1; i <= 64 && (name = lua_getlocal(state, debug, i)); ++i)
        {
            if (name[0] != '(')
                entry.variables[name] = Value(state, -1);
            lua_pop(state, 1);
        }
        lua_Debug frame{};
        for (int level = 0; level < kMaxStackFrames && lua_getstack(state, level, &frame); ++level)
        {
            lua_getinfo(state, "nSl", &frame);
            entry.stack.push_back(std::string(frame.name ? frame.name : frame.what) + ":" +
                                  std::to_string(frame.currentline));
        }
        self->trace_.push_back(std::move(entry));

        const int line = debug->currentline;
        if (!self->neverStop_ && lua_isyieldable(state) && (self->stepping_ || self->lineBreakpoints_.contains(line)))
        {
            self->stepping_ = false;
            self->pausedLine_ = line;
            lua_yield(state, 0); // from a hook this returns, and Lua yields as the hook ends: nothing may follow it
        }
    }
    // These run inside Lua, whose errors jump out past C++ destructors: a std::string is gone before anything can fail
    int LuaScript::MoveObject(lua_State* state)
    {
        auto* self = Self(state);
        const double x = luaL_checknumber(state, 1), y = luaL_checknumber(state, 2), z = luaL_checknumber(state, 3);
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            return luaL_error(state, "move requires finite coordinates");
        if (self->host_.move)
            self->host_.move(x, y, z);
        return 0;
    }
    int LuaScript::Position(lua_State* state)
    {
        auto* self = Self(state);
        const auto at = self->host_.position ? self->host_.position() : std::nullopt;
        for (std::size_t i = 0; i < 3; ++i)
            lua_pushnumber(state, at ? (*at)[i] : 0.0);
        return 3;
    }
    int LuaScript::PlaySound(lua_State* state)
    {
        auto* self = Self(state);
        const char* sound = luaL_checkstring(state, 1);
        if (self->host_.playSound)
            self->host_.playSound(sound);
        return 0;
    }
    int LuaScript::Animate(lua_State* state)
    {
        auto* self = Self(state);
        const char* clip = luaL_checkstring(state, 1);
        const bool loop = lua_isnoneornil(state, 2) || lua_toboolean(state, 2);
        if (self->host_.animate)
            self->host_.animate(clip, loop);
        return 0;
    }
    int LuaScript::Send(lua_State* state)
    {
        auto* self = Self(state);
        const char* message = luaL_checkstring(state, 1);
        const double value = luaL_optnumber(state, 2, 0.0);
        if (self->host_.send)
            self->host_.send(message, value);
        return 0;
    }
    int LuaScript::Log(lua_State* state)
    {
        auto* self = Self(state);
        const char* text = luaL_tolstring(state, 1, nullptr);
        if (self->host_.log)
            self->host_.log(text);
        lua_pop(state, 1);
        return 0;
    }
    bool LuaScript::LoadRequired(lua_State* state, const char* name)
    {
        ScriptSource source;
        if (host_.require)
        {
            source = host_.require(name);
        }
        else
        {
            source.error = "This script cannot require other scripts";
        }
        if (!source.error.empty())
        {
            lua_pushlstring(state, source.error.data(), source.error.size());
            return false;
        }
        if (std::find(dependencies_.begin(), dependencies_.end(), name) == dependencies_.end())
        {
            dependencies_.emplace_back(name);
        }
        const std::string chunk = std::string("@") + name + ".lua";
        return luaL_loadbufferx(state, source.text.data(), source.text.size(), chunk.c_str(), "t") == LUA_OK;
    }
    int LuaScript::Require(lua_State* state)
    {
        auto* self = Self(state);
        const char* name = luaL_checkstring(state, 1);
        lua_settop(state, 1);
        if (lua_getfield(state, LUA_REGISTRYINDEX, kLoaded) != LUA_TTABLE)
        {
            lua_pop(state, 1);
            lua_newtable(state);
            lua_pushvalue(state, -1);
            lua_setfield(state, LUA_REGISTRYINDEX, kLoaded);
        }
        if (lua_getfield(state, 2, name) != LUA_TNIL)
            return 1; // run once: every require of it gets what it returned
        lua_pop(state, 1);
        if (!self->LoadRequired(state, name))
            return lua_error(state);
        lua_call(state, 0, 1);
        if (lua_isnil(state, -1))
        {
            lua_pop(state, 1);
            lua_pushboolean(state, 1);
        }
        lua_pushvalue(state, -1);
        lua_setfield(state, 2, name);
        return 1;
    }
    bool LuaScript::Call(int args)
    {
        lua_State* call = lua_newthread(state_);
        lua_insert(state_, -(args + 2)); // under the function and its arguments, which move across to it
        lua_xmove(state_, call, args + 1);
        callRef_ = luaL_ref(state_, LUA_REGISTRYINDEX);
        call_ = call;
        lua_sethook(call_, Hook, LUA_MASKLINE | LUA_MASKCOUNT, 1000);
        return Resume(args);
    }
    bool LuaScript::Resume(int args)
    {
        budget_ = 100; // time spent stopped is not the script's
        suspended_ = false;
        int results = 0;
        const int status = lua_resume(call_, state_, args, &results);
        if (status == LUA_YIELD)
        {
            suspended_ = true;
            Globals();
            if (host_.paused)
                host_.paused(pausedLine_);
            return true;
        }
        const bool ok = status == LUA_OK;
        if (!ok)
            error_ = Value(call_, -1);
        EndCall();
        if (!ok)
            Globals();
        return ok;
    }
    void LuaScript::EndCall()
    {
        luaL_unref(state_, LUA_REGISTRYINDEX, callRef_);
        callRef_ = LUA_NOREF;
        call_ = nullptr;
        suspended_ = false;
    }
    bool LuaScript::CallGlobal(const char* function)
    {
        lua_getglobal(state_, function);
        if (!lua_isfunction(state_, -1))
        {
            lua_pop(state_, 1);
            return true; // scripts only have the hooks they need
        }
        return Call(0);
    }
    bool LuaScript::CallUpdate(double dt)
    {
        lua_getglobal(state_, "OnUpdate");
        if (!lua_isfunction(state_, -1))
        {
            lua_pop(state_, 1);
            lua_getglobal(state_, "update");
        }
        if (lua_isfunction(state_, -1))
        {
            trace_.clear();
            lua_pushnumber(state_, dt);
            ++calls_;
            return Call(1);
        }
        lua_pop(state_, 1); // one-shot scripts need no update function
        return true;
    }
    void LuaScript::Update(double dt)
    {
        if (!error_.empty() || !state_)
            return;
        if (suspended_)
        {
            if (!Resume(0) || suspended_)
                return;
        }
        else if (next_ == Stage::Done)
        {
            next_ = Stage::Tick;
        }
        // The next stage is noted first: one that stops part way is finished by Resume above
        while (next_ != Stage::Done)
        {
            const Stage stage = next_;
            next_ = stage == Stage::TopLevel ? Stage::Init : stage == Stage::Init ? Stage::Tick : Stage::Done;
            bool ok = true;
            switch (stage)
            {
            case Stage::TopLevel:
                trace_.clear();
                ok = Call(0); // the compiled top-level chunk, once on first Play tick
                initialized_ = ok;
                break;
            case Stage::Init: ok = CallGlobal("OnInit"); break;
            case Stage::Tick: ok = CallUpdate(dt); break;
            case Stage::Done: break;
            }
            if (!ok || suspended_)
                return;
        }
        Globals();
    }
    void LuaScript::StepLine()
    {
        stepping_ = true;
        if (suspended_)
            Update(0.0);
    }
    void LuaScript::Continue()
    {
        stepping_ = false;
        if (suspended_)
            Update(0.0);
    }
    void LuaScript::ToggleLineBreakpoint(int line)
    {
        if (lineBreakpoints_.erase(line) == 0)
            lineBreakpoints_.insert(line);
    }
    void LuaScript::Destroy()
    {
        if (state_ && initialized_ && !destroyed_ && error_.empty())
        {
            destroyed_ = true;
            if (call_)
                EndCall();
            neverStop_ = true;
            CallGlobal("OnDestroy");
            neverStop_ = false;
        }
    }
    bool LuaScript::Reload(std::string source, std::string& error)
    {
        if (!state_)
        {
            error = error_;
            return false;
        }
        if (call_)
        {
            EndCall(); // stopped part way: what was left of that call is gone with the old code
            next_ = Stage::Done;
        }
        lua_settop(state_, 0); // a script not started yet still has its old chunk here
        if (!Compile(source, error))
        {
            return false;
        }
        source_ = std::move(source);
        error_.clear();
        trace_.clear();

        lua_pushnil(state_);
        lua_setfield(state_, LUA_REGISTRYINDEX, kLoaded); // what it requires may have changed too
        if (!initialized_)
        {
            return true; // Update starts it as usual
        }

        // The values go aside, the new code runs, and they come back over what its top level set them to
        lua_newtable(state_);
        const int saved = lua_gettop(state_);
        lua_pushglobaltable(state_);
        lua_pushnil(state_);
        while (lua_next(state_, -2))
        {
            if (IsScriptVariable(state_))
            {
                lua_pushvalue(state_, -2);
                lua_pushvalue(state_, -2);
                lua_settable(state_, saved);
            }
            lua_pop(state_, 1);
        }
        lua_pop(state_, 1);
        lua_pushvalue(state_, saved - 1); // the chunk, under the saved values
        neverStop_ = true;
        const bool ran = Call(0);
        neverStop_ = false;
        if (!ran)
        {
            error = error_;
            lua_settop(state_, 0);
            return false;
        }
        lua_pushnil(state_);
        while (lua_next(state_, saved))
        {
            lua_setglobal(state_, lua_tostring(state_, -2)); // takes the value: the key stays for lua_next
        }
        lua_settop(state_, 0);
        Globals();
        return true;
    }
}
