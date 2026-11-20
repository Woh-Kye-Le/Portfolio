/* !
@file               AssetLayout.cpp
@author             w.kyele@digipen.edu [100%]
@date               30/10/2026
@brief
    Implements the migration of a game's assets/ folder to the current layout,
    by type: art/models/<name>/ for each model and its originals,
    art/textures/<name>/ for its images, art/sprites, art/spritesheets, with
    every path in scenes, prefabs, .mtl and .amodel files updated to match.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/game/AssetLayout.hpp"

// Engine Headers
#include "engine/assets/loaders/AtelierModel.hpp"
#include "engine/game/GameProject.hpp"

// Standard Library Headers
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <set>
#include <span>
#include <sstream>
#include <string_view>
#include <system_error>

namespace Engine::Game
{
    namespace
    {
        namespace fs = std::filesystem;

        std::string ToLower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        std::string Forward(std::string text)
        {
            std::replace(text.begin(), text.end(), '\\', '/');
            return text;
        }

        bool HasExtension(const fs::path& file, std::span<const std::string_view> extensions)
        {
            const std::string extension = ToLower(file.extension().string());
            return std::find(extensions.begin(), extensions.end(), extension) != extensions.end();
        }

        bool IsImage(const fs::path& file)
        {
            return HasExtension(file, kTextureExtensions);
        }

        bool IsModel(const fs::path& file)
        {
            return HasExtension(file, kMeshExtensions);
        }

        bool IsHidden(const fs::path& file)
        {
            const std::string name = file.filename().string();
            return !name.empty() && name.front() == '.';
        }

        // Regular files under folder (recursively, hidden ones left out), sorted. Empty if the folder is missing.
        std::vector<fs::path> FilesUnder(const fs::path& folder)
        {
            std::vector<fs::path> files;
            std::error_code ec;
            for (fs::recursive_directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec))
            {
                if (it->is_directory(ec) && IsHidden(it->path()))
                {
                    it.disable_recursion_pending();
                }
                else if (it->is_regular_file(ec) && !IsHidden(it->path()))
                {
                    files.push_back(it->path().lexically_normal());
                }
            }
            std::sort(files.begin(), files.end());
            return files;
        }

        std::string ReadText(const fs::path& file)
        {
            std::ifstream stream(file, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        }

        bool WriteText(const fs::path& file, const std::string& text)
        {
            std::ofstream stream(file, std::ios::binary);
            stream.write(text.data(), static_cast<std::streamsize>(text.size()));
            return static_cast<bool>(stream);
        }

        bool SameContents(const fs::path& a, const fs::path& b)
        {
            std::error_code ec;
            if (fs::file_size(a, ec) != fs::file_size(b, ec) || ec)
            {
                return false;
            }
            std::ifstream fa(a, std::ios::binary);
            std::ifstream fb(b, std::ios::binary);
            return std::equal(std::istreambuf_iterator<char>(fa), std::istreambuf_iterator<char>(),
                              std::istreambuf_iterator<char>(fb));
        }

        std::vector<std::string> Words(const std::string& line)
        {
            std::istringstream stream(line);
            return {std::istream_iterator<std::string>(stream), std::istream_iterator<std::string>()};
        }

        // Whether a line of an MTL file names an image, and so ends with its path
        bool NamesAnImage(const std::vector<std::string>& words)
        {
            return words.size() > 1 &&
                   (words[0].rfind("map_", 0) == 0 || words[0] == "bump" || words[0] == "norm" || words[0] == "decal");
        }

        // The material libraries an OBJ names with mtllib that exist beside it
        std::vector<fs::path> MaterialLibraries(const fs::path& obj)
        {
            std::vector<fs::path> libraries;
            std::ifstream stream(obj);
            for (std::string line; std::getline(stream, line);)
            {
                const auto words = Words(line);
                for (std::size_t i = 1; words.size() > 1 && words[0] == "mtllib" && i < words.size(); ++i)
                {
                    const fs::path library = (obj.parent_path() / Forward(words[i])).lexically_normal();
                    std::error_code ec;
                    if (fs::is_regular_file(library, ec) &&
                        std::find(libraries.begin(), libraries.end(), library) == libraries.end())
                    {
                        libraries.push_back(library);
                    }
                }
            }
            return libraries;
        }

        // Whether the file's bytes contain text (an FBX holds the file names of its images as plain text)
        bool Mentions(const fs::path& file, const std::string& text)
        {
            const std::string bytes = ReadText(file);
            return !text.empty() && std::search(bytes.begin(), bytes.end(), text.begin(), text.end()) != bytes.end();
        }

        // Replaces every occurrence of "from" (with its quotes) by "to" in a text; true if anything changed
        bool ReplaceQuoted(std::string& text, const std::string& from, const std::string& to)
        {
            const std::string needle = "\"" + from + "\"";
            const std::string replacement = "\"" + to + "\"";
            bool changed = false;
            for (std::size_t at = text.find(needle); at != std::string::npos;
                 at = text.find(needle, at + replacement.size()))
            {
                text.replace(at, needle.size(), replacement);
                changed = true;
            }
            return changed;
        }

        std::string Relative(const fs::path& gameDir, const fs::path& file)
        {
            return file.lexically_normal().lexically_relative(gameDir.lexically_normal()).generic_string();
        }

        // Whether a folder is named after a file: crate for crate.obj, or crate_2 (a second model or image of that name)
        bool NamedAfter(const fs::path& folder, const fs::path& file)
        {
            const std::string name = ToLower(folder.filename().string());
            const std::string stem = ToLower(file.stem().string());
            if (name == stem)
            {
                return true;
            }
            const std::string number = name.size() > stem.size() + 1 && name.rfind(stem + "_", 0) == 0
                                           ? name.substr(stem.size() + 1)
                                           : std::string{};
            return !number.empty() &&
                   std::all_of(number.begin(), number.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
        }

        // The folders of assets/models already in the current layout: named after a model or an image in them
        // (<name>/<name>.amodel, wood/wood.png, wood_2/wood.png)
        std::set<fs::path> OrganisedFolders(const fs::path& modelsRoot, const std::vector<fs::path>& files)
        {
            std::set<fs::path> organised;
            for (const fs::path& file : files)
            {
                const fs::path folder = file.parent_path();
                if ((IsModel(file) || IsImage(file)) && folder != modelsRoot.lexically_normal() &&
                    NamedAfter(folder, file))
                {
                    organised.insert(folder);
                }
            }
            return organised;
        }

        // Whether a file is inside one of the organised folders (or a folder in one)
        bool InOrganised(const fs::path& file, const fs::path& modelsRoot, const std::set<fs::path>& organised)
        {
            const fs::path top = modelsRoot.lexically_normal();
            for (fs::path folder = file.parent_path();
                 !folder.empty() && folder != top && folder != folder.parent_path(); folder = folder.parent_path())
            {
                if (organised.contains(folder))
                {
                    return true;
                }
            }
            return false;
        }

        // The files of a kind under assets/models that are not in an organised folder yet
        std::vector<fs::path> LooseFiles(const fs::path& modelsRoot, bool (*kind)(const fs::path&))
        {
            const std::vector<fs::path> files = FilesUnder(modelsRoot);
            const std::set<fs::path> organised = OrganisedFolders(modelsRoot, files);
            std::vector<fs::path> loose;
            for (const fs::path& file : files)
            {
                if (kind(file) && !InOrganised(file, modelsRoot, organised))
                {
                    loose.push_back(file);
                }
            }
            return loose;
        }

        std::vector<fs::path> LooseModels(const fs::path& modelsRoot)
        {
            return LooseFiles(modelsRoot, IsModel);
        }

        // A copy of a path with a number before the extension: crate.png -> crate_2.png
        fs::path Numbered(const fs::path& file, int n)
        {
            return file.parent_path() / (file.stem().string() + "_" + std::to_string(n) + file.extension().string());
        }

        // The file that holds the contents of source in folder: <folder>/<name> if that is free or already identical,
        // otherwise the first free numbered name
        fs::path DestinationIn(const fs::path& folder, const fs::path& source)
        {
            fs::path target = folder / source.filename();
            std::error_code ec;
            for (int n = 2; fs::exists(target, ec) && !SameContents(source, target); ++n)
            {
                target = Numbered(folder / source.filename(), n);
            }
            return target;
        }

        // Gives each image path of a .mtl file a new text: the paths were written for oldDir, the file now sits in
        // newDir, and newPathOf answers where an image (as it was) is now, or an empty path if it did not move
        using NewPathOf = std::function<fs::path(const fs::path& oldImage, const std::string& written)>;

        std::string NewReference(const std::string& written, const fs::path& oldDir, const fs::path& newDir,
                                 const NewPathOf& newPathOf)
        {
            const fs::path now = newPathOf((oldDir / Forward(written)).lexically_normal(), written);
            if (now.empty())
            {
                return {};
            }
            const std::string relative = now.lexically_relative(newDir).generic_string();
            return relative == written ? std::string{} : relative;
        }

        bool RewriteMaterialLibrary(const fs::path& file, const fs::path& oldDir, const NewPathOf& newPathOf)
        {
            const std::string text = ReadText(file);
            std::string rewritten;
            bool changed = false;
            for (std::size_t start = 0; start < text.size();)
            {
                const std::size_t end = text.find('\n', start);
                std::string line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
                start = end == std::string::npos ? text.size() : end + 1;

                const auto words = Words(line);
                if (NamesAnImage(words))
                {
                    const std::string& path = words.back();
                    const std::string now = NewReference(path, oldDir, file.parent_path(), newPathOf);
                    if (!now.empty())
                    {
                        line.replace(line.rfind(path), path.size(), now);
                        changed = true;
                    }
                }
                rewritten += line;
                if (end != std::string::npos)
                {
                    rewritten += '\n';
                }
            }
            return changed && WriteText(file, rewritten);
        }

        // A .amodel: its image paths are rewritten the same way, and the file written again with its checksum
        bool RewriteModel(const fs::path& file, const fs::path& oldDir, const NewPathOf& newPathOf)
        {
            const std::string text = ReadText(file);
            auto model = Assets::ParseAtelierModel(std::vector<std::uint8_t>(text.begin(), text.end()));
            if (!model)
            {
                return false;
            }
            bool changed = false;
            for (Assets::MaterialData& material : model->materials)
            {
                for (std::string* name :
                     {&material.baseColorTexture, &material.normalTexture, &material.metallicRoughnessTexture})
                {
                    const std::string now =
                        name->empty() ? std::string{} : NewReference(*name, oldDir, file.parent_path(), newPathOf);
                    if (!now.empty())
                    {
                        *name = now;
                        changed = true;
                    }
                }
            }
            if (!changed)
            {
                return false;
            }
            const auto bytes = Assets::WriteAtelierModel(*model);
            return WriteText(file, std::string(bytes.begin(), bytes.end()));
        }

        // Moves a file (copy and remove when a rename cannot be done)
        void MoveFile(const fs::path& from, const fs::path& to, std::error_code& ec)
        {
            fs::rename(from, to, ec);
            if (ec)
            {
                ec.clear();
                fs::copy_file(from, to, ec);
                if (!ec)
                {
                    fs::remove(from, ec);
                }
            }
        }

        // Whether path is folder or inside it
        bool IsUnder(const fs::path& path, const fs::path& folder)
        {
            const fs::path relative = path.lexically_normal().lexically_relative(folder.lexically_normal());
            return !relative.empty() && relative.generic_string().rfind("..", 0) != 0;
        }

        // Where a file has to go, and for which model (whose images it is), "" for none
        struct Target
        {
            fs::path folder;
            std::string group;
        };

        // A .mtl or .amodel whose image paths may need rewriting once everything has moved
        struct Reference
        {
            fs::path file; // where it was
            std::string group;
        };
    }

    bool NeedsLayoutMigration(const fs::path& gameDir)
    {
        const fs::path assets = gameDir / "assets";
        std::error_code ec;
        for (const char* old : {"models", "textures", "sprites", "spritesheets"})
        {
            if (!FilesUnder(assets / old).empty())
            {
                return true;
            }
        }
        const fs::path models = gameDir / kModelsFolder;
        const std::vector<fs::path> files = FilesUnder(models);
        return fs::is_directory(assets / "materials", ec) ||
               std::any_of(files.begin(), files.end(), [](const fs::path& file) { return IsImage(file); }) ||
               !LooseModels(models).empty();
    }

    LayoutMigration MigrateAssetLayout(const fs::path& gameDir)
    {
        LayoutMigration report;
        const fs::path root = gameDir.lexically_normal();
        const fs::path assets = root / "assets";
        const fs::path newModels = (root / kModelsFolder).lexically_normal();
        const fs::path newTextures = (root / kTexturesFolder).lexically_normal();
        std::error_code ec;
        if (!fs::is_directory(assets, ec))
        {
            return report;
        }

        // Where things were: the models folder of each earlier layout and of this one
        const std::vector<fs::path> modelRoots{(assets / "models").lexically_normal(), newModels};
        std::map<fs::path, std::vector<Target>> needs; // file -> where it has to be, in order (later ones get copies)
        std::vector<Reference> references;
        const auto need = [&needs](const fs::path& file, const fs::path& to, const std::string& group)
        {
            // Without a trailing separator ("crate/" when a folder was joined with an empty path)
            const fs::path folder = (to / "x").lexically_normal().parent_path();
            if (file.lexically_normal().parent_path() == folder)
            {
                return; // already there
            }
            std::vector<Target>& targets = needs[file.lexically_normal()];
            const bool known =
                std::any_of(targets.begin(), targets.end(), [&folder](const Target& t) { return t.folder == folder; });
            if (!known)
            {
                targets.push_back({folder, group});
            }
        };

        // The images a loose model can use: the old textures folder, and images loose in a models folder
        std::vector<fs::path> pool;
        for (const fs::path& file : FilesUnder(assets / "textures"))
        {
            if (IsImage(file))
            {
                pool.push_back(file);
            }
        }
        for (const fs::path& modelsRoot : modelRoots)
        {
            for (const fs::path& file : LooseFiles(modelsRoot, IsImage))
            {
                pool.push_back(file);
            }
        }
        const auto findImage = [&pool](const fs::path& wanted) -> fs::path
        {
            const fs::path normal = wanted.lexically_normal();
            if (std::find(pool.begin(), pool.end(), normal) != pool.end())
            {
                return normal;
            }
            // Written without its folder (map_Kd crate.png) while the image is in the textures folder
            const auto byName =
                std::find_if(pool.begin(), pool.end(), [&wanted](const fs::path& image)
                             { return ToLower(image.filename().string()) == ToLower(wanted.filename().string()); });
            return byName == pool.end() ? fs::path{} : *byName;
        };

        std::set<fs::path> taken; // pool images some model takes
        for (const fs::path& modelsRoot : modelRoots)
        {
            const std::vector<fs::path> files = FilesUnder(modelsRoot);
            const std::set<fs::path> organised = OrganisedFolders(modelsRoot, files);

            // A folder per model (the per-model layout, or this one): its images go to textures/<name>/, the rest to
            // models/<name>/; a folder holding only an image (a 3D texture of its own) goes to textures/
            for (const fs::path& folder : organised)
            {
                if (InOrganised(folder, modelsRoot, organised))
                {
                    continue; // inside another organised folder: moved with it
                }
                const fs::path inRoot = folder.lexically_relative(modelsRoot);
                const std::string group = Relative(root, newModels / inRoot);
                const bool hasModel = std::any_of(files.begin(), files.end(), [&](const fs::path& file)
                                                  { return IsModel(file) && IsUnder(file, folder); });
                for (const fs::path& file : files)
                {
                    if (!IsUnder(file, folder))
                    {
                        continue;
                    }
                    const fs::path within = file.parent_path().lexically_relative(folder);
                    if (IsImage(file))
                    {
                        need(file, hasModel ? (newTextures / inRoot / within) : newTextures, group);
                    }
                    else
                    {
                        need(file, newModels / inRoot / within, group);
                    }
                    const std::string extension = ToLower(file.extension().string());
                    if (extension == ".mtl" || extension == Assets::kAtelierModelExtension)
                    {
                        references.push_back({file, group});
                    }
                }
            }

            // A model on its own (the first layout): it gets models/<name>/, its images textures/<name>/
            for (const fs::path& model : LooseModels(modelsRoot))
            {
                const fs::path inRoot = model.parent_path().lexically_relative(modelsRoot) / model.stem();
                const fs::path modelFolder = newModels / inRoot;
                const fs::path textureFolder = newTextures / inRoot;
                const std::string group = Relative(root, modelFolder);
                need(model, modelFolder, group);
                const std::string extension = ToLower(model.extension().string());
                const auto takeImage = [&](const fs::path& wanted)
                {
                    const fs::path image = findImage(wanted);
                    if (!image.empty())
                    {
                        need(image, textureFolder, group);
                        taken.insert(image);
                    }
                };
                if (extension == ".obj")
                {
                    for (const fs::path& library : MaterialLibraries(model))
                    {
                        need(library, modelFolder, group);
                        references.push_back({library, group});
                        std::istringstream lines(ReadText(library));
                        for (std::string line; std::getline(lines, line);)
                        {
                            const auto words = Words(line);
                            if (NamesAnImage(words))
                            {
                                takeImage(library.parent_path() / Forward(words.back()));
                            }
                        }
                    }
                }
                else if (extension == Assets::kAtelierModelExtension)
                {
                    references.push_back({model, group});
                    const std::string text = ReadText(model);
                    const auto parsed = Assets::ParseAtelierModel(std::vector<std::uint8_t>(text.begin(), text.end()));
                    const std::vector<Assets::MaterialData> none;
                    for (const Assets::MaterialData& material : parsed ? parsed->materials : none)
                    {
                        for (const std::string* name :
                             {&material.baseColorTexture, &material.normalTexture, &material.metallicRoughnessTexture})
                        {
                            if (!name->empty())
                            {
                                takeImage(model.parent_path() / Forward(*name));
                            }
                        }
                    }
                }
                else if (extension == ".fbx")
                {
                    // The engine's FBX loader does not read textures, but the file names its images
                    for (const fs::path& image : pool)
                    {
                        if (Mentions(model, image.filename().string()))
                        {
                            need(image, textureFolder, group);
                            taken.insert(image);
                        }
                    }
                }
            }
        }

        // Images no model takes are textures for 3D objects of their own: art/textures/
        for (const fs::path& image : pool)
        {
            if (!taken.contains(image))
            {
                const fs::path within =
                    IsUnder(image, assets / "textures")
                        ? image.parent_path().lexically_relative((assets / "textures").lexically_normal())
                        : fs::path{};
                need(image, newTextures / within, "");
            }
        }

        // 2D images keep their place under art/
        for (const auto& [old, now] :
             {std::pair{"sprites", kSpritesFolder}, std::pair{"spritesheets", kSpriteSheetsFolder}})
        {
            const fs::path from = (assets / old).lexically_normal();
            for (const fs::path& file : FilesUnder(from))
            {
                need(file, root / now / file.parent_path().lexically_relative(from), "");
            }
        }

        // Move (or copy) everything planned
        std::map<std::string, std::map<fs::path, fs::path>> placed; // model -> file as it was -> where it is now
        std::map<fs::path, fs::path> movedTo;                       // file as it was -> its first new place
        for (const auto& [from, targets] : needs)
        {
            bool first = true;
            fs::path original = from; // once moved, the copies for the other folders are made from its new place
            for (const Target& target : targets)
            {
                const fs::path to = DestinationIn(target.folder, original);
                if (to.lexically_normal() == original.lexically_normal())
                {
                    continue; // never "move" a file onto itself
                }
                const bool already = fs::exists(to, ec) && SameContents(original, to);
                fs::create_directories(to.parent_path(), ec);
                ec.clear();
                if (!already)
                {
                    if (first)
                    {
                        MoveFile(from, to, ec);
                    }
                    else
                    {
                        fs::copy_file(original, to, ec);
                    }
                }
                else if (first)
                {
                    fs::remove(from, ec); // the same file is already there
                }
                if (ec)
                {
                    report.warnings.push_back("could not move " + Relative(root, from) + ": " + ec.message());
                    ec.clear();
                    continue;
                }
                placed[target.group][from] = to;
                if (first)
                {
                    report.moved.emplace_back(Relative(root, from), Relative(root, to));
                    movedTo[from] = to;
                    original = to;
                    first = false;
                }
            }
        }

        // The image paths inside .mtl and .amodel files follow the images
        for (const Reference& reference : references)
        {
            const auto moved = movedTo.find(reference.file);
            const fs::path now = moved == movedTo.end() ? reference.file : moved->second;
            const std::map<fs::path, fs::path>& mine = placed[reference.group];
            const NewPathOf newPathOf = [&](const fs::path& oldImage, const std::string& written) -> fs::path
            {
                if (const auto found = mine.find(oldImage); found != mine.end())
                {
                    return found->second;
                }
                // The original .mtl of a pack names its images by the pack's folders: match the name
                const std::string leaf = ToLower(fs::path(Forward(written)).filename().string());
                for (const auto& [was, is] : mine)
                {
                    if (IsImage(was) && ToLower(was.filename().string()) == leaf)
                    {
                        return is;
                    }
                }
                const auto anywhere = movedTo.find(oldImage);
                return anywhere == movedTo.end() ? fs::path{} : anywhere->second;
            };
            const std::string extension = ToLower(now.extension().string());
            const bool changed = extension == ".mtl"
                                     ? RewriteMaterialLibrary(now, reference.file.parent_path(), newPathOf)
                                     : RewriteModel(now, reference.file.parent_path(), newPathOf);
            report.rewritten += changed ? 1 : 0;
        }

        // Scenes, prefabs and the favorites name files by their path in the game
        if (!movedTo.empty())
        {
            std::vector<fs::path> texts;
            for (const char* folder : {"data", "assets"})
            {
                for (const fs::path& file : FilesUnder(root / folder))
                {
                    const std::string extension = ToLower(file.extension().string());
                    if (!IsUnder(file, newModels) &&
                        (extension == ".json" || extension == ".prefab" || extension == ".favorites"))
                    {
                        texts.push_back(file);
                    }
                }
            }
            for (const fs::path& file : texts)
            {
                std::string text = ReadText(file);
                bool changed = false;
                for (const auto& [from, to] : movedTo)
                {
                    changed = ReplaceQuoted(text, Relative(root, from), Relative(root, to)) || changed;
                }
                if (changed && WriteText(file, text))
                {
                    ++report.rewritten;
                }
            }
        }

        // The folders of the earlier layouts go when nothing is left in them
        for (const char* legacy : {"models", "textures", "materials", "sprites", "spritesheets"})
        {
            const fs::path folder = assets / legacy;
            if (!fs::is_directory(folder, ec))
            {
                continue;
            }
            if (FilesUnder(folder).empty())
            {
                fs::remove_all(folder, ec);
            }
            else
            {
                report.warnings.push_back("assets/" + std::string(legacy) +
                                          " still holds files that are not assets; they were left there");
            }
        }

        // A game (not a template folder) gets the folders of the current layout
        if (GameProject::IsGameProject(root))
        {
            for (const char* folder : {kModelsFolder, kTexturesFolder, kSpritesFolder, kSpriteSheetsFolder,
                                       kAudioFolder, kPrefabsFolder, kMaterialsFolder, kShadersFolder})
            {
                if (!fs::exists(root / folder, ec) && fs::create_directories(root / folder, ec))
                {
                    WriteText(root / folder / ".gitkeep", "");
                }
            }
        }
        return report;
    }
}
