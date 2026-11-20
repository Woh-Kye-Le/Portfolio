/* !
@file               MeshLoader.cpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Implements the mesh file loader (.obj through the OBJ parser, .fbx through
    OpenFBX, .amodel through the model reader) and the unit-cube placeholder.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/assets/loaders/MeshLoader.hpp"

// Engine Core Headers
#include "engine/core/FileSystem.hpp"

// Engine Assets Headers
#include "engine/assets/loaders/AtelierModel.hpp"
#include "engine/assets/loaders/GltfLoader.hpp"
#include "engine/assets/loaders/ModelLoader.hpp"
#include "engine/assets/loaders/ObjLoader.hpp"

// Third-Party Headers
#include <OpenFBX/ofbx.h>

// Standard Library Headers
#include <iterator>
#include <vector>

namespace Engine::Assets
{
    namespace
    {
        // ponytail: first mesh only, node transforms ignored; walk every mesh and apply getGlobalTransform()
        // once multi-part models matter
        std::optional<MeshData> LoadFbx(const std::vector<std::uint8_t>& bytes)
        {
            const auto flags = ofbx::LoadFlags::IGNORE_ANIMATIONS | ofbx::LoadFlags::IGNORE_BLEND_SHAPES |
                               ofbx::LoadFlags::IGNORE_CAMERAS | ofbx::LoadFlags::IGNORE_LIGHTS |
                               ofbx::LoadFlags::IGNORE_VIDEOS;
            ofbx::IScene* scene = ofbx::load(bytes.data(), bytes.size(), static_cast<ofbx::u16>(flags));
            if (!scene)
            {
                return std::nullopt;
            }

            MeshData mesh;
            if (scene->getMeshCount() > 0)
            {
                const ofbx::GeometryData& geometry = scene->getMesh(0)->getGeometryData();
                const ofbx::Vec3Attributes positions = geometry.getPositions();
                const ofbx::Vec2Attributes uvs = geometry.getUVs();

                for (int p = 0; p < geometry.getPartitionCount(); ++p)
                {
                    const ofbx::GeometryPartition partition = geometry.getPartition(p);
                    std::vector<int> triangle(static_cast<std::size_t>(partition.max_polygon_triangles) * 3);
                    for (int i = 0; i < partition.polygon_count; ++i)
                    {
                        const auto count = ofbx::triangulate(geometry, partition.polygons[i], triangle.data());
                        for (std::uint32_t k = 0; k < count; ++k)
                        {
                            const int corner = triangle[k];
                            const ofbx::Vec3 v = positions.get(corner);
                            mesh.positions.insert(
                                mesh.positions.end(),
                                {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)});
                            const ofbx::Vec2 uv = uvs.values ? uvs.get(corner) : ofbx::Vec2{0, 0};
                            mesh.uvs.insert(mesh.uvs.end(), {static_cast<float>(uv.x), static_cast<float>(uv.y)});
                            mesh.indices.push_back(static_cast<std::uint32_t>(mesh.indices.size()));
                        }
                    }
                }
            }
            scene->destroy();

            if (mesh.indices.empty())
            {
                return std::nullopt;
            }
            return mesh;
        }
    }

    /*!***********************************************************************
    @brief          Picks a parser by extension: .obj through ParseObj, .fbx
                    through OpenFBX, .amodel, .gltf and .glb through their model
                    readers with all their parts merged into one mesh.
    @param          path          Mesh file to read.
    @return         Populated MeshData, or nullopt when unsupported or unreadable.
    *************************************************************************/
    std::optional<MeshData> LoadMeshFromFile(const std::filesystem::path& path)
    {
        const auto ext = Core::Extension(path);
        if (ext == kAtelierModelExtension)
        {
            // A whole model used as one mesh: every part merged, placed by its node's transform
            const auto bytes = Core::ReadBytes(path);
            const auto model = bytes ? ParseAtelierModel(*bytes) : std::nullopt;
            return model ? FlattenModelToMesh(*model) : std::nullopt;
        }
        if (ext == ".obj")
        {
            const auto text = Core::ReadText(path);
            return text ? ParseObj(*text) : std::nullopt;
        }
        if (ext == ".fbx")
        {
            const auto bytes = Core::ReadBytes(path);
            return bytes ? LoadFbx(*bytes) : std::nullopt;
        }
        if (ext == ".gltf" || ext == ".glb")
        {
            const auto model = LoadGltfModel(path);
            return model ? FlattenModelToMesh(*model) : std::nullopt;
        }
        return std::nullopt;
    }

    /*!***********************************************************************
    @brief          Builds the unit cube used whenever a mesh file is missing.
    @return         Textured unit cube MeshData.
    *************************************************************************/
    MeshData PlaceholderMesh()
    {
        // Six faces of four corners, each face mapped to the full texture
        static constexpr float kFaces[6][4][3] = {
            {{-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}},
            {{0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}},
            {{-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f}},
            {{0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}},
            {{-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}},
            {{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f}},
        };
        static constexpr float kUvs[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

        MeshData mesh;
        mesh.name = "placeholder_cube";
        for (const auto& face : kFaces)
        {
            const auto first = static_cast<std::uint32_t>(mesh.positions.size() / 3);
            for (int c = 0; c < 4; ++c)
            {
                mesh.positions.insert(mesh.positions.end(), std::begin(face[c]), std::end(face[c]));
                mesh.uvs.insert(mesh.uvs.end(), std::begin(kUvs[c]), std::end(kUvs[c]));
            }
            mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
        }
        return mesh;
    }
}
