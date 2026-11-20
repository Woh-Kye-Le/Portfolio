/* !
@file               ModelImporter.hpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Declares ImportModel, which brings a model from anywhere on disk into a
    game by running the asset tool: the model gets a folder of its own under
    the game's art/models folder, holding the .amodel and the original files it
    was made from, and its images get art/textures/<name>/. The engine itself never reads those
    foreign formats through assimp; it only starts the tool and reads what the
    tool wrote.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Game Headers
#include "engine/game/GameProject.hpp"

// Standard Library Headers
#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Game
{
    // Model formats the asset tool converts (lower case); .amodel is the engine's own and is only copied
    inline constexpr std::array<std::string_view, 8> kImportableModelExtensions{".obj", ".fbx", ".gltf", ".glb",
                                                                                ".dae", ".3ds", ".ply",  ".stl"};

    // How long the longest side of an imported model is, in scene units (the built-in cube is 1)
    inline constexpr float kImportedModelSize = 2.0f;

    /*!***********************************************************************
    @brief          What an import produced.
    *************************************************************************/
    struct ModelImportResult
    {
        std::string path; // the model inside the game, relative to the project folder, ready for a scene
        std::vector<std::string> warnings; // what the tool could not find or left out (a missing texture, say)
    };

    /*!***********************************************************************
    @brief          Whether a file is a model format the asset tool converts (any case of the extension).
    @param          file          Path or file name.
    *************************************************************************/
    bool IsImportableModel(const std::filesystem::path& file);

    /*!***********************************************************************
    @brief          Whether a file is a model ImportModel takes: a format the asset tool converts, or the engine's own
                    .amodel (any case of the extension).
    @param          file          Path or file name.
    *************************************************************************/
    bool IsModelFile(const std::filesystem::path& file);

    /*!***********************************************************************
    @brief          Whether a path is a model pack to import in one go: a .zip file (its contents are checked by the
                    asset tool), or a folder that holds one model (files of one name, perhaps in several formats,
                    in it or a few folders down). A folder of several different models is not one.
    @param          path          File or folder.
    *************************************************************************/
    bool IsModelPackage(const std::filesystem::path& path);

    /*!***********************************************************************
    @brief          Where the asset tool is: AtelierAssetTool next to the running executable.
    @return         Its path, or an empty path when it is not there (it was not built, or not copied).
    *************************************************************************/
    std::filesystem::path AssetToolPath();

    /*!***********************************************************************
    @brief          Converts a model file into the game, into a folder named after the model:
                    <modelsFolder>/<name>/<name>.amodel with the files it was made from (the .obj with its .mtl, the
                    .fbx, the .glb, ...) so it can be imported again, and art/textures/<name>/ with the images it
                    uses (named in the .amodel by relative path). The model is centred on the origin and scaled so its longest side is
                    kImportedModelSize long, whatever size and place the file had. Importing the same model
                    again updates its folder; a different model with the same name gets <name>_2.
                    When the tool is not available, .obj and .fbx files are copied as they are instead into the same
                    folder (the engine can still draw them) and a warning says so; other formats cannot be imported
                    then. A .amodel is copied into its own folder with the images it names.
                    The source can also be a model pack (IsModelPackage): a folder or .zip with the model, its .mtl
                    and its textures. The tool picks the model inside and names the folders after it. The pack's
                    other images are kept in its textures folder too, even when no material names them (an OBJ that came
                    without its .mtl, say), so they can be used as the object's Texture or named by a .mtl later.
                    Files nothing can use (a .max scene, preview images) are not brought in.
    @param          project       The game to import into.
    @param          source        The model file, or a folder or .zip holding one, anywhere on disk.
    @param          modelsFolder  Where the model's folder goes, relative to the project folder (created if needed).
    @param          tool          The asset tool executable (AssetToolPath() by default).
    @return         The model's path in the game and any warnings.
    @throws         std::runtime_error with the tool's reason when the model cannot be converted, or when the file is
                    not a model format that can be imported.
    *************************************************************************/
    ModelImportResult ImportModel(const GameProject& project, const std::filesystem::path& source,
                                  const std::filesystem::path& modelsFolder = kModelsFolder,
                                  const std::filesystem::path& tool = AssetToolPath());
}
