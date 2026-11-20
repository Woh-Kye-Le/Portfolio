/* !
@file    	    GameProject.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		26/09/2026
@brief
    Implements GameProject: reading/writing game.cfg, loading and saving scenes,
    finding games and assets, importing files, and creating new games from the
    empty or demo templates.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/game/GameProject.hpp"
#include "engine/logic/Library.hpp"

#include "engine/assets/MaterialFile.hpp"
#include "engine/assets/loaders/AtelierModel.hpp"
#include "engine/input/InputActions.hpp"
#include "engine/platform/Paths.hpp"
#include "engine/serializer/CfgSerializer.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace Engine::Game
{
    namespace
    {
        constexpr const char* kConfigFile = "configs/game.cfg";
        constexpr const char* kInputActionsFile = "configs/input_actions.json";
        constexpr const char* kMainScene = "data/scenes/main.json";
        constexpr const char* kMenuScene = "data/scenes/menu.json"; // a template's main menu, where its game starts

        std::string ToLower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        // A number from game.cfg; a missing key or text that is not a number keeps value as it is
        void ReadNumber(Serializer::CFGSerializer& cfg, const char* key, float& value)
        {
            if (!cfg.HasKey(key))
            {
                return;
            }
            std::string text;
            cfg.ReadString(key, text);
            // The whole text has to be the number ("9.8 m/s" is not), and a finite one
            float number = 0.0f;
            const char* end = text.data() + text.size();
            const auto [last, error] = std::from_chars(text.data(), end, number);
            if (error == std::errc{} && last == end && std::isfinite(number))
            {
                value = number;
            }
        }

        bool SameContents(const std::filesystem::path& a, const std::filesystem::path& b)
        {
            std::error_code sizeError;
            if (std::filesystem::file_size(a, sizeError) != std::filesystem::file_size(b, sizeError) || sizeError)
            {
                return false;
            }
            std::ifstream fa(a, std::ios::binary);
            std::ifstream fb(b, std::ios::binary);
            return std::equal(std::istreambuf_iterator<char>(fa), std::istreambuf_iterator<char>(),
                              std::istreambuf_iterator<char>(fb));
        }

        // configs/game.cfg: missing keys, invalid window sizes and invalid numbers keep their defaults
        void ReadSettings(const std::filesystem::path& path, GameSettings& settings)
        {
            if (!std::filesystem::is_regular_file(path))
            {
                return;
            }

            Serializer::CFGSerializer cfg;
            cfg.LoadFile(path.string());

            if (cfg.HasKey("name"))
            {
                cfg.ReadString("name", settings.name);
            }
            if (cfg.HasKey("start_scene"))
            {
                cfg.ReadString("start_scene", settings.startScene);
            }
            for (auto [key, value] :
                 {std::pair{"window_width", &settings.windowWidth}, std::pair{"window_height", &settings.windowHeight}})
            {
                int number = 0;
                cfg.ReadInt(key, number); // 0 if missing or not a number
                if (number > 0)
                {
                    *value = number;
                }
            }
            ReadNumber(cfg, "gravity_x", settings.gravityX);
            ReadNumber(cfg, "gravity_y", settings.gravityY);
            ReadNumber(cfg, "gravity_z", settings.gravityZ);
        }

        void WriteSettings(const std::filesystem::path& path, const GameSettings& settings)
        {
            Serializer::CFGSerializer cfg;
            cfg.NewFile(path.string());
            cfg.WriteString("name", settings.name);
            cfg.WriteInt("window_width", settings.windowWidth);
            cfg.WriteInt("window_height", settings.windowHeight);
            cfg.WriteString("start_scene", settings.startScene);
            // As text: WriteFloat keeps only one decimal, which would save Earth's -9.81 as -9.8
            cfg.WriteString("gravity_x", std::format("{}", settings.gravityX));
            cfg.WriteString("gravity_y", std::format("{}", settings.gravityY));
            cfg.WriteString("gravity_z", std::format("{}", settings.gravityZ));
        }

        // The template's scene: empty, or the demo scene of that name. Throws std::runtime_error if it cannot be read.
        Scene::SceneDescription TemplateScene(GameTemplate gameTemplate, const std::filesystem::path& templatesDir)
        {
            if (gameTemplate == GameTemplate::Empty)
            {
                return {};
            }
            const auto file = templatesDir / "data" / "scenes" / (std::string(ToString(gameTemplate)) + ".json");
            Scene::SceneDescription scene;
            const auto report = Scene::LoadSceneFile(file, scene);
            if (!report.ok)
            {
                throw std::runtime_error(
                    std::format("The {} template could not be read: {}", ToString(gameTemplate), report.error));
            }
            return scene;
        }

        // The words of one line of an OBJ or MTL file
        std::vector<std::string> Words(const std::string& line)
        {
            std::istringstream stream(line);
            return {std::istream_iterator<std::string>(stream), std::istream_iterator<std::string>()};
        }

        // The texture files a .amodel names (its materials' images), as the paths written in the file, which are
        // relative to the model. Empty if the file cannot be read or is not a valid .amodel.
        std::vector<std::string> AmodelTextureNames(const std::filesystem::path& file)
        {
            std::ifstream stream(file, std::ios::binary);
            const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)),
                                                  std::istreambuf_iterator<char>());
            std::vector<std::string> names;
            if (const auto model = Assets::ParseAtelierModel(bytes))
            {
                for (const Assets::MaterialData& material : model->materials)
                {
                    for (const std::string* name :
                         {&material.baseColorTexture, &material.normalTexture, &material.metallicRoughnessTexture})
                    {
                        if (!name->empty())
                        {
                            names.push_back(*name);
                        }
                    }
                }
            }
            return names;
        }

        // The files a model pulls in besides itself. An OBJ: the material libraries it names with mtllib and the
        // images those libraries name (map_Kd, bump, ...). A .amodel: the images its materials name. All are found
        // next to the model, as the loader looks for them. Paths are relative to gameDir, like the scene's own; a
        // name that points outside the game folder is left out.
        void AddModelCompanions(const std::filesystem::path& gameDir, const std::filesystem::path& model,
                                std::vector<std::filesystem::path>& files)
        {
            const std::string modelExtension = ToLower(model.extension().string());
            if (modelExtension != ".obj" && modelExtension != Assets::kAtelierModelExtension)
            {
                return;
            }
            const auto modelDir = model.parent_path();
            const auto beside = [&modelDir](std::string name) -> std::filesystem::path
            {
                std::replace(name.begin(), name.end(), '\\', '/');
                auto path = (modelDir / name).lexically_normal();
                return path.empty() || path.is_absolute() || *path.begin() == ".." ? std::filesystem::path{} : path;
            };

            if (modelExtension == Assets::kAtelierModelExtension)
            {
                for (const std::string& name : AmodelTextureNames(gameDir / model))
                {
                    if (auto image = beside(name); !image.empty())
                    {
                        files.push_back(std::move(image));
                    }
                }
                return;
            }

            std::vector<std::filesystem::path> libraries;
            std::ifstream obj(gameDir / model);
            for (std::string line; std::getline(obj, line);)
            {
                const auto words = Words(line);
                for (std::size_t i = 1; words.size() > 1 && words[0] == "mtllib" && i < words.size(); ++i)
                {
                    if (auto library = beside(words[i]); !library.empty())
                    {
                        libraries.push_back(std::move(library));
                    }
                }
            }

            for (const auto& library : libraries)
            {
                files.push_back(library);
                std::ifstream mtl(gameDir / library);
                for (std::string line; std::getline(mtl, line);)
                {
                    const auto words = Words(line);
                    const bool namesAnImage = words.size() > 1 && (words[0].rfind("map_", 0) == 0 ||
                                                                   words[0] == "bump" || words[0] == "norm");
                    if (namesAnImage)
                    {
                        if (auto image = beside(words.back()); !image.empty())
                        {
                            files.push_back(std::move(image));
                        }
                    }
                }
            }
        }

        // Copies the asset files scene uses from templatesDir into gameDir, at the same relative paths
        // (a model comes with the material libraries and images it names)
        void CopyTemplateAssets(const Scene::SceneDescription& scene, const std::filesystem::path& templatesDir,
                                const std::filesystem::path& gameDir, const Logic::Library& library,
                                std::set<std::string>& usedLibrary)
        {
            std::vector<std::filesystem::path> files;
            for (const Scene::GameObject& object : scene.objects)
            {
                if (const auto& renderable = object.renderable)
                {
                    files.insert(files.end(),
                                 {renderable->mesh, renderable->model, renderable->texture, renderable->shader});
                    if (!renderable->shader.empty())
                    {
                        files.push_back(std::filesystem::path(renderable->shader).replace_extension(".frag"));
                    }
                    // Its material, and the shader and texture the material names
                    if (!renderable->material.empty())
                    {
                        files.emplace_back(renderable->material);
                        if (const auto material = Assets::ReadMaterialFile(templatesDir / renderable->material))
                        {
                            files.emplace_back(material->texture);
                            if (!material->shader.empty())
                            {
                                files.emplace_back(material->shader);
                                files.push_back(std::filesystem::path(material->shader).replace_extension(".frag"));
                            }
                        }
                    }
                }
                // The animation files its Animator plays clips of
                if (object.animator)
                {
                    files.insert(files.end(), object.animator->sources.begin(), object.animator->sources.end());
                }
                if (object.audioSource)
                {
                    files.emplace_back(object.audioSource->clip);
                }
                if (object.prefabInstance)
                {
                    files.emplace_back(object.prefabInstance->prefab); // so the new game can update its instances
                }
                // The pictures it shows, by path or by name: its screen element's, its sky's and its blocks'
                std::vector<std::string> pictures;
                if (object.screen && !object.screen->picture.empty())
                {
                    pictures.push_back(object.screen->picture);
                }
                if (object.sky && !object.sky->picture.empty())
                {
                    pictures.push_back(object.sky->picture);
                }
                // The sounds its blocks play, by path or by name, and the steps on it (its Surface's)
                std::vector<std::string> sounds;
                if (object.rigidBody && !object.rigidBody->surface.empty())
                {
                    sounds.push_back("step_" + object.rigidBody->surface);
                }
                if (object.logic)
                {
                    const auto walk = [&pictures, &sounds, &library,
                                       &usedLibrary](const auto& self, const std::vector<Logic::Block>& blocks) -> void
                    {
                        for (const Logic::Block& block : blocks)
                        {
                            if (block.op == Logic::Op::Call && usedLibrary.insert(block.text).second)
                            {
                                if (const Logic::LibraryBlock* saved = library.Find(block.text))
                                {
                                    self(self, saved->blocks);
                                }
                            }
                            if (block.op == Logic::Op::ShowScreenPicture && !block.text.empty() && block.text != "none")
                            {
                                pictures.push_back(block.text);
                            }
                            if (block.op == Logic::Op::PlaySound && !block.text.empty())
                            {
                                sounds.push_back(block.text);
                            }
                            if (block.op == Logic::Op::PlayFootstep)
                            {
                                sounds.emplace_back("step_default"); // on ground with no Surface
                            }
                            self(self, block.body);
                            self(self, block.elseBody);
                        }
                    };
                    for (const Logic::Script& script : object.logic->scripts)
                    {
                        walk(walk, script.blocks);
                    }
                }
                const auto byName = [&files, &templatesDir](const std::vector<std::string>& names,
                                                            std::span<const std::string_view> extensions)
                {
                    for (const std::string& name : names)
                    {
                        if (!std::filesystem::path(name).extension().empty())
                        {
                            files.emplace_back(name);
                            continue;
                        }
                        // The file of that name, and its numbered variants ("step_stone": step_stone_1, _2, ...)
                        const std::string wanted = ToLower(name);
                        for (const std::string& each : GameProject(templatesDir).FindAssets("assets", extensions))
                        {
                            const std::string stem = ToLower(std::filesystem::path(each).stem().string());
                            const std::string number =
                                stem.starts_with(wanted + "_") ? stem.substr(wanted.size() + 1) : std::string();
                            if (stem == wanted ||
                                (!number.empty() && std::all_of(number.begin(), number.end(),
                                                                [](unsigned char c) { return std::isdigit(c); })))
                            {
                                files.emplace_back(each);
                            }
                        }
                    }
                };
                byName(pictures, kTextureExtensions);
                byName(sounds, kAudioExtensions);
            }
            const std::size_t listed = files.size();
            for (std::size_t i = 0; i < listed; ++i)
            {
                const std::filesystem::path file = files[i]; // a copy: AddModelCompanions grows the list
                AddModelCompanions(templatesDir, file, files);
            }
            for (const auto& file : files)
            {
                std::error_code ec;
                if (file.empty() || !std::filesystem::is_regular_file(templatesDir / file, ec))
                {
                    continue;
                }
                // Many objects share a file (every crate uses crate.obj), so copy each one only once. Checked here
                // rather than with copy_options::skip_existing, which MinGW's standard library reports as a
                // "File exists" error instead of skipping.
                const auto target = gameDir / file;
                if (std::filesystem::exists(target, ec))
                {
                    continue;
                }
                std::filesystem::create_directories(target.parent_path());
                std::filesystem::copy_file(templatesDir / file, target);
            }
        }

        // Candidate folders holding the demo templates, in the order TemplatesDir() tries them
        std::vector<std::filesystem::path> TemplateSearchDirs()
        {
            std::vector<std::filesystem::path> candidates;
            if (const auto exeDir = Platform::ExecutableDir(); !exeDir.empty())
            {
                candidates.push_back(exeDir / "templates" / "demos");
            }
#ifdef GAM300_ROOT_DIR
#define ATELIER_STR(x) #x
#define ATELIER_XSTR(x) ATELIER_STR(x)
            candidates.push_back(std::filesystem::path(ATELIER_XSTR(GAM300_ROOT_DIR)) / "projects" / "atelier_editor");
#undef ATELIER_XSTR
#undef ATELIER_STR
#endif
            return candidates;
        }

        void WriteFile(const std::filesystem::path& path, const std::string& contents)
        {
            std::ofstream file(path, std::ios::binary);
            if (!file)
            {
                throw std::runtime_error("Could not write " + path.string());
            }
            file << contents;
        }

        // Candidate directories where the source builtin shaders might live. These are either the
        // source tree shaders (editor runs) or copies placed next to the EXE (built binaries).
        std::vector<std::filesystem::path> BuiltinShaderSearchDirs()
        {
            std::vector<std::filesystem::path> candidates;
#ifdef GAM300_ROOT_DIR
#define ATELIER_STR(x) #x
#define ATELIER_XSTR(x) ATELIER_STR(x)
            candidates.push_back(std::filesystem::path(ATELIER_XSTR(GAM300_ROOT_DIR)) / "projects" / "atelier_engine" /
                                 "src" / "assets" / "Shaders" / "Builtin");
#undef ATELIER_XSTR
#undef ATELIER_STR
#endif
            const auto exeDir = Platform::ExecutableDir();
            if (!exeDir.empty())
            {
                candidates.push_back(exeDir / "assets" / "shaders" / "Builtin");
                candidates.push_back(exeDir / "game" / "assets" / "shaders" / "Builtin");
            }
            return candidates;
        }

        // Copies builtin shaders into <gameDir>/assets/shaders/Builtin/.
        void CopyBuiltinShadersIntoGame(const std::filesystem::path& gameDir)
        {
            static constexpr std::array<const char*, 8> kBuiltinShaders = {
                "Standard3D.vert",    "Standard3D.frag",    "Unlit.vert", "Unlit.frag",
                "FallbackUnlit.vert", "FallbackUnlit.frag", "Sky.vert",   "Sky.frag",
            };
            static constexpr std::array<const char*, 1> kBuiltinIncludes = {
                "Common/BlinnPhong.glsl",
            };

            const auto searchDirs = BuiltinShaderSearchDirs();
            const auto destination = gameDir / "assets" / "shaders" / "Builtin";
            std::filesystem::create_directories(destination);

            std::error_code ec;
            for (const char* shaderName : kBuiltinShaders)
            {
                std::filesystem::path source;
                for (const auto& dir : searchDirs)
                {
                    const auto candidate = dir / shaderName;
                    if (std::filesystem::is_regular_file(candidate, ec))
                    {
                        source = candidate;
                        break;
                    }
                }
                if (source.empty())
                {
                    continue;
                }
                std::filesystem::copy_file(source, destination / shaderName,
                                           std::filesystem::copy_options::overwrite_existing, ec);
            }

            // GLSL includes live under Builtin/Common/ (paths relative to the including shader via
            // double-quote #include resolution in Shader::LoadFromFiles). Mirror them one directory deeper.
            const auto includeDest = destination / "Common";
            std::filesystem::create_directories(includeDest, ec);
            for (const char* includeName : kBuiltinIncludes)
            {
                std::filesystem::path source;
                for (const auto& dir : searchDirs)
                {
                    const auto candidate = dir / includeName;
                    if (std::filesystem::is_regular_file(candidate, ec))
                    {
                        source = candidate;
                        break;
                    }
                }
                if (source.empty())
                {
                    continue;
                }
                std::filesystem::copy_file(source, includeDest / std::filesystem::path(includeName).filename(),
                                           std::filesystem::copy_options::overwrite_existing, ec);
            }
        }
    }

    void CopyBuiltinShaders(const std::filesystem::path& gameDir)
    {
        CopyBuiltinShadersIntoGame(gameDir);
    }

    // Just reads game.cfg; nothing else is loaded until it's asked for
    GameProject::GameProject(std::filesystem::path projectDir) : projectDir_(std::move(projectDir))
    {
        settings_.name = projectDir_.filename().string();
        ReadSettings(projectDir_ / kConfigFile, settings_);
    }

    const std::filesystem::path& GameProject::ProjectDir() const
    {
        return projectDir_;
    }

    const GameSettings& GameProject::Settings() const
    {
        return settings_;
    }

    void GameProject::SaveSettings(const GameSettings& settings)
    {
        const auto path = projectDir_ / kConfigFile;
        std::filesystem::create_directories(path.parent_path());
        // The cfg writer gives up silently, so find out first whether the file can be written at all
        if (!std::ofstream(path, std::ios::app))
        {
            throw std::runtime_error("Could not write " + path.string());
        }
        WriteSettings(path, settings);
        settings_ = settings;
    }

    Scene::SceneDescription GameProject::LoadStartScene() const
    {
        return LoadScene(settings_.startScene);
    }

    // The forgiving version: any problem gives an empty scene (the details are in the log)
    Scene::SceneDescription GameProject::LoadScene(const std::filesystem::path& relativePath) const
    {
        Scene::SceneDescription scene;
        Scene::LoadSceneFile(projectDir_ / relativePath, scene); // problems are logged; a failed load stays empty
        return scene;
    }

    Scene::SceneLoadReport GameProject::LoadSceneChecked(const std::filesystem::path& relativePath,
                                                         Scene::SceneDescription& scene) const
    {
        return Scene::LoadSceneFile(projectDir_ / relativePath, scene);
    }

    // A folder is a game if it has configs/game.cfg - that's the only rule
    bool GameProject::IsGameProject(const std::filesystem::path& dir)
    {
        std::error_code ec;
        return std::filesystem::is_regular_file(dir / kConfigFile, ec);
    }

    std::vector<std::filesystem::path> GameProject::FindAll(const std::filesystem::path& projectsRoot)
    {
        std::vector<std::filesystem::path> projects;

        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(projectsRoot, ec))
        {
            if (entry.is_directory() && IsGameProject(entry.path()))
            {
                projects.push_back(entry.path());
            }
        }

        std::sort(projects.begin(), projects.end()); // directory order isn't guaranteed, sort so the list is stable
        return projects;
    }

    std::filesystem::path GameProject::ProjectsRoot()
    {
        // Create() makes the folder with the first game, so it may not exist yet
        return Platform::ExecutableDir() / "projects";
    }

    std::filesystem::path GameProject::FindDefaultGameProjectDir()
    {
        // ATELIER_GAME_PROJECT lets you point the runtime at any game folder without copying it (useful when testing)
        if (const char* env = std::getenv("ATELIER_GAME_PROJECT"))
        {
            const std::filesystem::path fromEnv = env;
            if (!fromEnv.empty() && std::filesystem::is_directory(fromEnv))
            {
                return fromEnv;
            }
        }

        const auto projects = FindAll(ProjectsRoot());
        return projects.empty() ? std::filesystem::path{} : projects.front();
    }

    // The name becomes a folder name, so only allow safe characters. This also blocks things like "../x"
    // that would create the game outside the projects folder.
    bool GameProject::IsValidName(std::string_view name)
    {
        if (name.empty() || name.size() > 64 || !std::isalnum(static_cast<unsigned char>(name.front())))
        {
            return false;
        }

        return std::all_of(name.begin(), name.end(),
                           [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-'; });
    }

    // First candidate folder that actually has demo scenes in it
    std::filesystem::path GameProject::TemplatesDir()
    {
        const auto candidates = TemplateSearchDirs();
        for (const auto& dir : candidates)
        {
            std::error_code ec;
            if (std::filesystem::is_directory(dir / "data" / "scenes", ec))
            {
                return dir;
            }
        }
        return candidates.empty() ? std::filesystem::path{} : candidates.front();
    }

    // Project > New Game and --new-game both end up here
    GameProject GameProject::Create(const std::filesystem::path& projectsRoot, const std::string& name,
                                    GameTemplate gameTemplate, const std::filesystem::path& templatesDir)
    {
        if (!IsValidName(name))
        {
            throw std::runtime_error("Invalid game name '" + name +
                                     "': use 1-64 letters, digits, '_' or '-', starting with a letter or digit");
        }

        const auto dir = projectsRoot / name;
        if (std::filesystem::exists(dir))
        {
            throw std::runtime_error("A folder named '" + name + "' already exists in " + projectsRoot.string());
        }

        // Read the template before making anything, so a missing one leaves no half-made game behind
        const auto source = templatesDir.empty() ? TemplatesDir() : templatesDir;
        const Scene::SceneDescription scene = TemplateScene(gameTemplate, source);
        // A template with a main menu ("<name>_menu.json" beside it): the game starts there, and its Play button goes
        // to the scene called main
        std::optional<Scene::SceneDescription> menu;
        if (gameTemplate != GameTemplate::Empty)
        {
            const auto file = source / "data" / "scenes" / (std::string(ToString(gameTemplate)) + "_menu.json");
            if (std::error_code ec; std::filesystem::is_regular_file(file, ec))
            {
                Scene::SceneDescription read;
                const auto report = Scene::LoadSceneFile(file, read);
                if (!report.ok)
                {
                    throw std::runtime_error(std::format("The {} template's menu could not be read: {}",
                                                         ToString(gameTemplate), report.error));
                }
                menu = std::move(read);
            }
        }

        // .gitkeep keeps the empty folders when the game is committed
        for (const char* folder : {kModelsFolder, kTexturesFolder, kSpritesFolder, kSpriteSheetsFolder, kAudioFolder,
                                   kPrefabsFolder, kMaterialsFolder, kShadersFolder, "scripts"})
        {
            std::filesystem::create_directories(dir / folder);
            WriteFile(dir / folder / ".gitkeep", "");
        }
        std::filesystem::create_directories(dir / "configs");
        std::filesystem::create_directories(dir / "data" / "scenes");

        GameSettings settings;
        settings.name = name;
        settings.startScene = menu ? kMenuScene : kMainScene;
        WriteSettings(dir / kConfigFile, settings);
        Input::InputActions::Defaults().Save(dir / kInputActionsFile);
        Scene::SaveSceneFile(dir / kMainScene, scene);
        Logic::Library library;
        Logic::LoadLibraryFile(source / Logic::kLibraryFile, library);
        std::set<std::string> usedLibrary;
        CopyTemplateAssets(scene, source, dir, library, usedLibrary);
        if (menu)
        {
            Scene::SaveSceneFile(dir / kMenuScene, *menu);
            CopyTemplateAssets(*menu, source, dir, library, usedLibrary);
        }
        // Only the My Blocks it uses
        std::erase_if(library.blocks,
                      [&usedLibrary](const Logic::LibraryBlock& saved) { return !usedLibrary.contains(saved.name); });
        if (!library.blocks.empty())
        {
            Logic::SaveLibraryFile(dir / Logic::kLibraryFile, library);
        }

        CopyBuiltinShadersIntoGame(dir);

        return GameProject(dir);
    }

    void GameProject::SaveScene(const std::filesystem::path& relativePath, const Scene::SceneDescription& scene) const
    {
        Scene::SaveSceneFile(projectDir_ / relativePath, scene);
    }

    void GameProject::SaveStartScene(const Scene::SceneDescription& scene) const
    {
        SaveScene(settings_.startScene, scene);
    }

    std::filesystem::path GameProject::InputActionsPath() const
    {
        return projectDir_ / kInputActionsFile;
    }

    // Used by the asset pickers in the Inspector and File > Game Scenes
    std::vector<std::string> GameProject::FindAssets(const std::filesystem::path& relativeFolder,
                                                     std::span<const std::string_view> extensions) const
    {
        std::vector<std::string> found;
        std::error_code ec;
        const auto folder = projectDir_ / relativeFolder;
        for (auto it = std::filesystem::recursive_directory_iterator(folder, ec);
             !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
        {
            if (!it->is_regular_file(ec))
            {
                continue;
            }
            const std::string extension = ToLower(it->path().extension().string());
            if (std::find(extensions.begin(), extensions.end(), extension) != extensions.end())
            {
                found.push_back(it->path().lexically_relative(projectDir_).generic_string());
            }
        }
        std::sort(found.begin(), found.end());
        return found;
    }

    std::vector<std::string> GameProject::FindModels() const
    {
        std::vector<std::string> found = FindAssets("assets", kMeshExtensions);
        // The folders with a .amodel: the .obj or .fbx beside one is what it was converted from
        std::set<std::string> converted;
        for (const std::string& path : found)
        {
            if (ToLower(std::filesystem::path(path).extension().string()) == Assets::kAtelierModelExtension)
            {
                converted.insert(std::filesystem::path(path).parent_path().generic_string());
            }
        }
        std::erase_if(found,
                      [&converted](const std::string& path)
                      {
                          const std::filesystem::path file(path);
                          return ToLower(file.extension().string()) != Assets::kAtelierModelExtension &&
                                 converted.contains(file.parent_path().generic_string());
                      });
        return found;
    }

    namespace
    {
        // A name for a new file: letters, digits, spaces, '-' and '_' only, and not empty
        std::string FileNameOf(const std::string& name, const char* fallback)
        {
            std::string clean;
            for (const unsigned char c : name)
            {
                if (std::isalnum(c) || c == ' ' || c == '-' || c == '_')
                {
                    clean.push_back(static_cast<char>(c));
                }
            }
            while (!clean.empty() && clean.back() == ' ')
            {
                clean.pop_back();
            }
            return clean.empty() ? std::string(fallback) : clean;
        }

        // folder/<name><extension>, or <name> 2, <name> 3... when that is taken
        std::filesystem::path FreeName(const std::filesystem::path& folder, const std::string& name,
                                       const char* extension)
        {
            std::error_code ec;
            std::filesystem::path path = folder / (name + extension);
            for (int copy = 2; std::filesystem::exists(path, ec); ++copy)
            {
                path = folder / (name + " " + std::to_string(copy) + extension);
            }
            return path;
        }

        std::string ReadWhole(const std::filesystem::path& path)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
            {
                return {};
            }
            std::ostringstream text;
            text << in.rdbuf();
            return text.str();
        }

        // Replaces the first `from` in text (nothing when it is not there)
        void ReplaceFirst(std::string& text, std::string_view from, std::string_view to)
        {
            if (const auto at = text.find(from); at != std::string::npos)
            {
                text.replace(at, from.size(), to);
            }
        }
    }

    std::string GameProject::CreateMaterial(const std::string& name) const
    {
        const std::filesystem::path path =
            FreeName(projectDir_ / kMaterialsFolder, FileNameOf(name, "Material"), Assets::kMaterialExtension);
        std::string error;
        if (!Assets::WriteMaterialFile(path, Assets::MaterialFile{}, &error))
        {
            throw std::runtime_error("Could not make the material: " + error);
        }
        return path.lexically_relative(projectDir_).generic_string();
    }

    std::string GameProject::CreateShader(const std::string& name) const
    {
        // Start from the engine's own standard shader, as the editor has it now
        std::string vertex;
        std::string fragment;
        for (const auto& dir : BuiltinShaderSearchDirs())
        {
            vertex = ReadWhole(dir / "Standard3D.vert");
            fragment = ReadWhole(dir / "Standard3D.frag");
            if (!vertex.empty() && !fragment.empty())
            {
                break;
            }
        }
        if (vertex.empty() || fragment.empty())
        {
            throw std::runtime_error("Could not make the shader: the engine's standard shader was not found");
        }
        // Its includes are the engine's copies in the game (assets/shaders/Builtin/), which it gets now if it has none
        CopyBuiltinShadersIntoGame(projectDir_);
        ReplaceFirst(fragment, "#include \"Common/", "#include \"Builtin/Common/");
        ReplaceFirst(
            fragment, "out vec4 o_Color;",
            "out vec4 o_Color;\n\n"
            "// Your shader's own values: a material using this shader sets them under Values, by these names\n"
            "uniform float u_Brightness = 1.0;");
        ReplaceFirst(fragment,
                     "    // Haze:", "    color *= u_Brightness; // try changing the colour here\n\n    // Haze:");

        const std::string clean = FileNameOf(name, "Shader");
        const std::filesystem::path folder = projectDir_ / kShadersFolder;
        const std::filesystem::path vertexPath = FreeName(folder, clean, ".vert");
        std::filesystem::path fragmentPath = vertexPath;
        fragmentPath.replace_extension(".frag");
        const std::string header = "// " + vertexPath.stem().string() +
                                   ": a shader of this game, made from the engine's standard lit shader.\n"
                                   "// Edit it and save: the game uses the new version straight away. Shown on the "
                                   "objects whose material picks it.\n";
        std::filesystem::create_directories(folder);
        WriteFile(vertexPath, header + vertex);
        WriteFile(fragmentPath, header + fragment);
        return vertexPath.lexically_relative(projectDir_).generic_string();
    }

    // Import in the Inspector and the Assets window: copies an image into the folder its use asks for
    std::string GameProject::ImportImage(const std::filesystem::path& source, ImageUse use) const
    {
        switch (use)
        {
        case ImageUse::Sprite: return ImportAsset(source, kSpritesFolder);
        case ImageUse::SpriteSheet: return ImportAsset(source, kSpriteSheetsFolder);
        case ImageUse::Texture3D: break;
        }
        return ImportAsset(source, kTexturesFolder);
    }

    // "Import texture" in the Inspector: copies a file from anywhere into the game
    std::string GameProject::ImportAsset(const std::filesystem::path& source,
                                         const std::filesystem::path& relativeFolder) const
    {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(source, ec))
        {
            throw std::runtime_error("Not a file: " + source.string());
        }

        const auto folder = projectDir_ / relativeFolder;
        std::filesystem::create_directories(folder);

        // Keep the file's name; on a clash with a different file, number the copy: crate.png, crate_2.png, ...
        auto target = folder / source.filename();
        bool alreadyInGame = false;
        for (int n = 2; std::filesystem::exists(target, ec); ++n)
        {
            if (std::filesystem::equivalent(source, target, ec))
            {
                return target.lexically_relative(projectDir_).generic_string(); // the game's own file
            }
            if (SameContents(source, target))
            {
                // The file is already in the game, but a model's materials and textures may not be: they are
                // brought over below all the same
                alreadyInGame = true;
                break;
            }
            target = folder / (source.stem().string() + "_" + std::to_string(n) + source.extension().string());
        }

        if (!alreadyInGame && (!std::filesystem::copy_file(source, target, ec) || ec))
        {
            throw std::runtime_error("Could not copy " + source.string() + " to " + target.string() + ": " +
                                     ec.message());
        }

        // A .amodel names its images relative to itself (../../textures/crate/crate.png): the ones it names are copied to the
        // same place beside the copy, unless they are already there. Nothing else comes with it.
        const std::string sourceExtension = ToLower(source.extension().string());
        if (sourceExtension == Assets::kAtelierModelExtension)
        {
            const auto gameDir = projectDir_.lexically_normal();
            for (const std::string& name : AmodelTextureNames(source))
            {
                const std::filesystem::path relative(name);
                const auto from = (source.parent_path() / relative).lexically_normal();
                const auto to = (target.parent_path() / relative).lexically_normal();
                const auto inside = to.lexically_relative(gameDir);
                const bool insideTheGame = !relative.is_absolute() && !inside.empty() && *inside.begin() != "..";
                std::error_code textureError;
                if (insideTheGame && std::filesystem::is_regular_file(from, textureError) &&
                    !std::filesystem::exists(to, textureError))
                {
                    std::filesystem::create_directories(to.parent_path(), textureError);
                    std::filesystem::copy_file(from, to, textureError);
                }
            }
            return target.lexically_relative(projectDir_).generic_string();
        }

        // A model is copied with the folder it sits in: its materials and its textures are named by the model
        // (crate.mtl, crate.png) and have to travel with it, or it draws untextured. Files already in the game are
        // left as they are, so importing the same model twice changes nothing.
        const bool isModel =
            std::find(kMeshExtensions.begin(), kMeshExtensions.end(), sourceExtension) != kMeshExtensions.end();
        if (isModel)
        {
            std::error_code walkError;
            const std::filesystem::path sourceDir = source.parent_path();
            for (auto it = std::filesystem::recursive_directory_iterator(sourceDir, walkError);
                 !walkError && it != std::filesystem::recursive_directory_iterator(); it.increment(walkError))
            {
                if (!it->is_regular_file(walkError))
                {
                    continue;
                }
                const auto sibling = folder / it->path().lexically_relative(sourceDir);
                if (std::filesystem::exists(sibling, walkError))
                {
                    continue;
                }
                std::filesystem::create_directories(sibling.parent_path());
                std::filesystem::copy_file(it->path(), sibling, walkError);
            }
        }

        return target.lexically_relative(projectDir_).generic_string();
    }

    const char* ToString(GameTemplate gameTemplate)
    {
        switch (gameTemplate)
        {
        case GameTemplate::Empty: return "empty";
        case GameTemplate::Rendering: return "rendering";
        case GameTemplate::Resources: return "resources";
        case GameTemplate::Ecs: return "ecs";
        case GameTemplate::Audio: return "audio";
        case GameTemplate::Input: return "input";
        case GameTemplate::Prefabs: return "prefabs";
        case GameTemplate::Rpg: return "rpg";
        case GameTemplate::Fps: return "fps";
        case GameTemplate::Racing: return "racing";
        }
        return "empty";
    }

    const char* Describe(GameTemplate gameTemplate)
    {
        switch (gameTemplate)
        {
        case GameTemplate::Empty: return "Nothing in it yet. Build everything yourself.";
        case GameTemplate::Rendering: return "100 wooden crates, lit by a sun and 4 lamps.";
        case GameTemplate::Resources:
            return "10 crates that share one model and picture, and what you see when a file is missing.";
        case GameTemplate::Ecs: return "Copy one object 1,000 times and see how fast the game still runs.";
        case GameTemplate::Audio: return "Music, and a player who hears a sound get louder as they walk up to it.";
        case GameTemplate::Input: return "A player the camera follows, and 4 places to walk to.";
        case GameTemplate::Prefabs: return "A player and 5 enemies made from one prefab (one has less health).";
        case GameTemplate::Rpg:
            return "An adventure: find the villager's 3 lost gems, and fight the slimes with Space.";
        case GameTemplate::Fps:
            return "A tactical shooter: rounds against 4 enemies in a yard with two sites, rifle and pistol.";
        case GameTemplate::Racing: return "A race track: drive through the gates in order, 3 laps. A and D steer.";
        }
        return "";
    }
}
