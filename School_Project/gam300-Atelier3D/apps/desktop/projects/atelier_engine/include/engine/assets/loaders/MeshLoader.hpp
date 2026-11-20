/* !
@file               MeshLoader.hpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Declares the mesh file loader and the placeholder mesh. Everything here
    stays CPU-side; Graphics::Mesh::FromAssetData turns a MeshData into GPU
    geometry.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Assets Headers
#include "engine/assets/data/MeshData.hpp"

// Standard Library Headers
#include <filesystem>
#include <optional>

namespace Engine::Assets
{
    /*!***********************************************************************
    @brief          Picks a parser by extension: .obj through ParseObj, .fbx
                    through OpenFBX, .amodel through the .amodel reader with all
                    its parts merged into one mesh.
    @param          path          Mesh file to read.
    @return         Populated MeshData, or nullopt for an unknown extension and
                    for a missing or malformed file.
    *************************************************************************/
    std::optional<MeshData> LoadMeshFromFile(const std::filesystem::path& path);

    /*!***********************************************************************
    @brief          Builds the unit cube (six faces, two triangles each) used
                    whenever a mesh file is missing.
    @return         Textured unit cube MeshData.
    *************************************************************************/
    MeshData PlaceholderMesh();
}
