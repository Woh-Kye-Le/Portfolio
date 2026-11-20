/* !
@file               GltfLoader.hpp
@author             c.kojat@digipen.edu [100%]
@date               02/11/2026
@brief
    Declares the glTF 2.0 reader (.gltf with its .bin buffers, and the
    single-file binary .glb): meshes, materials, nodes and the images they
    use, including the images packed inside a .glb.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Assets Headers
#include "engine/assets/data/ModelData.hpp"

// Standard Library Headers
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace Engine::Assets
{
    /*!***********************************************************************
    @brief          Reads a .gltf or .glb file into a ModelData. Each glTF mesh
                    becomes one MeshData with a SubMesh per triangle primitive;
                    nodes keep their transforms and the default scene's roots.
                    A material's image file is named relative to the model's
                    folder; an image packed in the file (a .glb's buffer, or a
                    data: URI) is named "<file>#<image index>", e.g. "car.glb#0",
                    which LoadTextureFromFile reads back out of the file.
                    glTF models face +Z and the engine's forward is -Z, so a
                    root node is added above the file's roots that turns the
                    model 180 degrees round Y.
    @param          path          The .gltf or .glb file.
    @return         Populated ModelData, or nullopt for a missing or malformed
                    file (or one with no triangles).
    *************************************************************************/
    std::optional<ModelData> LoadGltfModel(const std::filesystem::path& path);

    /*!***********************************************************************
    @brief          The file a texture path is read from: "car.glb#0" is read
                    from car.glb, any other path from itself.
    @param          path          A texture path, maybe naming a packed image.
    @return         The file on disk that holds the texture.
    *************************************************************************/
    std::filesystem::path GltfContainerOf(const std::filesystem::path& path);

    /*!***********************************************************************
    @brief          The encoded bytes (a .png or .jpg) of an image packed in a
                    glTF file, named as LoadGltfModel names it ("car.glb#0").
    @param          path          "<.gltf or .glb file>#<image index>".
    @return         The image file's bytes, or nullopt when the path does not
                    name a packed image or the file cannot be read.
    *************************************************************************/
    std::optional<std::vector<std::uint8_t>> ReadGltfImage(const std::filesystem::path& path);
}
