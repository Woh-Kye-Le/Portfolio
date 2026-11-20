/* !
@file    	    GameProject.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		26/09/2026
@brief
    Declares GameProject (a game folder: settings, scenes, input actions and
    assets), the game templates, and helpers to find, create and import into
    game projects.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/game/Shape.hpp"
#include "engine/scene/SceneFile.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Game
{
    // File types the engine loads (lower case), for listing and importing a game's assets
    inline constexpr std::array<std::string_view, 5> kTextureExtensions{".png", ".jpg", ".jpeg", ".bmp", ".tga"};
    inline constexpr std::array<std::string_view, 5> kMeshExtensions{".amodel", ".obj", ".fbx", ".glb", ".gltf"};
    inline constexpr std::array<std::string_view, 4> kAudioExtensions{".wav", ".ogg", ".mp3", ".flac"};

    // The folders of a game's assets/, by type (there is no materials folder: a .amodel holds its material slots):
    //   art/models/       one folder per model, named after it: <name>/<name>.amodel and the original files it was
    //                     made from (.obj + .mtl, .fbx, .glb, ...)
    //   art/textures/     images for 3D objects: <name>/ holds the images of model <name>; an image imported on its
    //                     own (a wood texture for the built-in cube) sits directly in art/textures/
    //   art/sprites/      2D images
    //   art/spritesheets/ sheets of frames used for 2D animation
    //   audio/            sounds and music
    //   prefabs/          prefabs
    // Imports always land in these places by file type, wherever they were dropped.
    inline constexpr const char* kArtFolder = "assets/art";
    inline constexpr const char* kModelsFolder = "assets/art/models";
    inline constexpr const char* kTexturesFolder = "assets/art/textures";
    inline constexpr const char* kSpritesFolder = "assets/art/sprites";
    inline constexpr const char* kSpriteSheetsFolder = "assets/art/spritesheets";
    inline constexpr const char* kAudioFolder = "assets/audio";
    inline constexpr const char* kPrefabsFolder = "assets/prefabs";
    inline constexpr const char* kMaterialsFolder = "assets/art/materials"; // .amat: how objects look
    inline constexpr const char* kShadersFolder = "assets/shaders";         // a game's own shaders: <name>.vert + .frag
    inline constexpr std::array<std::string_view, 1> kMaterialExtensions{".amat"};
    inline constexpr std::array<std::string_view, 1> kShaderExtensions{".vert"}; // the .frag goes with it

    // What an image is used for, which decides the folder it is imported into
    enum class ImageUse : std::uint8_t
    {
        Texture3D,  // on a 3D object (a mesh or the built-in cube): art/textures/
        Sprite,     // a plain 2D image: sprites/
        SpriteSheet // the frames of a 2D animation: spritesheets/
    };

    // configs/game.cfg of a game project (read and written with Serializer::CFGSerializer)
    struct GameSettings
    {
        std::string name;
        int windowWidth = 1280;
        int windowHeight = 720;
        std::string startScene = "data/scenes/main.json";

        // The pull on every dynamic rigid body in Play mode, in units per second squared. The default is Earth's
        // gravity, pulling down the Y axis (game.cfg: gravity_x, gravity_y, gravity_z).
        float gravityX = 0.0f;
        float gravityY = -9.81f;
        float gravityZ = 0.0f;
    };

    // Starting content for a newly created game: an empty scene, or one of the sample game's scenes (with the asset
    // files it uses), read from GameProject::TemplatesDir()
    enum class GameTemplate : std::uint8_t
    {
        Empty,
        // The milestone demos, one per engine feature: kept for showing the requirements (and for --template),
        // but not offered for new games
        Rendering, // 100 crates, 1 directional + 4 point lights, a Main camera
        Resources, // 10 crates and a few more objects, all built-in shapes
        Ecs,       // a template to spawn 1,000 copies of, and objects to add and remove components on
        Audio,     // a looping 3D sound, background music, and a player the camera follows
        Input,     // a player the camera follows, and waypoints marking the directions
        Prefabs,   // a player, and 5 copies of the Enemy prefab made from it (one with less health)
        // The games to start from, one per genre, each played with blocks
        Rpg,   // a village quest: find 3 gems for the villager, Space to fight the slimes
        Fps,   // an arena: clear 4 drones that chase you, Space fires
        Racing // a track of gates driven through in order, 3 laps
    };
    // What New Game offers
    inline constexpr std::array<GameTemplate, 4> kGameTemplates{GameTemplate::Empty, GameTemplate::Rpg,
                                                                GameTemplate::Fps, GameTemplate::Racing};
    // Every template, the demos too (AtelierEditor --new-game --template takes any of them)
    inline constexpr std::array<GameTemplate, 10> kAllGameTemplates{
        GameTemplate::Empty,     GameTemplate::Rpg, GameTemplate::Fps,   GameTemplate::Racing, GameTemplate::Rendering,
        GameTemplate::Resources, GameTemplate::Ecs, GameTemplate::Audio, GameTemplate::Input,  GameTemplate::Prefabs};

    // A game is a folder of data, so new games need no C++ code or build changes:
    //   <name>/configs/game.cfg       settings (marks the folder as a game project)
    //   <name>/configs/input_actions.json   named actions and their keys
    //   <name>/data/scenes/*.json     scenes
    //   <name>/assets/, <name>/scripts/
    class GameProject
    {
    public:
        // Reads configs/game.cfg; missing values keep their defaults and the name defaults to the folder name
        explicit GameProject(std::filesystem::path projectDir);

        const std::filesystem::path& ProjectDir() const;
        const GameSettings& Settings() const;

        // Writes settings to configs/game.cfg and uses them from now on. Throws std::runtime_error if the file cannot
        // be written.
        void SaveSettings(const GameSettings& settings);

        // Scenes are files in the Scene::SceneFile format (see SceneFile.hpp), at paths relative to the project folder,
        // e.g. "data/scenes/main.json"
        Scene::SceneDescription LoadStartScene() const;
        // Lenient: a missing or corrupted file gives an empty scene and bad entries are skipped, so a game always
        // starts. Use LoadSceneChecked to find out why.
        Scene::SceneDescription LoadScene(const std::filesystem::path& relativePath) const;
        // Same as LoadScene, but reports problems. scene is only changed when the report is ok.
        Scene::SceneLoadReport LoadSceneChecked(const std::filesystem::path& relativePath,
                                                Scene::SceneDescription& scene) const;

        // Writes a scene file, creating its folder if needed. Throws std::runtime_error if the file cannot be written.
        void SaveScene(const std::filesystem::path& relativePath, const Scene::SceneDescription& scene) const;
        void SaveStartScene(const Scene::SceneDescription& scene) const;

        // configs/input_actions.json: the game's named actions and their keys (see Input::InputActions)
        std::filesystem::path InputActionsPath() const;

        // Files under relativeFolder (e.g. "assets/models", searched recursively) whose extension is one of
        // extensions (any case), as paths relative to the project folder with '/' separators, sorted.
        // Empty if the folder does not exist.
        std::vector<std::string> FindAssets(const std::filesystem::path& relativeFolder,
                                            std::span<const std::string_view> extensions) const;
        // The models a scene can use: every .amodel, .obj, .fbx, .glb and .gltf under assets/, except the originals kept for
        // importing again (an .obj, .fbx or .glb in a folder that has a .amodel is the source of it, not another model).
        std::vector<std::string> FindModels() const;
        // Copies a file from anywhere into relativeFolder (created if needed) and returns its path relative to the
        // project folder, ready to put in a scene. A file already there with the same name and contents is reused;
        // a different one is kept and the copy gets a numbered name (crate_2.png).
        // A .obj, .fbx, .glb or .gltf comes with the folder it sits in (its materials and textures); a .amodel comes with just the
        // images it names, which are beside it. Models in other formats are converted by ImportModel
        // (ModelImporter.hpp) instead.
        // Throws std::runtime_error if source is not a file or cannot be copied.
        std::string ImportAsset(const std::filesystem::path& source, const std::filesystem::path& relativeFolder) const;
        // Copies an image into the folder its use asks for: art/textures/, art/sprites/ or art/spritesheets/ (a
        // different image of the same name gets a numbered name). Returns its path relative to the project folder.
        // Throws std::runtime_error as ImportAsset does.
        std::string ImportImage(const std::filesystem::path& source, ImageUse use) const;
        // Makes assets/art/materials/<name>.amat, plain white with the standard shader (a name already taken gets a number,
        // and characters a file name cannot have are left out). Returns its path relative to the project folder.
        // Throws std::runtime_error when it cannot be written.
        std::string CreateMaterial(const std::string& name) const;
        // Makes assets/shaders/<name>.vert and <name>.frag: a copy of the engine's standard lit shader with one value
        // of its own to start from (u_Brightness, which a material sets), named as CreateMaterial names. Returns the
        // .vert's path. Throws std::runtime_error when the engine's shaders cannot be found or it cannot be written.
        std::string CreateShader(const std::string& name) const;

        static bool IsGameProject(const std::filesystem::path& dir);
        // Game projects directly inside projectsRoot, sorted by folder name
        static std::vector<std::filesystem::path> FindAll(const std::filesystem::path& projectsRoot);

        // Where game projects live: projects/ in the folder containing the running exe
        static std::filesystem::path ProjectsRoot();
        // ATELIER_GAME_PROJECT if it points at a folder, else the first game in ProjectsRoot(), else an empty path
        static std::filesystem::path FindDefaultGameProjectDir();

        // 1-64 characters: letters, digits, '_' or '-', starting with a letter or digit
        static bool IsValidName(std::string_view name);

        // Where the demo templates are: templates/demos next to the running exe (the build copies the sample game
        // there), else the sample game in the source tree. Each template is data/scenes/<template name>.json in it.
        static std::filesystem::path TemplatesDir();

        // Creates projectsRoot/<name> with the standard folders, configs/game.cfg, configs/input_actions.json (the
        // default bindings) and data/scenes/main.json: empty, or the template's scene with the asset files it uses
        // copied from templatesDir (TemplatesDir() when empty). Files the template only names (such as the Resources
        // demo's missing texture) are not copied, so they stay missing.
        // Throws std::runtime_error if the name is invalid, the folder already exists or the template cannot be read;
        // then no folder is created.
        static GameProject Create(const std::filesystem::path& projectsRoot, const std::string& name,
                                  GameTemplate gameTemplate, const std::filesystem::path& templatesDir = {});

    private:
        std::filesystem::path projectDir_;
        GameSettings settings_;
    };

    // Copies the engine's built-in shaders (the editor's own, current ones) into <gameDir>/assets/shaders/Builtin/,
    // replacing older copies: a new game gets them, and Build Game refreshes the built game's
    void CopyBuiltinShaders(const std::filesystem::path& gameDir);

    // "empty", "rendering", "resources", "ecs", "audio", "input" or "prefabs"
    const char* ToString(GameTemplate gameTemplate);
    // What the template starts with, one line in a learner's words for the New Game window
    const char* Describe(GameTemplate gameTemplate);
}
