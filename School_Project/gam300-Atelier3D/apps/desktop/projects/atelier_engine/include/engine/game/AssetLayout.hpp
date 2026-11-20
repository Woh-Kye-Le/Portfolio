/* !
@file               AssetLayout.hpp
@author             w.kyele@digipen.edu [100%]
@date               30/10/2026
@brief
    Declares the one-off migration of a game's assets/ folder from the earlier
    layouts (every image in assets/textures with models loose in assets/models;
    or a folder per model in assets/models with its images inside) to the
    current one, by type: art/models, art/textures, art/sprites,
    art/spritesheets, audio and prefabs. See the folder constants in
    GameProject.hpp.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Standard Library Headers
#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace Engine::Game
{
    /*!***********************************************************************
    @brief          What a migration changed.
    *************************************************************************/
    struct LayoutMigration
    {
        // Every file that was moved, as old path and new path relative to the game folder with '/' separators. An
        // image that two models use is copied into each model's folder; its entry names the first copy.
        std::vector<std::pair<std::string, std::string>> moved;
        std::size_t rewritten = 0;         // scene, prefab, favorites, .mtl and .amodel files whose paths were updated
        std::vector<std::string> warnings; // what could not be moved or was left where it was

        bool Changed() const { return !moved.empty() || rewritten > 0; }
    };

    /*!***********************************************************************
    @brief          Whether a game folder still has an earlier layout: files in assets/models, assets/textures,
                    assets/sprites or assets/spritesheets, an assets/materials folder, an image in art/models, or a
                    model in art/models that is not in a folder of its own.
    @param          gameDir       The game folder, or any folder shaped like one (the demo templates).
    *************************************************************************/
    bool NeedsLayoutMigration(const std::filesystem::path& gameDir);

    /*!***********************************************************************
    @brief          Brings a game from an earlier layout to the current one, and does nothing to a game that is
                    already there (it can be run on every open):
                    - a model on its own moves to art/models/<name>/ (<name>: its file name without the extension)
                      with the .mtl files it names, and the images those or its .amodel name (an FBX: the images in
                      assets/textures its file mentions) move to art/textures/<name>/; an image several models use is
                      copied into each one's folder
                    - a model's folder (assets/models/<name>/) moves to art/models/<name>/, its images to
                      art/textures/<name>/
                    - an image no model uses goes to art/textures/, and sprites and sprite sheets to art/sprites/ and
                      art/spritesheets/
                    - the image paths inside .amodel and .mtl files are rewritten to the images' new places, relative
                      to the file (../../textures/crate/crate.png)
                    - every path in the scenes (data/), the prefabs and the favorites is changed to the new place of the
                      file; paths of files that were never there (a scene that names a missing texture) are kept
                    - the emptied folders of the earlier layouts are removed, and a game gets every folder of the
                      current layout.
                    Files are moved, not copied, so nothing is left behind in the old places.
    @param          gameDir       The game folder, or any folder shaped like one (the demo templates).
    @return         What was moved and rewritten, and what could not be.
    *************************************************************************/
    LayoutMigration MigrateAssetLayout(const std::filesystem::path& gameDir);
}
