/* !
@file    	    AssetBrowserPanel.cpp
@author  	    j.cai@digipen.edu [100%]
@date    		25/10/2026
@brief
    Implements the editor's Assets window: the search box, the kind and
    favorites filter buttons, the toolbar, the assign hint and the grid of
    asset tiles with their previews, hover tooltips, audio play buttons,
    favorite stars and right-click menus, rescanned every few seconds.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/panels/assets/AssetBrowserPanel.hpp"
#include "editor/panels/assets/AssetDragDrop.hpp"

#include "editor/FileDialog.hpp"
#include "editor/OpenFile.hpp"
#include "editor/Theme.hpp"
#include "engine/core/Log.hpp"
#include "engine/game/ModelImporter.hpp"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <system_error>

namespace Editor::Panels
{
    namespace
    {
        // How often the assets folder is scanned again, so files imported or copied in show up
        constexpr double kRescanSeconds = 2.0;
        constexpr float kMinTileSize = 48.0f;
        constexpr float kMaxTileSize = 128.0f;
        // Gold, for favorite stars and the Favorites button while it is on
        // The dot on tiles of the files the scene uses
        constexpr ImU32 kInSceneColor = IM_COL32(90, 210, 110, 255);
        constexpr ImU32 kFavoriteColor = IM_COL32(205, 150, 20, 255);

        ImU32 KindColor(AssetKind kind)
        {
            switch (kind)
            {
            case AssetKind::Model: return IM_COL32(70, 120, 210, 255);
            case AssetKind::Texture: return IM_COL32(70, 160, 95, 255);
            case AssetKind::Audio: return IM_COL32(215, 135, 60, 255);
            case AssetKind::Material: return IM_COL32(190, 80, 160, 255);
            case AssetKind::Shader: return IM_COL32(110, 110, 130, 255);
            }
            return IM_COL32(128, 128, 128, 255);
        }

        // What an asset of this kind becomes on an object
        const char* UseName(AssetKind kind)
        {
            switch (kind)
            {
            case AssetKind::Model: return "model";
            case AssetKind::Texture: return "texture";
            case AssetKind::Audio: return "sound";
            case AssetKind::Material: return "material";
            case AssetKind::Shader: return "shader";
            }
            return "asset";
        }

        // The filter button's name for a kind
        const char* PluralName(AssetKind kind)
        {
            switch (kind)
            {
            case AssetKind::Model: return "Models";
            case AssetKind::Texture: return "Textures";
            case AssetKind::Audio: return "Audio";
            case AssetKind::Material: return "Materials";
            case AssetKind::Shader: return "Shaders";
            }
            return "Assets";
        }

        // The badge text: the file's extension in capitals (OBJ, PNG, WAV)
        std::string BadgeText(const AssetEntry& asset)
        {
            std::string text = std::filesystem::path(asset.name).extension().string();
            if (!text.empty() && text.front() == '.')
            {
                text.erase(0, 1);
            }
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            return text;
        }

        // A preview picture as large as fits in [min, max] without stretching it, centred
        void DrawPreviewImage(ImDrawList* drawList, const AssetPreviews::Preview& preview, const ImVec2& min,
                              const ImVec2& max)
        {
            const float width = max.x - min.x;
            const float height = max.y - min.y;
            const float aspect = static_cast<float>(preview.width) / static_cast<float>(std::max(preview.height, 1));
            const ImVec2 size =
                aspect * height > width ? ImVec2(width, width / aspect) : ImVec2(height * aspect, height);
            const ImVec2 start((min.x + max.x - size.x) * 0.5f, (min.y + max.y - size.y) * 0.5f);
            // OpenGL keeps the bottom row first, ImGui draws from the top: flip V
            drawList->AddImage(static_cast<ImTextureID>(preview.textureId), start,
                               ImVec2(start.x + size.x, start.y + size.y), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        }

        // A five-pointed star, filled or as an outline
        void DrawStar(ImDrawList* drawList, const ImVec2& center, float radius, ImU32 color, bool filled)
        {
            constexpr int kPoints = 10; // outer and inner corners in turn
            ImVec2 points[kPoints];
            for (int i = 0; i < kPoints; ++i)
            {
                const float angle = -IM_PI * 0.5f + static_cast<float>(i) * IM_PI / 5.0f; // first point straight up
                const float r = (i % 2 == 0) ? radius : radius * 0.45f;
                points[i] = ImVec2(center.x + std::cos(angle) * r, center.y + std::sin(angle) * r);
            }
            if (filled)
            {
                drawList->AddConcavePolyFilled(points, kPoints, color);
            }
            else
            {
                drawList->AddPolyline(points, kPoints, color, ImDrawFlags_Closed, 1.5f);
            }
        }

        // The file's extension in a small tag of its kind's colour, in the bottom-left corner of [min, max]
        void DrawKindTag(ImDrawList* drawList, AssetKind kind, const std::string& text, const ImVec2& min,
                         const ImVec2& max)
        {
            const ImVec2 padding(4.0f, 1.0f);
            const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
            const ImVec2 tagMin(min.x + 3.0f, max.y - textSize.y - padding.y * 2.0f - 3.0f);
            const ImVec2 tagMax(tagMin.x + textSize.x + padding.x * 2.0f, max.y - 3.0f);
            drawList->AddRectFilled(tagMin, tagMax, KindColor(kind), 3.0f);
            drawList->AddText(ImVec2(tagMin.x + padding.x, tagMin.y + padding.y), IM_COL32_WHITE, text.c_str());
        }
    }

    void AssetBrowserPanel::SetProject(const std::filesystem::path& projectDir)
    {
        if (projectDir == projectDir_)
        {
            return;
        }
        StopPreviewSound();
        previews_.Clear();
        projectDir_ = projectDir;
        Refresh();

        std::string error;
        if (projectDir_.empty())
        {
            favorites_.Clear();
        }
        else if (!favorites_.Load(projectDir_, error))
        {
            LOG_WARN(Editor, "Assets: {} (starting with no favorites)", error);
        }
    }

    void AssetBrowserPanel::Shutdown()
    {
        StopPreviewSound();
        previews_.Clear();
    }

    void AssetBrowserPanel::Refresh()
    {
        assets_ = projectDir_.empty() ? std::vector<AssetEntry>{} : ScanAssets(projectDir_);
        folders_ = projectDir_.empty() ? std::vector<std::string>{} : ScanFolders(projectDir_);
        if (std::find(folders_.begin(), folders_.end(), folder_) == folders_.end())
        {
            folder_ = "assets"; // the open folder was removed (or another game was opened)
        }
    }

    void AssetBrowserPanel::OpenFolder(const std::string& folder)
    {
        folder_ = folder;
        lastScanTime_ = -1.0;
    }

    std::size_t AssetBrowserPanel::Import(const std::vector<std::filesystem::path>& sources,
                                          std::vector<std::string>& errors)
    {
        if (projectDir_.empty())
        {
            errors.emplace_back("Open a game before importing files");
            return 0;
        }
        const Engine::Game::GameProject project(projectDir_);
        std::size_t imported = 0;
        // Whether a folder (relative to the game, '/' separators) is root or inside it
        const auto isUnder = [](const std::filesystem::path& folder, const std::string& root)
        {
            const std::string text = folder.generic_string();
            return text == root || text.rfind(root + "/", 0) == 0;
        };
        // Whether a folder of the game holds a model file itself (it is a model's own folder)
        const auto holdsModel = [this](const std::filesystem::path& folder)
        {
            std::error_code ec;
            for (std::filesystem::directory_iterator it(projectDir_ / folder, ec), end; !ec && it != end;
                 it.increment(ec))
            {
                if (it->is_regular_file(ec) && KindOf(it->path()) == AssetKind::Model)
                {
                    return true;
                }
            }
            return false;
        };
        const auto importFile = [&](const std::filesystem::path& file, const std::filesystem::path& into)
        {
            try
            {
                if (Engine::Game::IsModelFile(file) || Engine::Game::IsModelPackage(file))
                {
                    // Models go through the asset tool: the .amodel and the original files in art/models/<name>/, the
                    // images in art/textures/<name>/. A model pack (a .zip, or a folder holding one model) goes in
                    // whole, with its images. A folder of art/models that is open (props/, say) groups the new one,
                    // but a model never goes inside another model's folder.
                    std::filesystem::path models = Engine::Game::kModelsFolder;
                    if (isUnder(into, Engine::Game::kModelsFolder) && into != models)
                    {
                        models = holdsModel(into) ? into.parent_path() : into;
                    }
                    const Engine::Game::ModelImportResult result = Engine::Game::ImportModel(project, file, models);
                    for (const std::string& warning : result.warnings)
                    {
                        LOG_WARN(Editor, "Import {}: {}", result.path, warning);
                    }
                }
                else if (KindOf(file) == AssetKind::Texture && isUnder(into, Engine::Game::kSpriteSheetsFolder))
                {
                    project.ImportImage(file, Engine::Game::ImageUse::SpriteSheet);
                }
                else if (KindOf(file) == AssetKind::Texture && isUnder(into, Engine::Game::kSpritesFolder))
                {
                    project.ImportImage(file, Engine::Game::ImageUse::Sprite);
                }
                else if (KindOf(file) == AssetKind::Texture &&
                         (isUnder(into, Engine::Game::kModelsFolder) || isUnder(into, Engine::Game::kArtFolder) ||
                          into == "assets"))
                {
                    // Under models/ or art/ (or assets/ itself) an image is a texture for 3D objects
                    project.ImportImage(file, Engine::Game::ImageUse::Texture3D);
                }
                else if (KindOf(file) == AssetKind::Audio && !isUnder(into, Engine::Game::kAudioFolder))
                {
                    project.ImportAsset(file, Engine::Game::kAudioFolder);
                }
                else
                {
                    project.ImportAsset(file, into); // art/textures/... and every other folder keep what lands there
                }
                ++imported;
            }
            catch (const std::exception& e)
            {
                errors.emplace_back(e.what());
            }
        };
        for (const std::filesystem::path& source : sources)
        {
            std::error_code ec;
            if (std::filesystem::is_directory(source, ec) && !Engine::Game::IsModelPackage(source))
            {
                // A folder comes with everything in it, under a folder of the same name
                for (auto it = std::filesystem::recursive_directory_iterator(source, ec);
                     !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
                {
                    if (it->is_regular_file(ec))
                    {
                        const auto relative = it->path().parent_path().lexically_relative(source.parent_path());
                        importFile(it->path(), std::filesystem::path(folder_) / relative);
                    }
                }
            }
            else
            {
                importFile(source, folder_);
            }
        }
        lastScanTime_ = -1.0; // they show up next frame
        return imported;
    }

    bool AssetBrowserPanel::Prepare()
    {
        if (projectDir_.empty())
        {
            ImGui::TextUnformatted("Select a project to browse assets.");
            return false;
        }
        const double now = ImGui::GetTime();
        if (lastScanTime_ < 0.0 || now - lastScanTime_ >= kRescanSeconds)
        {
            Refresh();
            previews_.Prune(projectDir_, assets_); // pictures of changed files are made again
            lastScanTime_ = now;
        }
        if (!openNext_.empty())
        {
            OpenFolder(openNext_);
            openNext_.clear();
            Refresh();
        }
        previews_.BeginFrame();

        // Which files the running scene uses, and by how many objects
        sceneUses_.clear();
        if (sceneResources_)
        {
            for (const auto& loaded : sceneResources_->ListAssets())
            {
                if (!loaded.isPlaceholder && loaded.refCount > 0)
                {
                    sceneUses_[loaded.path] = loaded.refCount;
                }
            }
        }
        return true;
    }

    void AssetBrowserPanel::DrawSimpleContents()
    {
        if (!Prepare())
        {
            return;
        }
        DrawPopups();
        ImGui::PushTextWrapPos(0.0f);
        if (target_)
        {
            ImGui::TextDisabled("Click one to use it on %s, or drag it onto any object", target_->objectName.c_str());
        }
        else
        {
            ImGui::TextDisabled("Pick an object, then click one of these to use it on it. Or drag one onto an object.");
        }
        ImGui::PopTextWrapPos();
        if (ImGui::Button("Add a sound..."))
        {
            PickAndImport(true);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("Pick sounds on your computer (.wav, .ogg, .mp3, .flac) to use in your game");
        }
        ImGui::SameLine();
        if (ImGui::Button("Bring in a file..."))
        {
            PickAndImport(false);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("Or drag pictures, 3D models or sounds from your computer onto the editor");
        }
        DrawSearchBar();
        ImGui::Spacing();

        // One group per kind, each as many tiles to a row as fit
        static constexpr std::array<std::pair<AssetKind, const char*>, 3> kGroups{
            {{AssetKind::Model, "3D models"}, {AssetKind::Texture, "Pictures"}, {AssetKind::Audio, "Sounds"}}};
        const float tileWidth = 84.0f * ImGui::GetStyle().FontScaleMain;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        bool any = false;
        for (const auto& [kind, title] : kGroups)
        {
            std::vector<const AssetEntry*> group;
            for (const AssetEntry& asset : assets_)
            {
                if (asset.kind == kind && MatchesSearch(asset, search_.data()))
                {
                    group.push_back(&asset);
                }
            }
            if (group.empty())
            {
                continue;
            }
            any = true;
            ImGui::SeparatorText(std::format("{} ({})", title, group.size()).c_str());
            const auto columns = static_cast<std::size_t>(
                std::max(1.0f, (ImGui::GetContentRegionAvail().x + spacing) / (tileWidth + spacing)));
            for (std::size_t i = 0; i < group.size(); ++i)
            {
                if (i % columns != 0)
                {
                    ImGui::SameLine();
                }
                if (DrawTile(*group[i], tileWidth) && target_ && onUse_)
                {
                    onUse_(*group[i]);
                }
            }
        }
        if (!any)
        {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled(assets_.empty()
                                    ? "Nothing here yet. Drag pictures (.png, .jpg), 3D models (.glb, .obj, .fbx) "
                                      "or sounds (.wav, .ogg) from your computer onto the editor to bring "
                                      "them into your game."
                                    : "Nothing has that name.");
            ImGui::PopTextWrapPos();
        }
    }

    void AssetBrowserPanel::DrawContents()
    {
        if (!Prepare())
        {
            return;
        }

        DrawPopups();
        UpdateVisible();
        if (DrawHeader())
        {
            UpdateVisible();
        }
        if (viewPrefabs_)
        {
            if (ImGui::BeginChild("##prefabs") && drawPrefabs_)
            {
                drawPrefabs_();
            }
            ImGui::EndChild();
            return;
        }
        DrawTargetHint();

        // The folder tree on the left, when the window is wide enough for it next to the tiles
        const float scale = ImGui::GetStyle().FontScaleMain;
        if (viewFolders_ && ImGui::GetContentRegionAvail().x > 380.0f * scale && !folders_.empty())
        {
            ImGui::BeginChild("##folders", ImVec2(FolderTreeWidth(), 0.0f), ImGuiChildFlags_Borders);
            DrawFolderTree("assets");
            ImGui::EndChild();
            ImGui::SameLine();
        }

        const bool searching = search_.front() != '\0';
        const bool hasFolders = viewFolders_ && !searching &&
                                std::any_of(folders_.begin(), folders_.end(),
                                            [this](const auto& each) { return ParentFolder(each) == folder_; });
        if (visible_.empty() && !hasFolders)
        {
            ImGui::BeginGroup();
            if (assets_.empty() && (folder_ == "assets" || !viewFolders_) && !searching)
            {
                ImGui::TextWrapped("No asset files yet. Drag files here from File Explorer, or use the + button.");
                ImGui::EndGroup();
                return;
            }
            if (!searching && !favoritesOnly_ && !IsFiltering(kindFilter_))
            {
                ImGui::TextWrapped("This folder is empty. Drag files onto the editor from File Explorer to put them "
                                   "here, or use the + button.");
                ImGui::EndGroup();
                return;
            }
            if (favoritesOnly_ && favoriteMatches_ == 0 && search_.front() == '\0')
            {
                ImGui::TextWrapped("No favorites yet. Click the star on a tile (or right-click it) to add one.");
            }
            else if (search_.front() == '\0')
            {
                ImGui::TextWrapped("No assets of the selected types.");
            }
            else
            {
                ImGui::TextWrapped("No assets match \"%s\"%s.", search_.data(),
                                   IsFiltering(kindFilter_) || favoritesOnly_ ? " in the selected types" : "");
            }
            ImGui::EndGroup();
            return;
        }
        DrawGrid();
        if (!openNext_.empty())
        {
            OpenFolder(openNext_);
            openNext_.clear();
        }
    }

    void AssetBrowserPanel::DrawBreadcrumb()
    {
        // Assets > models > props: each part goes back up to it.
        std::vector<std::string> parts;
        for (std::string at = folder_; !at.empty(); at = ParentFolder(at))
        {
            parts.insert(parts.begin(), at);
        }
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        for (std::size_t i = 0; i < parts.size(); ++i)
        {
            if (i > 0)
            {
                ImGui::SameLine(0.0f, 2.0f);
                ImGui::TextDisabled(">");
                ImGui::SameLine(0.0f, 2.0f);
            }
            const std::string name =
                i == 0 ? std::string("Assets") : std::filesystem::path(parts[i]).filename().string();
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::SmallButton(name.c_str()))
            {
                openNext_ = parts[i];
            }
            ImGui::PopID();
        }
        ImGui::PopStyleColor();
        if (!openNext_.empty())
        {
            OpenFolder(openNext_);
            openNext_.clear();
        }
    }

    void AssetBrowserPanel::DrawPopups()
    {
        if (ImGui::BeginPopup("##newAsset"))
        {
            ImGui::TextUnformatted("Make a new...");
            ImGui::SetNextItemWidth(220.0f * ImGui::GetStyle().FontScaleMain);
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::InputTextWithHint("##name", "Name (e.g. Shiny Red)", newAssetName_.data(), newAssetName_.size());
            const Engine::Game::GameProject project(projectDir_);
            const auto make = [&](bool material)
            {
                try
                {
                    const std::string path = material ? project.CreateMaterial(newAssetName_.data())
                                                      : project.CreateShader(newAssetName_.data());
                    LOG_INFO(Editor, "Made {}", path);
                    openNext_ = std::filesystem::path(path).parent_path().generic_string();
                    lastScanTime_ = -1.0; // list it now
                    if (material && onEditMaterial_)
                    {
                        onEditMaterial_(path);
                    }
                    else if (!material)
                    {
                        std::string error;
                        OpenWithDefaultApp(projectDir_ / std::filesystem::path(path).replace_extension(".frag"), error);
                    }
                    ImGui::CloseCurrentPopup();
                }
                catch (const std::exception& e)
                {
                    newAssetError_ = e.what();
                }
            };
            ImGui::BeginDisabled(projectDir_.empty());
            if (ImGui::Button("Material"))
            {
                make(true);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("In assets/art/materials: colour, texture and shader for objects. It opens to edit.");
            }
            ImGui::SameLine();
            if (ImGui::Button("Shader"))
            {
                make(false);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("In assets/shaders: a copy of the standard shader (.vert and .frag) to change.\n"
                                  "Its .frag opens in your code editor; pick it in a material to use it.");
            }
            ImGui::EndDisabled();
            if (!newAssetError_.empty())
            {
                ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.4f, 1.0f), "%s", newAssetError_.c_str());
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup("##importFiles"))
        {
            ImGui::Text("Import into %s", folder_.c_str());
            ImGui::SetNextItemWidth(320.0f * ImGui::GetStyle().FontScaleMain);
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }
            const bool enter = ImGui::InputTextWithHint("##path", "C:/path/to/file.png", importPath_.data(),
                                                        importPath_.size(), ImGuiInputTextFlags_EnterReturnsTrue);
            if (ImGui::Button("Import") || enter)
            {
                std::string source = importPath_.data();
                std::erase(source, '"'); // pasted paths are often quoted
                std::vector<std::string> errors;
                const std::size_t count = Import({std::filesystem::path(source)}, errors);
                for (const std::string& error : errors)
                {
                    LOG_WARN(Editor, "Import: {}", error);
                }
                if (count > 0)
                {
                    LOG_INFO(Editor, "Imported {} file{} into {}", count, count == 1 ? "" : "s", folder_);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopup("##newFolder"))
        {
            ImGui::Text("New folder in %s", folder_.c_str());
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::SetNextItemWidth(200.0f * ImGui::GetStyle().FontScaleMain);
            const bool enter = ImGui::InputTextWithHint("##name", "Folder name", newFolder_.data(), newFolder_.size(),
                                                        ImGuiInputTextFlags_EnterReturnsTrue);
            const std::string name = newFolder_.data();
            const bool valid =
                !name.empty() && name.find_first_of("/\\:*?\"<>|") == std::string::npos && name.front() != '.';
            ImGui::BeginDisabled(!valid);
            if (ImGui::Button("Make") || (enter && valid))
            {
                std::error_code ec;
                const auto made = projectDir_ / folder_ / name;
                std::filesystem::create_directories(made, ec);
                if (ec)
                {
                    folderError_ = ec.message();
                }
                else
                {
                    openNext_ = (std::filesystem::path(folder_) / name).generic_string();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndDisabled();
            if (!folderError_.empty())
            {
                ImGui::TextColored(Theme::kErrorText, "%s", folderError_.c_str());
            }
            ImGui::EndPopup();
        }
    }

    // As wide as the longest folder name needs at its depth in the tree (with its arrow, the padding and room for a
    // scrollbar), so no name is cut off at any font size; at least treeWidth_, and at most 40% of the window so the
    // tiles keep their room
    float AssetBrowserPanel::FolderTreeWidth() const
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float arrow = ImGui::GetTreeNodeToLabelSpacing();
        float widest = arrow + ImGui::CalcTextSize("Assets").x;
        for (const std::string& folder : folders_)
        {
            // assets/art/models: depth 2 under the Assets node
            const auto depth = static_cast<float>(std::count(folder.begin(), folder.end(), '/'));
            const std::string name = std::filesystem::path(folder).filename().string();
            widest = std::max(widest, depth * style.IndentSpacing + arrow + ImGui::CalcTextSize(name.c_str()).x);
        }
        const float needed = widest + style.FramePadding.x * 2.0f + style.WindowPadding.x * 2.0f + style.ScrollbarSize;
        const float most = std::max(ImGui::GetContentRegionAvail().x * 0.4f, treeWidth_ * style.FontScaleMain);
        return std::clamp(needed, treeWidth_ * style.FontScaleMain, most);
    }

    void AssetBrowserPanel::DrawFolderTree(const std::string& folder)
    {
        std::vector<const std::string*> children;
        for (const std::string& each : folders_)
        {
            if (ParentFolder(each) == folder)
            {
                children.push_back(&each);
            }
        }
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (folder == "assets")
        {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
        if (children.empty())
        {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (folder == folder_)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        // The open folder's parents stay open, so it can be seen in the tree
        if (folder_.starts_with(folder + "/"))
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }
        const std::string name =
            folder == "assets" ? std::string("Assets") : std::filesystem::path(folder).filename().string();
        const bool open = ImGui::TreeNodeEx(folder.c_str(), flags, "%s", name.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            openNext_ = folder;
        }
        if (open)
        {
            for (const std::string* child : children)
            {
                DrawFolderTree(*child);
            }
            ImGui::TreePop();
        }
    }

    void AssetBrowserPanel::DrawFolderTile(const std::string& folder, float tileWidth)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float labelHeight = ImGui::GetTextLineHeight();
        const ImVec2 size(tileWidth, tileWidth + style.ItemInnerSpacing.y + labelHeight);
        const std::string name = std::filesystem::path(folder).filename().string();
        ImGui::PushID(folder.c_str());
        ImGui::BeginGroup();
        ImGui::Selectable("##folder", false, ImGuiSelectableFlags_AllowDoubleClick, size);
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            openNext_ = folder;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("%s\nDouble-click to open", folder.c_str());
        }
        if (ImGui::BeginPopupContextItem("##folderMenu"))
        {
            if (ImGui::MenuItem("Open"))
            {
                openNext_ = folder;
            }
            ImGui::EndPopup();
        }
        const ImVec2 min = ImGui::GetItemRectMin();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // A folder: a tab on top of a body, in a warm yellow
        const float inset = tileWidth * 0.14f;
        const ImVec2 bodyMin(min.x + inset, min.y + inset + tileWidth * 0.12f);
        const ImVec2 bodyMax(min.x + tileWidth - inset, min.y + tileWidth - inset);
        const ImU32 back = IM_COL32(200, 150, 60, 255);
        const ImU32 front = IM_COL32(236, 186, 86, 255);
        drawList->AddRectFilled(ImVec2(bodyMin.x, bodyMin.y - tileWidth * 0.1f),
                                ImVec2(bodyMin.x + (bodyMax.x - bodyMin.x) * 0.45f, bodyMin.y + 4.0f), back, 3.0f);
        drawList->AddRectFilled(bodyMin, bodyMax, front, 4.0f);

        const ImVec2 labelMin(min.x + 2.0f, min.y + tileWidth + style.ItemInnerSpacing.y);
        const ImVec2 labelMax(min.x + tileWidth - 2.0f, labelMin.y + labelHeight);
        const ImVec2 nameSize = ImGui::CalcTextSize(name.c_str());
        if (nameSize.x <= labelMax.x - labelMin.x)
        {
            drawList->AddText(ImVec2((labelMin.x + labelMax.x - nameSize.x) * 0.5f, labelMin.y),
                              ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
        }
        else
        {
            ImGui::RenderTextEllipsis(drawList, labelMin, labelMax, labelMax.x, name.c_str(), nullptr, &nameSize);
        }
        ImGui::EndGroup();
        ImGui::PopID();
    }

    void AssetBrowserPanel::DrawSearchBar(float reserved)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const bool searching = search_.front() != '\0';
        const float clearWidth = searching ? ImGui::GetFrameHeight() + style.ItemInnerSpacing.x : 0.0f;
        ImGui::SetNextItemWidth(std::max(1.0f, ImGui::GetContentRegionAvail().x - clearWidth - reserved));
        ImGui::InputTextWithHint("##search", "Search by name", search_.data(), search_.size(),
                                 ImGuiInputTextFlags_EscapeClearsAll);
        if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
        {
            ImGui::SetTooltip("Shows the assets whose file name contains this text (any case)");
        }
        if (searching)
        {
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            if (ImGui::Button("x##clearSearch", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
            {
                search_.front() = '\0';
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Clear the search");
            }
        }
    }

    void AssetBrowserPanel::UpdateVisible()
    {
        visible_.clear();
        kindCounts_.fill(0);
        favoriteMatches_ = 0;
        const bool searching = search_.front() != '\0';
        for (const AssetEntry& asset : assets_)
        {
            if (!MatchesSearch(asset, search_.data()) || (viewFolders_ && !searching && FolderOf(asset) != folder_))
            {
                continue;
            }
            ++kindCounts_[static_cast<std::size_t>(asset.kind)];
            const bool favorite = favorites_.Contains(asset.path);
            favoriteMatches_ += favorite ? 1 : 0;
            if (PassesKindFilter(asset.kind, kindFilter_) && (!favoritesOnly_ || favorite))
            {
                visible_.push_back(&asset);
            }
        }
    }

    bool AssetBrowserPanel::DrawHeader()
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float scale = style.FontScaleMain;
        bool changed = false;

        // Every file by kind (as in the Simple interface), the files as they are on disk, or the prefabs
        static constexpr std::array<const char*, 3> kViews{"By type", "Folders", "Prefabs"};
        const float viewHeight = ImGui::GetFrameHeight();
        int view = viewPrefabs_ ? 2 : viewFolders_ ? 1 : 0;
        if (Theme::Segmented("##assetView", kViews.data(), static_cast<int>(kViews.size()), view,
                             Theme::SegmentedWidth(kViews.data(), static_cast<int>(kViews.size()), viewHeight),
                             viewHeight))
        {
            viewFolders_ = view == 1;
            viewPrefabs_ = view == 2;
            changed = true;
        }
        if (viewPrefabs_)
        {
            return changed;
        }

        // One row: the search, the filter (lit while it hides something), and what can be added
        const bool filtering = IsFiltering(kindFilter_) || favoritesOnly_;
        const char* filterLabel = filtering ? "Filter *###assetFilterButton" : "Filter###assetFilterButton";
        const float filterWidth = ImGui::CalcTextSize("Filter *").x + style.FramePadding.x * 2.0f;
        const float addWidth = ImGui::GetFrameHeight();
        DrawSearchBar(filterWidth + addWidth + style.ItemSpacing.x * 2.0f);
        ImGui::SameLine();
        if (filtering)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
        }
        if (ImGui::Button(filterLabel, ImVec2(filterWidth, 0.0f)))
        {
            ImGui::OpenPopup("##assetFilter");
        }
        if (filtering)
        {
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("Which kinds of asset to show, and how big");
        }
        ImGui::SameLine();
        if (ImGui::Button("+##assetAdd", ImVec2(addWidth, 0.0f)))
        {
            ImGui::OpenPopup("##assetAddMenu");
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("Import a file, or make a material, shader or folder");
        }

        if (ImGui::BeginPopup("##assetFilter"))
        {
            // It stays open while several are ticked
            ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
            Theme::Label("SHOW");
            if (ImGui::MenuItem("Everything", nullptr, !IsFiltering(kindFilter_)))
            {
                kindFilter_.fill(false);
                changed = true;
            }
            for (const AssetKind kind : kAssetKinds)
            {
                const std::size_t index = static_cast<std::size_t>(kind);
                if (ImGui::MenuItem(PluralName(kind), std::to_string(kindCounts_[index]).c_str(), kindFilter_[index]))
                {
                    kindFilter_[index] = !kindFilter_[index];
                    changed = true;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Only favorites", std::to_string(favoriteMatches_).c_str(), favoritesOnly_))
            {
                favoritesOnly_ = !favoritesOnly_;
                changed = true;
            }
            ImGui::PopItemFlag();
            ImGui::Separator();
            Theme::Label("TILE SIZE");
            ImGui::SetNextItemWidth(180.0f * scale);
            ImGui::SliderFloat("##tileSize", &tileSize_, kMinTileSize, kMaxTileSize, "%.0f");
            ImGui::Separator();
            ImGui::TextDisabled("%zu of %zu assets", visible_.size(), assets_.size());
            // Scanned at the start of the next frame, so visible_ stays valid for the rest of this one
            if (ImGui::SmallButton("Scan the files again"))
            {
                lastScanTime_ = -1.0;
            }
            ImGui::EndPopup();
        }

        // Each of these opens a popup of the window, not of this menu; the Open dialog waits until the menu is closed
        const char* openAfter = nullptr;
        enum class PickKind : std::uint8_t
        {
            None,
            Any,
            Sounds
        };
        PickKind pick = PickKind::None;
        if (ImGui::BeginPopup("##assetAddMenu"))
        {
            if (ImGui::MenuItem("Import a file..."))
            {
                pick = PickKind::Any;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("Copy a file into the game (or drag files onto the editor from File Explorer)");
            }
            if (ImGui::MenuItem("Import sounds..."))
            {
                pick = PickKind::Sounds;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("Copy sounds (.wav, .ogg, .mp3, .flac) into the game's assets/audio");
            }
            if (ImGui::MenuItem("New material or shader..."))
            {
                newAssetName_.fill('\0');
                newAssetError_.clear();
                openAfter = "##newAsset";
            }
            if (viewFolders_ && ImGui::MenuItem("New folder..."))
            {
                newFolder_.fill('\0');
                folderError_.clear();
                openAfter = "##newFolder";
            }
            ImGui::EndPopup();
        }
        if (openAfter != nullptr)
        {
            ImGui::OpenPopup(openAfter);
        }
        if (pick != PickKind::None)
        {
            PickAndImport(pick == PickKind::Sounds);
        }

        if (viewFolders_)
        {
            DrawBreadcrumb();
        }
        return changed;
    }

    void AssetBrowserPanel::DrawTargetHint() const
    {
        // Only when a click does something; wrapped, so the hint stays readable in a narrow docked window
        if (target_)
        {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("Click one to use it on %s", target_->objectName.c_str());
            ImGui::PopTextWrapPos();
        }
    }

    void AssetBrowserPanel::DrawGrid()
    {
        if (ImGui::BeginChild("##assetGrid", ImVec2(0.0f, 0.0f)))
        {
            const float tileWidth = tileSize_ * ImGui::GetStyle().FontScaleMain;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float width = ImGui::GetContentRegionAvail().x;
            const std::size_t columns =
                static_cast<std::size_t>(std::max(1.0f, (width + spacing) / (tileWidth + spacing)));
            std::size_t tile = 0;
            if (!viewFolders_)
            {
                // A heading for each kind, each kind's tiles from the start of a row
                for (const AssetKind kind : kAssetKinds)
                {
                    tile = 0;
                    for (const AssetEntry* asset : visible_)
                    {
                        if (asset->kind != kind)
                        {
                            continue;
                        }
                        if (tile == 0)
                        {
                            const auto count =
                                std::count_if(visible_.begin(), visible_.end(),
                                              [kind](const AssetEntry* each) { return each->kind == kind; });
                            ImGui::SeparatorText(std::format("{} ({})", PluralName(kind), count).c_str());
                        }
                        if (tile++ % columns != 0)
                        {
                            ImGui::SameLine();
                        }
                        if (DrawTile(*asset, tileWidth) && target_ && onUse_)
                        {
                            onUse_(*asset);
                        }
                    }
                }
                ImGui::EndChild();
                return;
            }
            if (search_.front() == '\0')
            {
                for (const std::string& folder : folders_)
                {
                    if (ParentFolder(folder) != folder_)
                    {
                        continue;
                    }
                    if (tile++ % columns != 0)
                    {
                        ImGui::SameLine();
                    }
                    DrawFolderTile(folder, tileWidth);
                }
            }
            for (std::size_t i = 0; i < visible_.size(); ++i)
            {
                if (tile++ % columns != 0)
                {
                    ImGui::SameLine();
                }
                const AssetEntry& asset = *visible_[i];
                if (DrawTile(asset, tileWidth) && target_ && onUse_)
                {
                    onUse_(asset);
                }
            }
        }
        ImGui::EndChild();
    }

    bool AssetBrowserPanel::DrawTile(const AssetEntry& asset, float tileWidth)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float labelHeight = ImGui::GetTextLineHeight();
        const ImVec2 size(tileWidth, tileWidth + style.ItemInnerSpacing.y + labelHeight);
        const std::string* field = TargetUses(asset.kind);
        const AssetPreviews::Preview* preview = previews_.Get(projectDir_, asset);

        // A group, so the next tile's SameLine lines up with the whole tile even when it has a play button
        ImGui::PushID(asset.path.c_str());
        ImGui::BeginGroup();
        ImGui::SetNextItemAllowOverlap(); // the play button sits on top of the tile
        const bool clicked =
            ImGui::Selectable("##tile", field && *field == asset.path, ImGuiSelectableFlags_None, size);
        const bool hovered = ImGui::IsItemHovered();
        DrawTileMenu(asset);
        BeginAssetDrag(asset);
        const ImVec2 min = ImGui::GetItemRectMin();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const float inset = tileWidth * 0.1f;
        const float rounding = style.FrameRounding + 4.0f;
        const ImVec2 badgeMin(min.x + inset, min.y + inset);
        const ImVec2 badgeMax(min.x + tileWidth - inset, min.y + tileWidth - inset);
        const std::string badge = BadgeText(asset);
        bool playHovered = false;
        if (preview && preview->textureId != 0)
        {
            // The picture, with the file's extension in a small tag of the kind's colour
            drawList->AddRectFilled(badgeMin, badgeMax, ImGui::GetColorU32(ImGuiCol_FrameBg), rounding);
            DrawPreviewImage(drawList, *preview, badgeMin, badgeMax);
            DrawKindTag(drawList, asset.kind, badge, badgeMin, badgeMax);
        }
        else if (asset.kind == AssetKind::Audio && audio_)
        {
            drawList->AddRectFilled(badgeMin, badgeMax, KindColor(asset.kind), rounding);
            DrawKindTag(drawList, asset.kind, badge, badgeMin, badgeMax);
            // A little above the middle, clear of the extension tag
            const float badgeWidth = badgeMax.x - badgeMin.x;
            playHovered = DrawPlayButton(
                asset, ImVec2((badgeMin.x + badgeMax.x) * 0.5f, badgeMin.y + badgeWidth * 0.4f), badgeWidth * 0.42f);
        }
        else
        {
            // The badge: the kind's colour with the file's extension in the middle
            drawList->AddRectFilled(badgeMin, badgeMax, KindColor(asset.kind), rounding);
            const ImVec2 badgeSize = ImGui::CalcTextSize(badge.c_str());
            drawList->AddText(
                ImVec2((badgeMin.x + badgeMax.x - badgeSize.x) * 0.5f, (badgeMin.y + badgeMax.y - badgeSize.y) * 0.5f),
                IM_COL32_WHITE, badge.c_str());
        }

        // A green dot in the bottom-right corner when the scene uses the file
        if (sceneUses_.contains(asset.path))
        {
            const float dot = std::max(3.0f, (badgeMax.x - badgeMin.x) * 0.05f);
            const ImVec2 center(badgeMax.x - dot - 4.0f, badgeMax.y - dot - 4.0f);
            drawList->AddCircleFilled(center, dot + 1.5f, IM_COL32(0, 0, 0, 140));
            drawList->AddCircleFilled(center, dot, kInSceneColor);
        }

        // The favorite star in the top-right corner of the picture area
        const float starRadius = std::max(6.0f, (badgeMax.x - badgeMin.x) * 0.12f);
        const bool starHovered = DrawFavoriteButton(
            asset, ImVec2(badgeMax.x - starRadius - 3.0f, badgeMin.y + starRadius + 3.0f), starRadius, hovered);

        // The file name under it: centred, or cut with "..." when it is wider than the tile
        const ImVec2 labelMin(min.x + 2.0f, min.y + tileWidth + style.ItemInnerSpacing.y);
        const ImVec2 labelMax(min.x + tileWidth - 2.0f, labelMin.y + labelHeight);
        const ImVec2 nameSize = ImGui::CalcTextSize(asset.name.c_str());
        if (nameSize.x <= labelMax.x - labelMin.x)
        {
            drawList->AddText(ImVec2((labelMin.x + labelMax.x - nameSize.x) * 0.5f, labelMin.y),
                              ImGui::GetColorU32(ImGuiCol_Text), asset.name.c_str());
        }
        else
        {
            ImGui::RenderTextEllipsis(drawList, labelMin, labelMax, labelMax.x, asset.name.c_str(), nullptr, &nameSize);
        }

        ImGui::EndGroup();
        ImGui::PopID();

        // The play and star buttons have their own tooltips
        if (hovered && !playHovered && !starHovered && !ImGui::GetDragDropPayload())
        {
            DrawTooltip(asset, preview);
        }
        return clicked;
    }

    void AssetBrowserPanel::DrawTooltip(const AssetEntry& asset, const AssetPreviews::Preview* preview) const
    {
        if (!ImGui::BeginTooltip())
        {
            return;
        }
        if (preview && preview->textureId != 0)
        {
            const float size = 160.0f * ImGui::GetStyle().FontScaleMain;
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max(min.x + size, min.y + size);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_FrameBg), ImGui::GetStyle().FrameRounding);
            DrawPreviewImage(drawList, *preview, min, max);
            ImGui::Dummy(ImVec2(size, size));
        }
        ImGui::TextUnformatted(asset.name.c_str());
        if (favorites_.Contains(asset.path))
        {
            ImGui::SameLine();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kFavoriteColor), "(favorite)");
        }
        ImGui::TextDisabled("%s", asset.path.c_str());
        if (const auto used = sceneUses_.find(asset.path); used != sceneUses_.end())
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kInSceneColor), "Used by %u %s in the scene",
                               used->second, used->second == 1 ? "object" : "objects");
        }
        if (preview && !preview->details.empty())
        {
            ImGui::Text("%s, %s", ToString(asset.kind), preview->details.c_str());
        }
        else
        {
            ImGui::TextUnformatted(ToString(asset.kind));
        }

        if (asset.kind != AssetKind::Audio && !preview)
        {
            ImGui::TextDisabled("Making the preview...");
        }
        if (target_)
        {
            ImGui::Text("Click to use as %s's %s", target_->objectName.c_str(), UseName(asset.kind));
        }
        if (asset.kind == AssetKind::Audio && audio_)
        {
            ImGui::TextDisabled("Press the play button to listen");
        }
        ImGui::EndTooltip();
    }

    bool AssetBrowserPanel::DrawPlayButton(const AssetEntry& asset, const ImVec2& center, float diameter)
    {
        const float radius = diameter * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(center.x - radius, center.y - radius));
        if (ImGui::InvisibleButton("##play", ImVec2(diameter, diameter)))
        {
            TogglePreviewSound(asset);
        }
        BeginAssetDrag(asset); // it covers the middle of the tile, where a drag is most likely to start
        const bool hovered = ImGui::IsItemHovered();
        const bool playing = IsPreviewPlaying(asset);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddCircleFilled(center, radius, IM_COL32(0, 0, 0, hovered ? 170 : 110));
        const float s = radius * 0.45f;
        if (playing)
        {
            drawList->AddRectFilled(ImVec2(center.x - s * 0.8f, center.y - s * 0.8f),
                                    ImVec2(center.x + s * 0.8f, center.y + s * 0.8f), IM_COL32_WHITE, 2.0f);
        }
        else
        {
            // Nudged right so the triangle looks centred in the circle
            drawList->AddTriangleFilled(ImVec2(center.x - s * 0.6f, center.y - s),
                                        ImVec2(center.x - s * 0.6f, center.y + s), ImVec2(center.x + s, center.y),
                                        IM_COL32_WHITE);
        }
        if (hovered)
        {
            ImGui::SetTooltip(playing ? "Stop %s" : "Play %s", asset.name.c_str());
        }
        return hovered;
    }

    void AssetBrowserPanel::TogglePreviewSound(const AssetEntry& asset)
    {
        if (!audio_)
        {
            return;
        }
        const bool wasPlaying = IsPreviewPlaying(asset);
        StopPreviewSound();
        if (wasPlaying)
        {
            return;
        }
        const Engine::Audio::SoundId sound = audio_->LoadSound((projectDir_ / asset.path).string());
        if (!sound.IsValid())
        {
            LOG_WARN(Editor, "Assets: could not play {}", asset.path);
            return;
        }
        previewVoice_ = audio_->Play(sound);
        previewPath_ = asset.path;
        LOG_DEBUG(Editor, "Assets: playing {}", asset.path);
    }

    void AssetBrowserPanel::StopPreviewSound()
    {
        if (audio_ && previewVoice_.IsValid())
        {
            audio_->Stop(previewVoice_);
        }
        previewVoice_ = {};
        previewPath_.clear();
    }

    bool AssetBrowserPanel::IsPreviewPlaying(const AssetEntry& asset) const
    {
        return audio_ && previewPath_ == asset.path && audio_->IsPlaying(previewVoice_);
    }

    bool AssetBrowserPanel::DrawFavoriteButton(const AssetEntry& asset, const ImVec2& center, float radius,
                                               bool tileHovered)
    {
        // A little bigger than the star, so it is easy to hit
        const float size = radius * 2.0f + 4.0f;
        ImGui::SetCursorScreenPos(ImVec2(center.x - size * 0.5f, center.y - size * 0.5f));
        if (ImGui::InvisibleButton("##favorite", ImVec2(size, size)))
        {
            ToggleFavorite(asset);
        }
        BeginAssetDrag(asset); // the tile can be dragged from anywhere on it
        const bool hovered = ImGui::IsItemHovered();
        const bool favorite = favorites_.Contains(asset.path);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (favorite || tileHovered || hovered)
        {
            // A dark disc behind it keeps the star visible on light pictures
            drawList->AddCircleFilled(center, radius + 2.0f, IM_COL32(0, 0, 0, 120));
            DrawStar(drawList, center, radius, favorite ? kFavoriteColor : IM_COL32(255, 255, 255, hovered ? 255 : 170),
                     favorite);
        }
        if (hovered)
        {
            ImGui::SetTooltip(favorite ? "Remove from favorites" : "Add to favorites");
        }
        return hovered;
    }

    void AssetBrowserPanel::DrawTileMenu(const AssetEntry& asset)
    {
        if (!ImGui::BeginPopupContextItem("##tileMenu"))
        {
            return;
        }
        ImGui::TextDisabled("%s", asset.name.c_str());
        ImGui::Separator();
        if (ImGui::MenuItem(favorites_.Contains(asset.path) ? "Remove from favorites" : "Add to favorites"))
        {
            ToggleFavorite(asset);
        }
        if (ImGui::MenuItem("Copy path"))
        {
            ImGui::SetClipboardText(asset.path.c_str());
        }
        if (asset.kind == AssetKind::Material && onEditMaterial_ && ImGui::MenuItem("Edit material"))
        {
            onEditMaterial_(asset.path);
        }
        if (asset.kind == AssetKind::Shader || asset.kind == AssetKind::Material)
        {
            std::string error;
            if (asset.kind == AssetKind::Shader && ImGui::MenuItem("Open .frag in code editor"))
            {
                if (!OpenWithDefaultApp(projectDir_ / std::filesystem::path(asset.path).replace_extension(".frag"),
                                        error))
                {
                    LOG_WARN(Editor, "Could not open the shader: {}", error);
                }
            }
            if (ImGui::MenuItem(asset.kind == AssetKind::Shader ? "Open .vert in code editor" : "Open in code editor"))
            {
                if (!OpenWithDefaultApp(projectDir_ / asset.path, error))
                {
                    LOG_WARN(Editor, "Could not open {}: {}", asset.name, error);
                }
            }
        }
        ImGui::EndPopup();
    }

    void AssetBrowserPanel::ToggleFavorite(const AssetEntry& asset)
    {
        favorites_.Toggle(asset.path);
        std::string error;
        if (!favorites_.Save(projectDir_, error))
        {
            LOG_WARN(Editor, "Assets: {}", error);
        }
    }

    const std::string* AssetBrowserPanel::TargetUses(AssetKind kind) const
    {
        if (!target_)
        {
            return nullptr;
        }
        switch (kind)
        {
        case AssetKind::Model: return target_->model.empty() ? &target_->mesh : &target_->model;
        case AssetKind::Texture: return &target_->texture;
        case AssetKind::Audio: return &target_->clip;
        case AssetKind::Material: return &target_->material;
        case AssetKind::Shader: return nullptr; // a shader is used through a material
        }
        return nullptr;
    }

    void AssetBrowserPanel::PickAndImport(bool soundsOnly)
    {
        if (!CanPickFiles())
        {
            importPath_.fill('\0');
            ImGui::OpenPopup("##importFiles");
            return;
        }
        const auto list = [](const auto& extensions)
        { return std::vector<std::string_view>(extensions.begin(), extensions.end()); };
        const FileFilter sounds{"Sounds", list(Engine::Game::kAudioExtensions)};
        std::vector<FileFilter> filters;
        if (soundsOnly)
        {
            filters = {sounds};
        }
        else
        {
            std::vector<std::string_view> everything = list(Engine::Game::kMeshExtensions);
            for (const auto& more : {list(Engine::Game::kTextureExtensions), list(Engine::Game::kAudioExtensions)})
            {
                everything.insert(everything.end(), more.begin(), more.end());
            }
            // A model pack (.zip) is converted like a model
            std::vector<std::string_view> models = list(Engine::Game::kMeshExtensions);
            models.emplace_back(".zip");
            everything.emplace_back(".zip");
            filters = {{"Pictures, 3D models and sounds", everything},
                       {"Pictures", list(Engine::Game::kTextureExtensions)},
                       {"3D models", models},
                       sounds};
        }
        const std::vector<std::filesystem::path> files =
            PickFiles(soundsOnly ? "Add sounds to your game" : "Bring files into your game", filters);
        if (files.empty())
        {
            return; // cancelled
        }
        if (onImportFiles_)
        {
            onImportFiles_(files);
            return;
        }
        std::vector<std::string> errors;
        const std::size_t count = Import(files, errors);
        for (const std::string& error : errors)
        {
            LOG_WARN(Editor, "Import: {}", error);
        }
        LOG_INFO(Editor, "Imported {} file{}", count, count == 1 ? "" : "s");
    }
}
