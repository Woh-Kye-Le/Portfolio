/* !
@file    	    AssetCatalog.hpp
@author  	    j.cai@digipen.edu [100%]
@date    		25/10/2026
@brief
    The asset files of a game as the Assets browser lists them: every model,
    texture and audio clip under the game's assets/ folder, with its kind and
    file name.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/game/GameProject.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace Editor::Panels
{
    // What an asset file is, from its extension
    enum class AssetKind : std::uint8_t
    {
        Model,
        Texture,
        Audio,
        Material, // a .amat: how an object looks
        Shader,   // a game's own shader: its .vert (the .frag beside it goes with it)
    };

    inline constexpr std::array kAssetKinds{AssetKind::Model, AssetKind::Texture, AssetKind::Audio, AssetKind::Material,
                                            AssetKind::Shader};

    // The kinds the filter buttons let through, indexed by AssetKind. All false means no filter: every kind shows.
    using AssetKindFilter = std::array<bool, kAssetKinds.size()>;

    inline bool IsFiltering(const AssetKindFilter& filter)
    {
        return std::find(filter.begin(), filter.end(), true) != filter.end();
    }

    // Whether an asset of this kind shows with the filter (any kind when no button is on)
    inline bool PassesKindFilter(AssetKind kind, const AssetKindFilter& filter)
    {
        return !IsFiltering(filter) || filter[static_cast<std::size_t>(kind)];
    }

    // One file of the Assets browser
    struct AssetEntry
    {
        std::string path; // relative to the game folder with '/' separators, as a scene stores it
        std::string name; // file name with its extension (crate.obj and crate.png are different assets)
        AssetKind kind = AssetKind::Model;
    };

    inline const char* ToString(AssetKind kind)
    {
        switch (kind)
        {
        case AssetKind::Model: return "Model";
        case AssetKind::Texture: return "Texture";
        case AssetKind::Audio: return "Audio";
        case AssetKind::Material: return "Material";
        case AssetKind::Shader: return "Shader";
        }
        return "Asset";
    }

    inline std::string ToLowerAscii(std::string_view text)
    {
        std::string lower(text);
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lower;
    }

    // The kind of a file the engine can load, by extension (any case); nullopt for anything else (.json, .txt, ...)
    inline std::optional<AssetKind> KindOf(const std::filesystem::path& file)
    {
        const std::string extension = ToLowerAscii(file.extension().string());
        auto has = [&extension](std::span<const std::string_view> extensions)
        { return std::find(extensions.begin(), extensions.end(), extension) != extensions.end(); };
        if (has(Engine::Game::kMeshExtensions))
        {
            return AssetKind::Model;
        }
        if (has(Engine::Game::kTextureExtensions))
        {
            return AssetKind::Texture;
        }
        if (has(Engine::Game::kAudioExtensions))
        {
            return AssetKind::Audio;
        }
        if (has(Engine::Game::kMaterialExtensions))
        {
            return AssetKind::Material;
        }
        if (has(Engine::Game::kShaderExtensions))
        {
            return AssetKind::Shader;
        }
        return std::nullopt;
    }

    // Whether the asset's file name contains query, ignoring case and the spaces around query ("crate" finds crate.obj
    // and crate.png, "PNG" every .png file). An empty query matches every asset.
    inline bool MatchesSearch(const AssetEntry& asset, std::string_view query)
    {
        const auto first = query.find_first_not_of(" \t");
        if (first == std::string_view::npos)
        {
            return true;
        }
        query = query.substr(first, query.find_last_not_of(" \t") - first + 1);
        return ToLowerAscii(asset.name).find(ToLowerAscii(query)) != std::string::npos;
    }

    // The folder an asset is in, relative to the game folder with '/' separators ("assets/models")
    inline std::string FolderOf(const AssetEntry& asset)
    {
        const auto slash = asset.path.rfind('/');
        return slash == std::string::npos ? std::string{} : asset.path.substr(0, slash);
    }

    // The folder a folder is in ("assets/models/props" is in "assets/models"; "assets" is in "")
    inline std::string ParentFolder(std::string_view folder)
    {
        const auto slash = folder.rfind('/');
        return slash == std::string_view::npos ? std::string{} : std::string(folder.substr(0, slash));
    }

    // "assets" and every folder under projectDir/assets (hidden ones, and what is in them, skipped), relative to the
    // game folder with '/' separators, sorted ignoring case. Empty if the game has no assets folder.
    inline std::vector<std::string> ScanFolders(const std::filesystem::path& projectDir)
    {
        std::vector<std::string> folders;
        std::error_code ec;
        if (!std::filesystem::is_directory(projectDir / "assets", ec))
        {
            return folders;
        }
        folders.emplace_back("assets");
        for (auto it = std::filesystem::recursive_directory_iterator(projectDir / "assets", ec);
             !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
        {
            const std::string name = it->path().filename().string();
            if (!it->is_directory(ec))
            {
                continue;
            }
            if (name.empty() || name.front() == '.')
            {
                it.disable_recursion_pending();
                continue;
            }
            folders.push_back(it->path().lexically_relative(projectDir).generic_string());
        }
        std::sort(folders.begin(), folders.end(),
                  [](const std::string& a, const std::string& b) { return ToLowerAscii(a) < ToLowerAscii(b); });
        return folders;
    }

    // Every model, texture and audio file under projectDir/assets (searched recursively, hidden files and folders
    // skipped, and the original .obj or .fbx kept beside a .amodel left out), sorted by file name ignoring case. Empty
    // if the game has no assets folder.
    inline std::vector<AssetEntry> ScanAssets(const std::filesystem::path& projectDir)
    {
        std::vector<AssetEntry> assets;
        std::error_code ec;
        for (auto it = std::filesystem::recursive_directory_iterator(projectDir / "assets", ec);
             !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
        {
            const auto& file = it->path();
            const std::string name = file.filename().string();
            if (it->is_directory(ec) && !name.empty() && name.front() == '.')
            {
                it.disable_recursion_pending(); // a hidden folder, and everything in it
                continue;
            }
            if (it->is_directory(ec) && name == "Builtin" && file.parent_path().filename() == "shaders")
            {
                it.disable_recursion_pending(); // the engine's own shaders, copied into the game for it to run
                continue;
            }
            if (!it->is_regular_file(ec) || name.empty() || name.front() == '.')
            {
                continue;
            }
            if (const auto kind = KindOf(file))
            {
                assets.push_back({file.lexically_relative(projectDir).generic_string(), name, *kind});
            }
        }
        // A model's folder keeps the .obj or .fbx it was converted from, for importing it again; the .amodel beside it
        // is the model to use, so the original is not listed
        std::vector<std::string> converted;
        for (const AssetEntry& asset : assets)
        {
            if (asset.kind == AssetKind::Model &&
                ToLowerAscii(std::filesystem::path(asset.name).extension().string()) == ".amodel")
            {
                converted.push_back(FolderOf(asset));
            }
        }
        std::erase_if(assets,
                      [&converted](const AssetEntry& asset)
                      {
                          return asset.kind == AssetKind::Model &&
                                 ToLowerAscii(std::filesystem::path(asset.name).extension().string()) != ".amodel" &&
                                 std::find(converted.begin(), converted.end(), FolderOf(asset)) != converted.end();
                      });
        std::sort(assets.begin(), assets.end(),
                  [](const AssetEntry& a, const AssetEntry& b)
                  {
                      const std::string left = ToLowerAscii(a.name);
                      const std::string right = ToLowerAscii(b.name);
                      return left != right ? left < right : a.path < b.path;
                  });
        return assets;
    }
}
