/* !
@file               ObjLoader.hpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Declares the Wavefront OBJ text parsers: a mesh-only one producing a
    CPU-side MeshData and a full one that also reads the MTL libraries and
    produces a multi-material ModelData.

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
#include <string>

namespace Engine::Assets
{
    /*!***********************************************************************
    @brief          Parses OBJ text (v, vt, vn, f) into a MeshData.
                    Face formats supported: v, v/vt, v/vt/vn and v//vn.
                    Polygons with more than three corners are fan-triangulated.
    @param          text          Entire OBJ file contents.
    @return         Populated MeshData, or nullopt when a face index is out of
                    range or no triangles were produced.
    *************************************************************************/
    std::optional<MeshData> ParseObj(const std::string& text);

    /*!***********************************************************************
    @brief          Parses OBJ text into a ModelData, reading the mtllib files
                    next to it so usemtl groups become real material slots. The
                    index buffer is split into one SubMesh run per material and
                    each run's materialIndex points at that slot. Faces before
                    any usemtl (or with none at all) belong to the material
                    named by their g/o group, or to a single "default" slot.
                    A material's texture paths are stored relative to the model's
                    own folder, the way ModelData documents them.
    @param          text          Entire OBJ file contents.
    @param          sourceDir     Folder the OBJ was read from, used to find its
                                  mtllib files. A library that is missing simply
                                  contributes no materials.
    @return         Populated ModelData, or nullopt when a face index is out of
                    range or no triangles were produced.
    *************************************************************************/
    std::optional<ModelData> ParseObjModel(const std::string& text, const std::filesystem::path& sourceDir);
}
