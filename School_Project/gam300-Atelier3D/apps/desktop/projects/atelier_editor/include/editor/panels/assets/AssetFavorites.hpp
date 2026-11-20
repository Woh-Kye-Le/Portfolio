/* !
@file    	    AssetFavorites.hpp
@author  	    j.cai@digipen.edu [100%]
@date    		25/10/2026
@brief
    AssetFavorites, the assets a game's creator starred in the Assets browser,
    kept in the game folder (data/assets.editor.favorites) so they are still
    there the next time the game is opened.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/serializer/JsonSerializer.hpp"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace Editor::Panels
{
    // A game's favorite assets, by path relative to the game folder (as AssetEntry::path)
    class AssetFavorites
    {
    public:
        // Where in the game folder the favorites are saved: { "version": 1, "favorites": [ "assets/...", ... ] }
        static constexpr const char* kFile = "data/assets.editor.favorites";
        static constexpr int kFormatVersion = 1;

        bool Contains(std::string_view path) const { return paths_.find(path) != paths_.end(); }
        std::size_t Count() const { return paths_.size(); }
        const std::set<std::string, std::less<>>& Paths() const { return paths_; }

        // Adds path, or removes it if it is already a favorite
        void Toggle(const std::string& path)
        {
            if (!paths_.erase(path))
            {
                paths_.insert(path);
            }
        }

        void Clear() { paths_.clear(); }

        /*!
        \brief   Replaces the favorites with the ones saved in the game. A game without the file has none.
        \param   projectDir The game folder.
        \param   error Why the file could not be read, when this returns false.
        \return  false if the file exists but cannot be read (the favorites are then empty).
        */
        bool Load(const std::filesystem::path& projectDir, std::string& error)
        {
            paths_.clear();
            const std::filesystem::path file = projectDir / kFile;
            std::error_code ec;
            if (!std::filesystem::exists(file, ec))
            {
                return true;
            }

            Engine::Serializer::JSONSerializer serializer;
            try
            {
                serializer.LoadFile(file.string());
            }
            catch (const std::exception& e)
            {
                error = "Could not read " + file.filename().string() + ": " + e.what();
                return false;
            }
            const json& root = serializer.GetCurrentNode();
            const auto version = root.find("version");
            const auto favorites = root.find("favorites");
            if (!root.is_object() || version == root.end() || !version->is_number_integer() ||
                version->get<int>() < 1 || version->get<int>() > kFormatVersion || favorites == root.end() ||
                !favorites->is_array())
            {
                error = "Could not read " + file.filename().string() +
                        ": expected { \"version\": 1, \"favorites\": [ ... ] }";
                return false;
            }
            // Anything in the list that is not a path is skipped
            for (const json& path : *favorites)
            {
                if (path.is_string() && !path.get<std::string>().empty())
                {
                    paths_.insert(path.get<std::string>());
                }
            }
            return true;
        }

        /*!
        \brief   Writes the favorites into the game folder, creating data/ if needed.
        \param   projectDir The game folder.
        \param   error Why the file could not be written, when this returns false.
        \return  true if the file was written.
        */
        bool Save(const std::filesystem::path& projectDir, std::string& error) const
        {
            const std::filesystem::path file = projectDir / kFile;
            try
            {
                std::error_code ec;
                std::filesystem::create_directories(file.parent_path(), ec);
                json root = json::object();
                root["version"] = kFormatVersion;
                root["favorites"] = std::vector<std::string>(paths_.begin(), paths_.end());

                Engine::Serializer::JSONSerializer serializer;
                serializer.ParseJSON(root.dump());
                serializer.SaveFile(file.string());
                return true;
            }
            catch (const std::exception& e)
            {
                error = "Could not write " + file.string() + ": " + e.what();
                return false;
            }
        }

    private:
        std::set<std::string, std::less<>> paths_; // sorted, so the saved file only changes where a star changed
    };
}
