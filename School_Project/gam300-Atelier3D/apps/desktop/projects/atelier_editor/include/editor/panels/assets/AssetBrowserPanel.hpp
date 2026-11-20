/* !
@file    	    AssetBrowserPanel.hpp
@author  	    j.cai@digipen.edu [100%]
@date    		25/10/2026
@brief
    Declares AssetBrowserPanel, the editor's Assets window: a toolbar over a
    grid of the game's models, textures and audio clips. Clicking a model or a
    texture uses it on the selected object.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "editor/panels/Panel.hpp"
#include "editor/panels/assets/AssetCatalog.hpp"
#include "editor/panels/assets/AssetFavorites.hpp"
#include "editor/panels/assets/AssetPreviews.hpp"

#include "engine/assets/ResourceManager.hpp"
#include "engine/audio/AudioSystem.hpp"

#include <imgui/imgui.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Editor::Panels
{
    /*!
    \brief  The Assets window, laid out like Unity's Project window: the game's assets/ folder as a tree of
            folders on the left (when there is room), and on the right the open folder (a path to click back up,
            its folders as tiles to double-click into, then its files). The search looks through every folder.
            Import copies files in from anywhere, and so does dropping files from the desktop's file manager on the
            editor (Import()); New Folder makes one in the open folder.

            Above the files: a search box, filter buttons (All, Models, Textures, Audio, Favorites)
            and a toolbar (asset count, Refresh, tile size) over a grid of tiles, one per model, texture
            and audio file under the game's assets/ folder whose name matches the search and that passes
            the filters. Images and models show a picture of themselves (bigger in the tooltip on hover)
            and audio clips a play button. A star on each tile (or its right-click menu) marks it as a
            favorite, saved in the game folder. Clicking a tile hands the asset to the editor (SetOnUse)
            to use on the selected object; tiles of what that object uses are highlighted, and tiles of
            files the running scene uses have a green dot. The folder is scanned again every few
            seconds, so imported or copied files show up on their own.
    */
    class AssetBrowserPanel final : public Panel
    {
    public:
        static constexpr const char* kTitle = "Assets";

        // The selected object: who a click uses an asset on, and what it uses now (those tiles are highlighted)
        struct Target
        {
            std::string objectName;
            std::string mesh;     // empty when it has no Renderable (or no mesh)
            std::string model;    // empty when it has no Renderable (or no whole model)
            std::string texture;  // empty when it has no Renderable (or no texture)
            std::string clip;     // empty when it has no Audio Source (or no clip)
            std::string material; // empty when it has no Renderable (or no material)
        };

        AssetBrowserPanel() : Panel(kTitle) {}

        /*!
        \brief   Sets the game whose assets are listed; scans it again when it changes.
        \param   projectDir The game folder, or empty when no game is open.
        */
        void SetProject(const std::filesystem::path& projectDir);

        /*!
        \brief   Sets the selected object, for this frame.
        \param   target The selected object, or nullopt when nothing is selected (a click then does nothing).
        */
        void SetTarget(std::optional<Target> target) { target_ = std::move(target); }

        // What a click on a tile does while an object is selected: the editor uses the asset on that object
        void SetOnUse(std::function<void(const AssetEntry&)> onUse) { onUse_ = std::move(onUse); }
        // Called with the files picked in the system's Open dialog (Add a sound, Bring in a file, Import a file): the
        // editor imports them as it does files dropped on it. Unset, Import() copies them in here.
        void SetOnImportFiles(std::function<void(const std::vector<std::filesystem::path>&)> onImportFiles)
        {
            onImportFiles_ = std::move(onImportFiles);
        }

        // What "Edit" on a material does (the editor opens its Material window)
        void SetOnEditMaterial(std::function<void(const std::string& path)> onEdit)
        {
            onEditMaterial_ = std::move(onEdit);
        }

        // What the window shows in its Prefabs view (the editor's list of them)
        void SetPrefabsView(std::function<void()> draw) { drawPrefabs_ = std::move(draw); }

        // The running scene's assets (nullptr when none): tiles of the files it uses get a dot
        void SetSceneResources(const Engine::Assets::ResourceManager* resources) { sceneResources_ = resources; }

        // Scans the assets folder again at the start of the next frame (after an import, for example)
        void RequestRescan() { lastScanTime_ = -1.0; }

        // The audio system the play buttons use (nullptr: audio tiles have no play button)
        void SetAudio(Engine::Audio::AudioSystem* audio) { audio_ = audio; }

        /*!
        \brief   Stops the clip being played and frees the previews. Call it while the OpenGL context is
                 still alive (the editor's Shutdown).
        */
        void Shutdown();

        // Scans the assets folder again now
        void Refresh();

        // The folder being looked at, relative to the game folder ("assets", "assets/models")
        const std::string& Folder() const { return folder_; }
        void OpenFolder(const std::string& folder);
        const std::vector<std::string>& Folders() const { return folders_; }

        /*!
        \brief   Copies files from anywhere into the game, keeping their names (a different file with the same name
                 gets a numbered copy), following the asset layout (GameProject.hpp):
                 - models (.obj, .fbx, .gltf, ..., .amodel) are converted by the asset tool: the .amodel and the
                   original files in art/models/<name>/ (under the open folder when that is a grouping folder inside
                   art/models), the images in art/textures/<name>/. A model pack (a .zip, or a folder holding one
                   model) is converted in one go and keeps the pack's images.
                 - images go by the open folder: art/spritesheets or art/sprites keep them there; under art/models,
                   art/ or assets/ itself they are textures (art/textures); any other folder keeps them
                 - sounds go to audio/, anything else into the open folder (a dropped folder comes with everything
                   in it).
        \param   sources The files and folders to copy.
        \param   errors Gets a line for each one that could not be copied.
        \return  How many files were imported.
        */
        std::size_t Import(const std::vector<std::filesystem::path>& sources, std::vector<std::string>& errors);

        const std::vector<AssetEntry>& Assets() const { return assets_; }
        // The text in the search box (only assets whose file name contains it are shown)
        const char* Search() const { return search_.data(); }
        // The kinds the Filter popup shows (all false: every kind)
        const AssetKindFilter& KindFilter() const { return kindFilter_; }
        const AssetFavorites& Favorites() const { return favorites_; }

        /*!
        \brief   Draws the By type / Folders / Prefabs switch, the search and its menus and the asset grid into the
                 current window.
        */
        void DrawContents() override;

        // The Simple interface's version, for beginners: no folders, filters or toolbar; every file of the game by
        // kind (3D models, pictures, sounds) as tiles, a search, and a button to bring files in
        void DrawSimpleContents();

    private:
        // Scans the folder again when it is time, and gets the frame's previews and scene uses ready. False (and
        // says so) when no game is open.
        bool Prepare();
        // The path of the open folder as buttons back up it ("Assets > models")
        void DrawBreadcrumb();
        // The folder tree; a click opens a folder
        void DrawFolderTree(const std::string& folder);
        // How wide the folder tree is drawn: enough for its longest name
        float FolderTreeWidth() const;
        // One folder in the open folder: a folder picture over its name; double-click to open it
        void DrawFolderTile(const std::string& folder, float tileWidth);
        // The Import and New Folder popups
        void DrawPopups();

        /*!
        \brief   Draws the search box across the window, with a button that clears it while it has text.
                 Escape in the box clears it too.
        \param   reserved Width to leave to its right, for what goes on the same row.
        */
        void DrawSearchBar(float reserved = 0.0f);

        /*!
        \brief   Rebuilds visible_ (the assets that match the search and pass the kind and favorites
                 filters, in the catalog's order), kindCounts_ (how many of each kind match the search)
                 and favoriteMatches_ (how many favorites match it).
        */
        void UpdateVisible();

        /*!
        \brief   Draws the top of the window: the By type / Folders / Prefabs switch; for the files the search, a Filter
                 popup (the kinds with how many of each match the search, favorites, the tile size) and a + menu
                 (import, new material, shader or folder); and in Folders the path of the open folder.
        \return  true if what is shown changed.
        */
        bool DrawHeader();

        /*!
        \brief   Draws one line saying what a click does with the current target.
        */
        void DrawTargetHint() const;

        /*!
        \brief   Draws the scrolling grid of the visible tiles, as many per row as fit the window width.
        */
        void DrawGrid();

        /*!
        \brief   Draws one tile over its file name, highlighted when the target already uses it: the picture
                 of an image or model, a play button for audio, or a coloured badge while the picture is not
                 made yet (or the file cannot be read). Hovering it shows DrawTooltip.
        \param   asset The asset to draw.
        \param   tileWidth Width of the tile in pixels (the picture area is square).
        \return  true if the tile was clicked (not its play or star button).
        */
        bool DrawTile(const AssetEntry& asset, float tileWidth);

        /*!
        \brief   Draws the star in a tile's top-right corner: gold for a favorite, an outline while the tile
                 is hovered, nothing otherwise. Clicking it adds or removes the favorite.
        \param   asset The tile's asset.
        \param   center Centre of the star in screen pixels.
        \param   radius Size of the star in pixels.
        \param   tileHovered Whether the mouse is over the tile (shows the outline).
        \return  true while the mouse is over the star.
        */
        bool DrawFavoriteButton(const AssetEntry& asset, const ImVec2& center, float radius, bool tileHovered);

        /*!
        \brief   Draws a tile's right-click menu: add to / remove from favorites, and copy its path.
        \param   asset The tile's asset.
        */
        void DrawTileMenu(const AssetEntry& asset);

        // Adds or removes the asset from the favorites and saves them in the game folder
        void ToggleFavorite(const AssetEntry& asset);

        /*!
        \brief   Draws the hover tooltip: a bigger picture, the file name and path, the kind with its details
                 (image size, vertex and triangle counts) and what a click does.
        \param   asset The hovered asset.
        \param   preview Its preview, or nullptr when it has none (yet).
        */
        void DrawTooltip(const AssetEntry& asset, const AssetPreviews::Preview* preview) const;

        /*!
        \brief   Draws the round play / stop button of an audio tile and plays or stops the clip on a click.
        \param   asset The audio asset.
        \param   center Centre of the button in screen pixels.
        \param   diameter Size of the button in pixels.
        \return  true while the mouse is over the button.
        */
        bool DrawPlayButton(const AssetEntry& asset, const ImVec2& center, float diameter);

        // Plays the asset's clip on the SFX bus (stopping the one playing), or stops it if it is the one playing
        void TogglePreviewSound(const AssetEntry& asset);
        void StopPreviewSound();
        bool IsPreviewPlaying(const AssetEntry& asset) const;

        // What the selected object uses for this kind of asset (nullptr when nothing is selected)
        const std::string* TargetUses(AssetKind kind) const;

        std::filesystem::path projectDir_;
        std::vector<AssetEntry> assets_;
        std::vector<const AssetEntry*> visible_; // into assets_; rebuilt every frame after any rescan
        std::array<char, 128> search_{};
        AssetKindFilter kindFilter_{};
        std::array<std::size_t, kAssetKinds.size()> kindCounts_{};
        AssetFavorites favorites_;
        bool favoritesOnly_ = false;
        std::size_t favoriteMatches_ = 0;
        std::optional<Target> target_;
        std::function<void(const AssetEntry&)> onUse_;
        std::function<void(const std::vector<std::filesystem::path>&)> onImportFiles_;
        // Opens the system's Open dialog for sounds only, or for every kind of asset, and imports what was picked.
        // Where there is no dialog it opens the Import popup to type a path instead.
        void PickAndImport(bool soundsOnly);
        std::function<void(const std::string&)> onEditMaterial_;
        std::array<char, 64> newAssetName_{}; // the New Material / New Shader name being typed
        std::string newAssetError_;
        const Engine::Assets::ResourceManager* sceneResources_ = nullptr;
        std::unordered_map<std::string, std::uint32_t> sceneUses_; // path -> objects using it; rebuilt every frame
        AssetPreviews previews_;
        Engine::Audio::AudioSystem* audio_ = nullptr;
        Engine::Audio::VoiceId previewVoice_{}; // the clip a play button started
        std::string previewPath_;               // ...and its asset path
        float tileSize_ = 72.0f;                // badge size before DPI scaling
        bool viewFolders_ = false;              // the files as they are on disk, rather than every file by kind
        bool viewPrefabs_ = false;              // the prefabs, rather than the files
        std::function<void()> drawPrefabs_;
        double lastScanTime_ = -1.0;
        std::vector<std::string> folders_; // "assets" and every folder under it
        std::string folder_ = "assets";    // the open one
        std::string openNext_;             // a folder double-clicked this frame (opened once the grid is drawn)
        float treeWidth_ = 150.0f;
        std::array<char, 512> importPath_{};
        std::array<char, 64> newFolder_{};
        std::string folderError_;
    };
}
