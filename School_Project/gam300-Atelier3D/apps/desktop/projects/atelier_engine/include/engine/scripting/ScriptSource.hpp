#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace Engine::Scripting
{
    // Where a game keeps its .lua files, relative to the game folder. Shared scripts go here too, one file each:
    // require("enemy") reads assets/scripts/enemy.lua.
    inline constexpr const char* kScriptsFolder = "assets/scripts";

    struct ScriptSource
    {
        std::string text;
        std::string error;
    };

    // The file a script names: relative to the game folder, or just its name in the scripts folder
    inline std::filesystem::path ScriptPath(const std::filesystem::path& root, const std::string& file)
    {
        std::error_code ec;
        const auto path = root / std::filesystem::path(file);
        if (std::filesystem::is_regular_file(path, ec))
        {
            return path;
        }
        return root / kScriptsFolder / std::filesystem::path(file);
    }

    inline ScriptSource ReadScriptSource(const std::filesystem::path& root, const std::string& file)
    {
        std::error_code ec;
        const auto path = ScriptPath(root, file);

        if (!std::filesystem::is_regular_file(path, ec))
        {
            return {{}, "Script file not found: " + file};
        }

        std::ifstream input(path, std::ios::binary);

        if (!input)
        {
            return {{}, "Script file could not be read: " + file};
        }

        std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};

        if (input.bad())
        {
            return {{}, "Script file could not be read: " + file};
        }

        return {std::move(text), {}};
    }

    // What require(name) reads: a module of the scripts folder. A name cannot lead out of it.
    inline ScriptSource ReadRequiredScript(const std::filesystem::path& root, const std::string& name)
    {
        if (name.empty() || name.find("..") != std::string::npos || name.find(':') != std::string::npos ||
            name.front() == '/' || name.front() == '\\')
        {
            return {{}, "Script name not allowed: " + name};
        }
        return ReadScriptSource(root, std::string(kScriptsFolder) + "/" + name + ".lua");
    }
}
