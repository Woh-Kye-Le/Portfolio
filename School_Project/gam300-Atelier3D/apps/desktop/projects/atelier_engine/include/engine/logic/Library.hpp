/* !
@file    	    Library.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		01/11/2026
@brief
    Declares the block library: long stacks of blocks saved under a name, so
    a script can use the whole stack as one block ("when Play starts >
    Movement Logic"), and its file in the game folder.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/logic/Blocks.hpp"
#include "engine/scene/SceneFile.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Logic
{
    // A stack of blocks saved under a name. Its blocks have ids of their own (from 1); a script runs them with a
    // library block (Op::Call) whose text is the name.
    struct LibraryBlock
    {
        std::string name;
        std::vector<Block> blocks;
        // The numbers it takes where it is used ("damage", "range"): its blocks read them as variables
        std::vector<std::string> inputs;

        bool operator==(const LibraryBlock&) const = default;
    };

    // A game's library: every object in every scene of the game can use its blocks
    struct Library
    {
        std::vector<LibraryBlock> blocks;

        const LibraryBlock* Find(std::string_view name) const;
        bool operator==(const Library&) const = default;
    };

    // Where a game keeps its library, relative to the game folder
    inline constexpr const char* kLibraryFile = "data/library.blocks.json";

    // Whether name can be a library block's name: 1-48 characters, not blank
    bool IsValidLibraryName(std::string_view name);

    // "damage, range" as its names, without the blanks; the empty ones left out
    std::vector<std::string> ParseInputs(std::string_view text);
    // The other way: "damage, range"
    std::string InputsText(const std::vector<std::string>& inputs);

    // The library's blocks whose names hold search (any case; empty: all of them), in name order, for a list of them
    std::vector<const LibraryBlock*> LibraryBlocksMatching(const Library& library, std::string_view search);

    // A block that runs the library block called name
    Block MakeCall(std::string name, std::uint32_t id);

    // Saves the blocks of list from index down (from the block after it, when it is a hat) to the library as name,
    // replacing a library block of that name, and puts one block that runs them (with id callId) in their place:
    // "when Play starts > move > turn > ..." becomes "when Play starts > name". Returns false, changing nothing, if
    // the name cannot be used or there are no blocks to save there.
    bool SaveToLibrary(Library& library, std::vector<Block>& list, std::size_t index, const std::string& name,
                       std::uint32_t callId);

    // Puts the blocks the library block at index of list runs in its place (a copy, with ids from nextId), so they can
    // be changed. Returns false if that block does not run one the library has.
    bool UnpackCall(const Library& library, std::vector<Block>& list, std::size_t index, std::uint32_t& nextId);

    // Renames a library block. Returns false if there is none called from, or to cannot be used (or is taken).
    bool RenameLibraryBlock(Library& library, std::string_view from, std::string_view to);
    // Makes every block of logic that runs the library block from run to instead
    void RenameCalls(LogicDesc& logic, std::string_view from, std::string_view to);
    // How many blocks of logic run the library block called name
    std::size_t CountCalls(const LogicDesc& logic, std::string_view name);

    // Reads a library file: { "version": 1, "blocks": [ { "name": "Movement Logic", "blocks": [ ... ] } ] }. A missing
    // file is an empty library (ok). Forgiving like a scene file: bad entries are left out with a warning.
    Scene::SceneLoadReport LoadLibraryFile(const std::filesystem::path& path, Library& library);
    // Writes it, making its folder if needed. Throws std::runtime_error if it cannot be written.
    void SaveLibraryFile(const std::filesystem::path& path, const Library& library);
}
