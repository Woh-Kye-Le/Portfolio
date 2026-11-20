/* !
@file               ModelLoader.hpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Declares the model file loader - the entry point that turns a .gltf/.glb,
    .amodel, .obj or .fbx file into the engine's internal ModelData - plus the
    placeholder model used when a file cannot be read.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Assets Headers
#include "engine/assets/data/MeshData.hpp"
#include "engine/assets/data/ModelData.hpp"

// Standard Library Headers
#include <filesystem>
#include <optional>

namespace Engine::Assets
{
    /*!***********************************************************************
    @brief          Picks a model parser by extension and fills in a ModelData:
                    .amodel (the engine's own format, written by the asset tool)
                    through the .amodel reader, .obj through the OBJ+MTL loader,
                    .fbx through OpenFBX.
    @param          path          Model file to read.
    @return         Populated ModelData, or nullopt for an unknown extension and
                    for a missing or malformed file.
    *************************************************************************/
    std::optional<ModelData> LoadModelFromFile(const std::filesystem::path& path);

    /*!***********************************************************************
    @brief          Builds the single-mesh, single-material stand-in used
                    whenever a model file is missing, so the object still shows
                    up in the scene.
    @return         Textured unit cube wrapped in a ModelData.
    *************************************************************************/
    ModelData PlaceholderModel();

    /*!***********************************************************************
    @brief          Merges every mesh of a model into one MeshData, each placed by
                    the world transform of the node that carries it (meshes of a
                    model without nodes keep their own coordinates). Material runs
                    are dropped: the result is one plain mesh with one implicit run.
                    This is how a model file is used where only a mesh is wanted.
    @param          model         Model to flatten.
    @return         Merged MeshData, or nullopt when the model holds no triangles.
    *************************************************************************/
    std::optional<MeshData> FlattenModelToMesh(const ModelData& model);
}
