/* !
@file               ModelLoader.cpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Implements the model file loader and the placeholder model. Meshes, material
    slots and nodes all end up in the same ModelData regardless of format.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/assets/loaders/ModelLoader.hpp"

// Engine Core Headers
#include "engine/core/FileSystem.hpp"

// Engine Assets Headers
#include "engine/assets/loaders/AtelierModel.hpp"
#include "engine/assets/loaders/GltfLoader.hpp"
#include "engine/assets/loaders/MeshLoader.hpp"
#include "engine/assets/loaders/ObjLoader.hpp"

// Third-Party Headers
#include <glm/glm.hpp>

// Standard Library Headers
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Engine::Assets
{
    namespace
    {
        /*!***********************************************************************
        @brief          Wraps one MeshData as a whole model: one mesh, one
                        material slot, no hierarchy. Used by the formats that
                        still only produce a single mesh.
        @param          mesh          The mesh to wrap.
        @param          name          Model name (usually the file stem).
        @return         ModelData carrying that mesh under a default material.
        *************************************************************************/
        ModelData WrapSingleMesh(MeshData mesh, const std::string& name)
        {
            ModelData model;
            model.name = name;

            MaterialData material;
            material.name = name.empty() ? "default" : name + "_material";
            model.materials.push_back(std::move(material));

            // No submeshes: the whole index buffer is one draw under material 0.
            model.meshes.push_back(std::move(mesh));
            return model;
        }
    }

    /*!***********************************************************************
    @brief          Picks a model parser by extension.
    @param          path          Model file to read.
    @return         Populated ModelData, or nullopt when unsupported or unreadable.
    *************************************************************************/
    std::optional<ModelData> LoadModelFromFile(const std::filesystem::path& path)
    {
        const std::string ext = Core::Extension(path);
        const std::string name = path.stem().string();

        if (ext == kAtelierModelExtension)
        {
            // The engine's own format: a ModelData saved by the asset tool after it converted the original file
            const auto bytes = Core::ReadBytes(path);
            if (!bytes)
            {
                return std::nullopt;
            }
            auto model = ParseAtelierModel(*bytes);
            if (!model)
            {
                return std::nullopt;
            }
            model->name = name;
            return model;
        }

        if (ext == ".obj")
        {
            // OBJ carries its materials in the mtllib files it names, so it fills
            // the whole ModelData itself: one mesh, one submesh run per usemtl.
            const auto text = Core::ReadText(path);
            if (!text)
            {
                return std::nullopt;
            }
            auto model = ParseObjModel(*text, path.parent_path());
            if (!model)
            {
                return std::nullopt;
            }
            model->name = name;
            return model;
        }

        if (ext == ".fbx")
        {
            // ponytail: FBX still goes through the single-mesh mesh loader, so it
            // loads as one mesh with one material. Reading every mesh (and its node
            // transform and material) is the FBX loader's own next step.
            auto mesh = LoadMeshFromFile(path);
            if (!mesh)
            {
                return std::nullopt;
            }
            return WrapSingleMesh(std::move(*mesh), name);
        }

        if (ext == ".gltf" || ext == ".glb")
        {
            return LoadGltfModel(path);
        }
        return std::nullopt;
    }

    /*!***********************************************************************
    @brief          Builds the single-mesh, single-material stand-in model.
    @return         Unit cube wrapped in a ModelData.
    *************************************************************************/
    ModelData PlaceholderModel()
    {
        return WrapSingleMesh(PlaceholderMesh(), "placeholder_model");
    }

    /*!***********************************************************************
    @brief          Merges every mesh of a model into one MeshData, each placed by the world transform of the
                    node that carries it.
    @param          model         Model to flatten.
    @return         Merged MeshData, or nullopt when the model holds no triangles.
    *************************************************************************/
    std::optional<MeshData> FlattenModelToMesh(const ModelData& model)
    {
        MeshData flat;
        const bool anyNormals = std::any_of(model.meshes.begin(), model.meshes.end(),
                                            [](const MeshData& mesh) { return !mesh.normals.empty(); });

        const auto append = [&flat, anyNormals](const MeshData& mesh, const glm::mat4& world)
        {
            const std::size_t vertexCount = mesh.positions.size() / 3;
            const auto first = static_cast<std::uint32_t>(flat.positions.size() / 3);

            // The inverse-transpose keeps normals perpendicular under the transform; identity if it cannot be inverted
            glm::mat3 normalMatrix(1.0f);
            const float determinant = glm::determinant(glm::mat3(world));
            if (std::isfinite(determinant) && std::abs(determinant) > 1e-12f)
            {
                normalMatrix = glm::transpose(glm::inverse(glm::mat3(world)));
            }

            for (std::size_t i = 0; i < vertexCount; ++i)
            {
                const glm::vec3 position(world * glm::vec4(mesh.positions[i * 3], mesh.positions[i * 3 + 1],
                                                           mesh.positions[i * 3 + 2], 1.0f));
                flat.positions.insert(flat.positions.end(), {position.x, position.y, position.z});

                const bool hasUv = i * 2 + 1 < mesh.uvs.size();
                flat.uvs.insert(flat.uvs.end(), {hasUv ? mesh.uvs[i * 2] : 0.0f, hasUv ? mesh.uvs[i * 2 + 1] : 0.0f});

                if (anyNormals)
                {
                    glm::vec3 normal(0.0f); // a mesh without normals leaves them to the uploader
                    if (mesh.normals.size() == mesh.positions.size())
                    {
                        normal = normalMatrix *
                                 glm::vec3(mesh.normals[i * 3], mesh.normals[i * 3 + 1], mesh.normals[i * 3 + 2]);
                        const float length = glm::length(normal);
                        normal = length > 1e-8f ? normal / length : glm::vec3(0.0f);
                    }
                    flat.normals.insert(flat.normals.end(), {normal.x, normal.y, normal.z});
                }
            }
            for (const std::uint32_t index : mesh.indices)
            {
                flat.indices.push_back(first + index);
            }
        };

        if (model.nodes.empty())
        {
            for (const MeshData& mesh : model.meshes)
            {
                append(mesh, glm::mat4(1.0f));
            }
        }
        else
        {
            std::vector<std::uint32_t> roots = model.roots;
            if (roots.empty())
            {
                std::vector<bool> isChild(model.nodes.size(), false);
                for (const ModelNode& node : model.nodes)
                {
                    for (const std::uint32_t child : node.children)
                    {
                        if (child < isChild.size())
                        {
                            isChild[child] = true;
                        }
                    }
                }
                for (std::uint32_t i = 0; i < model.nodes.size(); ++i)
                {
                    if (!isChild[i])
                    {
                        roots.push_back(i);
                    }
                }
            }

            constexpr int kMaxDepth = 256; // a malformed hierarchy with a cycle must not recurse forever
            const std::function<void(std::uint32_t, const glm::mat4&, int)> visit =
                [&](std::uint32_t index, const glm::mat4& parent, int depth)
            {
                if (index >= model.nodes.size() || depth > kMaxDepth)
                {
                    return;
                }
                const ModelNode& node = model.nodes[index];
                const glm::mat4 world = parent * node.localTransform;
                for (const std::uint32_t meshIndex : node.meshIndices)
                {
                    if (meshIndex < model.meshes.size())
                    {
                        append(model.meshes[meshIndex], world);
                    }
                }
                for (const std::uint32_t child : node.children)
                {
                    visit(child, world, depth + 1);
                }
            };
            for (const std::uint32_t root : roots)
            {
                visit(root, glm::mat4(1.0f), 0);
            }
        }

        if (flat.indices.empty())
        {
            return std::nullopt;
        }
        flat.name = model.name;
        return flat;
    }
}
