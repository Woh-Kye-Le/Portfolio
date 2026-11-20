/* !
@file    	    Library.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		01/11/2026
@brief
    Implements the block library: saving a stack as a library block, putting
    its blocks back, renaming, and the library's file.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/logic/Library.hpp"

#include "../scene/GameObjectJson.hpp"
#include "engine/core/Log.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <iterator>

namespace Engine::Logic
{
    namespace
    {
        constexpr int kLibraryFormatVersion = 1;

        // Calls visit(block) for every block of blocks and inside them
        template <class Visitor> void VisitBlocks(std::vector<Block>& blocks, const Visitor& visit)
        {
            for (Block& block : blocks)
            {
                if (block.id != 0)
                {
                    visit(block);
                }
                for (std::vector<Block>* inner : {&block.inputs, &block.slots, &block.body, &block.elseBody})
                {
                    VisitBlocks(*inner, visit);
                }
            }
        }
    }

    const LibraryBlock* Library::Find(std::string_view name) const
    {
        const auto found = std::find_if(blocks.begin(), blocks.end(),
                                        [name](const LibraryBlock& block) { return block.name == name; });
        return found != blocks.end() ? &*found : nullptr;
    }

    bool IsValidLibraryName(std::string_view name)
    {
        const bool blank = std::all_of(name.begin(), name.end(),
                                       [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; });
        return !name.empty() && name.size() <= 48 && !blank;
    }

    Block MakeCall(std::string name, std::uint32_t id)
    {
        Block block = MakeBlock(Op::Call, id);
        block.text = std::move(name);
        block.inputs.resize(block.numbers.size()); // what its inputs are comes from the library
        return block;
    }

    std::vector<const LibraryBlock*> LibraryBlocksMatching(const Library& library, std::string_view search)
    {
        const auto lower = [](std::string_view text)
        {
            std::string out(text);
            std::transform(out.begin(), out.end(), out.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return out;
        };
        const std::string wanted = lower(search);
        std::vector<const LibraryBlock*> found;
        for (const LibraryBlock& saved : library.blocks)
        {
            if (lower(saved.name).find(wanted) != std::string::npos)
            {
                found.push_back(&saved);
            }
        }
        std::sort(found.begin(), found.end(),
                  [&lower](const LibraryBlock* a, const LibraryBlock* b) { return lower(a->name) < lower(b->name); });
        return found;
    }

    std::vector<std::string> ParseInputs(std::string_view text)
    {
        std::vector<std::string> inputs;
        std::size_t at = 0;
        while (at <= text.size() && inputs.size() < std::tuple_size_v<decltype(Block::numbers)>)
        {
            const std::size_t comma = std::min(text.find(',', at), text.size());
            std::string_view word = text.substr(at, comma - at);
            while (!word.empty() && word.front() == ' ')
            {
                word.remove_prefix(1);
            }
            while (!word.empty() && word.back() == ' ')
            {
                word.remove_suffix(1);
            }
            if (!word.empty())
            {
                inputs.emplace_back(word);
            }
            at = comma + 1;
        }
        return inputs;
    }

    std::string InputsText(const std::vector<std::string>& inputs)
    {
        std::string text;
        for (const std::string& input : inputs)
        {
            text += (text.empty() ? "" : ", ") + input;
        }
        return text;
    }

    bool SaveToLibrary(Library& library, std::vector<Block>& list, std::size_t index, const std::string& name,
                       std::uint32_t callId)
    {
        if (!IsValidLibraryName(name) || index >= list.size())
        {
            return false;
        }
        const std::size_t from = InfoOf(list[index].op).shape == BlockShape::Hat ? index + 1 : index;
        if (from >= list.size() || InfoOf(list[from].op).shape == BlockShape::Condition ||
            InfoOf(list[from].op).shape == BlockShape::Value)
        {
            return false; // a hat with nothing under it, or not a stack
        }
        LibraryBlock saved{.name = name, .blocks = TakeStack(list, from)};
        std::uint32_t nextId = 1;
        Renumber(saved.blocks, nextId); // its own ids
        list.push_back(MakeCall(name, callId));

        const auto existing = std::find_if(library.blocks.begin(), library.blocks.end(),
                                           [&name](const LibraryBlock& block) { return block.name == name; });
        if (existing != library.blocks.end())
        {
            *existing = std::move(saved);
        }
        else
        {
            library.blocks.push_back(std::move(saved));
        }
        return true;
    }

    bool UnpackCall(const Library& library, std::vector<Block>& list, std::size_t index, std::uint32_t& nextId)
    {
        if (index >= list.size() || list[index].op != Op::Call)
        {
            return false;
        }
        const LibraryBlock* saved = library.Find(list[index].text);
        if (!saved)
        {
            return false;
        }
        std::vector<Block> copy = saved->blocks;
        Renumber(copy, nextId);
        list.erase(list.begin() + static_cast<std::ptrdiff_t>(index));
        list.insert(list.begin() + static_cast<std::ptrdiff_t>(index), std::make_move_iterator(copy.begin()),
                    std::make_move_iterator(copy.end()));
        return true;
    }

    bool RenameLibraryBlock(Library& library, std::string_view from, std::string_view to)
    {
        if (!IsValidLibraryName(to) || library.Find(to))
        {
            return false;
        }
        const auto found = std::find_if(library.blocks.begin(), library.blocks.end(),
                                        [from](const LibraryBlock& block) { return block.name == from; });
        if (found == library.blocks.end())
        {
            return false;
        }
        const std::string old(from); // from may point into the block being renamed
        found->name = std::string(to);
        for (LibraryBlock& block : library.blocks)
        {
            VisitBlocks(block.blocks,
                        [&old, to](Block& each)
                        {
                            if (each.op == Op::Call && each.text == old)
                            {
                                each.text = std::string(to);
                            }
                        });
        }
        return true;
    }

    void RenameCalls(LogicDesc& logic, std::string_view from, std::string_view to)
    {
        const std::string old(from);
        for (Script& script : logic.scripts)
        {
            VisitBlocks(script.blocks,
                        [&old, to](Block& block)
                        {
                            if (block.op == Op::Call && block.text == old)
                            {
                                block.text = std::string(to);
                            }
                        });
        }
    }

    std::size_t CountCalls(const LogicDesc& logic, std::string_view name)
    {
        std::size_t calls = 0;
        LogicDesc copy = logic; // VisitBlocks walks non-const lists
        for (Script& script : copy.scripts)
        {
            VisitBlocks(script.blocks,
                        [name, &calls](const Block& block)
                        {
                            if (block.op == Op::Call && block.text == name)
                            {
                                ++calls;
                            }
                        });
        }
        return calls;
    }

    Scene::SceneLoadReport LoadLibraryFile(const std::filesystem::path& path, Library& library)
    {
        Scene::SceneLoadReport report;
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
        {
            library = {};
            report.ok = true; // no library yet
            return report;
        }
        try
        {
            Serializer::JSONSerializer json;
            json.LoadFile(path.string());
            if (!json.GetCurrentNode().is_object() || !json.HasArray("blocks"))
            {
                report.error = "the file must contain { \"version\": 1, \"blocks\": [ ... ] }";
            }
            else
            {
                Library loaded;
                json.EnterArray("blocks");
                const std::size_t count = json.GetCurrentNode().size();
                for (std::size_t i = 0; i < count; ++i)
                {
                    json.EnterArrayElement(i);
                    const std::string where = std::format("library block {}", i);
                    std::string name;
                    if (json.GetCurrentNode().is_object() && json.TryReadString("name", name) &&
                        IsValidLibraryName(name) && !loaded.Find(name))
                    {
                        std::string inputs;
                        json.TryReadString("inputs", inputs);
                        loaded.blocks.push_back({.name = name,
                                                 .blocks = Scene::Detail::ReadBlockList(json, "blocks", report, where),
                                                 .inputs = ParseInputs(inputs)});
                    }
                    else
                    {
                        report.warnings.push_back(where + ": no name, or a name already used - left out");
                    }
                    json.LeaveObject();
                }
                json.LeaveArray();
                library = std::move(loaded);
                report.ok = true;
            }
        }
        catch (const std::exception& e)
        {
            report.ok = false;
            report.error = e.what();
        }
        if (!report.ok)
        {
            LOG_ERROR(IO, "Block library {}: {}", path.string(), report.error);
        }
        for (const std::string& warning : report.warnings)
        {
            LOG_WARN(IO, "Block library {}: {}", path.string(), warning);
        }
        return report;
    }

    void SaveLibraryFile(const std::filesystem::path& path, const Library& library)
    {
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }
        Serializer::JSONSerializer json;
        json.ParseJSON("{}");
        json.WriteInt("version", kLibraryFormatVersion);
        json.CreateArray("blocks");
        json.EnterArray("blocks");
        for (std::size_t i = 0; i < library.blocks.size(); ++i)
        {
            json.CreateObject();
            json.EnterArrayElement(i);
            json.WriteString("name", library.blocks[i].name);
            if (!library.blocks[i].inputs.empty())
            {
                json.WriteString("inputs", InputsText(library.blocks[i].inputs));
            }
            Scene::Detail::WriteBlockList(json, "blocks", library.blocks[i].blocks);
            json.LeaveObject();
        }
        json.LeaveArray();
        json.SaveFile(path.string()); // throws std::runtime_error if it cannot be written
    }
}
