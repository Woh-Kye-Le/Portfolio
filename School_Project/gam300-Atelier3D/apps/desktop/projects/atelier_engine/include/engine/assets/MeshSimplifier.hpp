/* !
@file               MeshSimplifier.hpp
@author             w.kyele@digipen.edu [100%]
@date               04/11/2026
@brief
    Makes simpler versions of a model for level of detail: the same meshes with
    fewer triangles, by collapsing the edges whose removal changes the shape
    least (quadric error metrics, Garland and Heckbert 1997). The engine does
    this by itself when it loads a model, so a game needs one file per model.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Assets Headers
#include "engine/assets/data/ModelData.hpp"

// Standard Library Headers
#include <array>
#include <cstddef>
#include <filesystem>

namespace Engine::Assets
{
    // A model with fewer triangles than this is cheap enough to draw at any distance: it gets no simpler versions
    inline constexpr std::size_t kLodMinimumTriangles = 1000;

    // The share of the full model's triangles LOD 1 and LOD 2 keep
    inline constexpr std::array<float, 2> kLodTriangleRatios{0.5f, 0.2f};

    /*!***********************************************************************
    @brief          How many triangles a mesh draws.
    *************************************************************************/
    std::size_t TriangleCount(const MeshData& mesh);

    /*!***********************************************************************
    @brief          How many triangles a model draws at a level of detail (0: its own meshes; 1, 2: model.lods).
    *************************************************************************/
    std::size_t TriangleCount(const ModelData& model, int level = 0);

    /*!***********************************************************************
    @brief          A copy of a mesh with about ratio of its triangles. Each step removes the edge that moves the
                    surface least, moving one end onto the other, so the vertices left keep their own positions,
                    texture coordinates, normals and skinning. Vertices at the same place (loaders often give each
                    triangle its own) are worked on together; a place where the texture coordinates differ (a texture
                    seam) is never moved, so the picture does not tear, and edges along the mesh's border keep their
                    place. A collapse that would turn a triangle over is skipped, so it can keep more than asked.
    @param          mesh          The mesh to simplify.
    @param          ratio         0 to 1: the share of its triangles to keep.
    @return         The simpler mesh: the same material runs (an emptied run is left out), indices from 0.
    *************************************************************************/
    MeshData SimplifyMesh(const MeshData& mesh, float ratio);

    /*!***********************************************************************
    @brief          Fills model.lods with its simpler versions (kLodTriangleRatios), one per level, each a full list of
                    meshes in the same order as model.meshes. A model under kLodMinimumTriangles gets none, and a level
                    stops being made when it would not save at least a tenth of the triangles of the one before.
    *************************************************************************/
    void GenerateLods(ModelData& model);

    /*!***********************************************************************
    @brief          GenerateLods, made once per model file: the simpler versions are kept in the system's temp folder
                    (atelier_lod_cache) and read back from there the next time the same file is loaded, so a big model
                    is only simplified again when its file changes (its size or time saved). A cache that cannot be
                    read or written only means the work is done again.
    @param          model         The model just read from source; gets its lods.
    @param          source        The file it was read from.
    *************************************************************************/
    void LoadOrGenerateLods(ModelData& model, const std::filesystem::path& source);
}
