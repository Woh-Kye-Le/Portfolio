#pragma once

#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace Engine::Logic
{
    using Breakpoints = std::map<std::uint32_t, std::set<std::uint32_t>>; // object id -> block ids

    // What a breakpoint needs to stop: switched off it does nothing, and with a condition it only stops when that
    // holds. Blocks with no rule stop always.
    struct BreakpointRule
    {
        bool enabled = true;
        std::string condition; // "Speed > 5": a variable of the object, a comparison, a number or true / false

        bool operator==(const BreakpointRule&) const = default;
    };
    using BreakpointRules = std::map<std::pair<std::uint32_t, std::uint32_t>, BreakpointRule>; // object, block

    struct BreakpointCondition
    {
        std::string variable;
        std::string comparison; // >=, <=, !=, ==, > or <
        double value = 0.0;

        bool Holds(double current) const
        {
            return comparison == ">"    ? current > value
                   : comparison == "<"  ? current < value
                   : comparison == ">=" ? current >= value
                   : comparison == "<=" ? current <= value
                   : comparison == "!=" ? current != value
                                        : current == value;
        }
    };

    // nullopt if it is not written as "variable comparison value" ("=" is taken as "==")
    inline std::optional<BreakpointCondition> ParseBreakpointCondition(std::string_view text)
    {
        const auto trim = [](std::string_view part)
        {
            const auto first = part.find_first_not_of(" \t");
            return first == std::string_view::npos ? std::string_view{}
                                                   : part.substr(first, part.find_last_not_of(" \t") - first + 1);
        };
        for (const std::string_view comparison : std::array<std::string_view, 7>{">=", "<=", "!=", "==", ">", "<", "="})
        {
            const auto at = text.find(comparison);
            if (at == std::string_view::npos)
            {
                continue;
            }
            const std::string_view variable = trim(text.substr(0, at));
            const std::string_view value = trim(text.substr(at + comparison.size()));
            BreakpointCondition condition{std::string(variable), comparison == "=" ? "==" : std::string(comparison)};
            if (variable.empty() || value.empty())
            {
                return std::nullopt;
            }
            if (value == "true" || value == "false")
            {
                condition.value = value == "true" ? 1.0 : 0.0;
                return condition;
            }
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), condition.value);
            if (error != std::errc{} || end != value.data() + value.size())
            {
                return std::nullopt;
            }
            return condition;
        }
        return std::nullopt;
    }

    // A scene's breakpoints between sessions: a line of "object block enabled condition" each. An editor's own data: a
    // game plays the same without it. None left: the file goes.
    inline void SaveBreakpoints(const std::filesystem::path& path, const Breakpoints& breakpoints,
                                const BreakpointRules& rules = {})
    {
        std::error_code ec;
        if (breakpoints.empty())
        {
            std::filesystem::remove(path, ec);
            return;
        }
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream out(path, std::ios::binary);
        for (const auto& [object, blocks] : breakpoints)
        {
            for (const std::uint32_t block : blocks)
            {
                const auto rule = rules.find({object, block});
                const bool enabled = rule == rules.end() || rule->second.enabled;
                out << object << ' ' << block << ' ' << (enabled ? 1 : 0);
                if (rule != rules.end() && !rule->second.condition.empty())
                {
                    out << ' ' << rule->second.condition;
                }
                out << '\n';
            }
        }
    }

    // Nothing, for a file that is missing; stops at the first line it cannot read
    inline Breakpoints LoadBreakpoints(const std::filesystem::path& path, BreakpointRules* rules = nullptr)
    {
        Breakpoints breakpoints;
        std::ifstream in(path, std::ios::binary);
        std::string text;
        while (std::getline(in, text))
        {
            std::istringstream line(text);
            std::uint32_t object = 0;
            std::uint32_t block = 0;
            if (!(line >> object >> block))
            {
                break;
            }
            breakpoints[object].insert(block);
            int enabled = 1;
            line >> enabled; // files of older builds stop at the block
            std::string condition;
            std::getline(line >> std::ws, condition);
            if (rules != nullptr && (enabled == 0 || !condition.empty()))
            {
                (*rules)[{object, block}] = BreakpointRule{enabled != 0, condition};
            }
        }
        return breakpoints;
    }
}
