/* !
@file               MeshData.hpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Declares the CPU-side mesh description every loader writes into. A MeshData
    holds flat vertex attribute arrays plus the submeshes that split its index
    buffer into per-material runs.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Standard Library Headers
#include <cstdint>
#include <string>
#include <vector>

namespace Engine::Assets
{
    /*!***********************************************************************
    @brief          One contiguous run of indices inside a mesh that shares a single material.
    *************************************************************************/
    struct SubMesh
    {
        std::uint32_t baseIndex = 0;     // first index inside MeshData::indices
        std::uint32_t indexCount = 0;    // number of indices in this run
        std::int32_t baseVertex = 0;     // value added to each index before lookup
        std::uint32_t materialIndex = 0; // index into ModelData::materials
    };

    /*!***********************************************************************
    @brief          CPU-side mesh: flat vertex attributes, indices and the submeshes that split them.
    *************************************************************************/
    struct MeshData
    {
        std::vector<float> positions;       // xyz per vertex
        std::vector<float> uvs;             // uv per vertex; empty when the source has no UVs
        std::vector<float> normals;         // xyz per vertex; empty when normals have to be derived
        std::vector<std::uint32_t> indices; // triangles
        std::vector<SubMesh> subMeshes;     // material runs; empty means a single implicit submesh
        // Skinning: up to four bones per vertex, as indices into the joints of ModelData::skins[skin] with their
        // weights (which add up to 1). Both empty, and skin -1, for a mesh that is not bent by a skeleton.
        std::vector<std::uint16_t> joints; // 4 per vertex
        std::vector<float> weights;        // 4 per vertex
        std::int32_t skin = -1;            // index into ModelData::skins
        std::string name;
        std::uint32_t rendererCacheId = 0; // id of the uploaded GPU copy; 0 = not uploaded yet
    };
}
