/* !
@file               AssetConverter.hpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Declares the model converter of the asset tool: it reads a model file in any
    format assimp understands (OBJ, FBX, glTF, ...), cleans it up and writes it
    as the engine's own .amodel file together with its textures. assimp is used
    here and only here - the engine, the editor and the games never link it.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Standard Library Headers
#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Atelier::Tool
{
    /*!***********************************************************************
    @brief          What to convert and where the results go.
    *************************************************************************/
    struct ConvertOptions
    {
        std::filesystem::path input;       // model file to read, or a folder or .zip with one model in it
        std::filesystem::path output;      // .amodel file to write, or a folder to write <model name>.amodel into
        std::filesystem::path texturesDir; // where the textures are copied; empty = beside the output
        float scale = 1.0f;                // multiplies the whole model (FBX files made in centimetres, say)
        float fitSize = 0.0f;              // > 0: scales the model so its longest side is this long (before scale)
        bool center = false;               // moves the model so the middle of its bounds is at the origin
        // output is the game's models folder, and the model gets a folder of its own in it: <output>/<name>/ holds
        // <name>.amodel and the original files (the .obj with its .mtl, the .fbx, the .glb, ...) so it can be
        // imported again. Its images go to <texturesDir>/<name>/ (beside the .amodel when texturesDir is empty),
        // named in the .amodel by relative path, together with, for a pack, the pack's other images, which can be put
        // on the object as its Texture or wired up with a .mtl later. By format:
        //   .obj   models: .obj + .mtl         textures: its images (+ the pack's other images)
        //   .fbx   models: .fbx                textures: its images (+ the pack's other images; also .dae, .3ds, ...)
        //   .gltf  models: .gltf + .bin        textures: the images it names
        //   .glb   models: .glb                textures: the images written out of it
        bool modelFolder = false;
    };

    /*!***********************************************************************
    @brief          What a conversion did.
    *************************************************************************/
    struct ConvertResult
    {
        bool ok = false;
        std::string error;                 // why it failed, when it did
        std::vector<std::string> warnings; // things that were left out or could not be found
        std::size_t meshes = 0;
        std::size_t materials = 0;
        std::size_t nodes = 0;
        std::vector<std::filesystem::path> textures; // the texture files now next to the model, as written
        std::filesystem::path output;                // the .amodel file written
        std::vector<std::filesystem::path> kept;     // modelFolder: the original files and loose images stored with it
        std::string source; // the model file that was converted (inside the folder or zip, when one was given)
    };

    // Model formats the tool reads, best first: when a folder or zip holds the same model in several formats, the
    // first one here is converted (glTF and FBX carry their materials; OBJ needs its .mtl; PLY and STL have none)
    inline constexpr std::array<std::string_view, 8> kModelExtensions{".glb", ".gltf", ".fbx", ".dae",
                                                                      ".obj", ".3ds",  ".ply", ".stl"};

    /*!***********************************************************************
    @brief          Converts one model file into a .amodel file. The model is triangulated, gets normals where the
                    file has none, shares vertices, and keeps its node hierarchy, material slots and (relative)
                    texture paths. Textures are found the way the file names them (relative path, full path, or
                    the bare file name beside the model or in a textures folder), copied into the textures folder
                    and referred to relative to the output model. Textures embedded in the file are written out
                    as image files. The skeleton (up to four bones a vertex) and the animations are kept; lights and cameras are not.
                    The input can also be a folder or a .zip (a downloaded model pack): the model inside is found
                    (one model, possibly in several formats; see kModelExtensions) and converted with the files
                    around it. Files the model does not use (a .max scene, previews, readmes) are left out.
                    With modelFolder the model goes into <output>/<name>/ (see ConvertOptions): importing the same
                    model again updates that folder, and a different model with the same name gets <name>_2.
    @param          options       Input, output and settings.
    @return         The outcome; ok is false and error says why when nothing was written.
    *************************************************************************/
    ConvertResult ConvertModelFile(const ConvertOptions& options);
}
