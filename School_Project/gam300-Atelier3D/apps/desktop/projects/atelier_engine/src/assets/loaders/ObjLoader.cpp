/* !
@file               ObjLoader.cpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Implements the Wavefront OBJ text parsers: reads v/vt/vn statements and
    fan-triangulates f statements into a flat vertex/index buffer, and - for the
    model parser - reads the mtllib files so usemtl groups become material slots
    and submesh runs.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/assets/loaders/ObjLoader.hpp"

// Engine Core Headers
#include "engine/core/FileSystem.hpp"

// Standard Library Headers
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace Engine::Assets
{
    namespace
    {
        // OBJ indices are 1-based; negative values count back from the end. Returns -1 when out of range.
        long ObjIndex(const std::string& text, std::size_t count)
        {
            if (text.empty())
            {
                return -1;
            }
            const long value = std::strtol(text.c_str(), nullptr, 10);
            const long index = value < 0 ? static_cast<long>(count) + value : value - 1;
            return index >= 0 && index < static_cast<long>(count) ? index : -1;
        }

        // The rest of a statement, trimmed: names and texture paths may hold spaces.
        std::string RestOfLine(std::istringstream& words)
        {
            std::string rest;
            std::getline(words, rest);
            const auto first = rest.find_first_not_of(" \t\r");
            const auto last = rest.find_last_not_of(" \t\r");
            return first == std::string::npos ? std::string{} : rest.substr(first, last - first + 1);
        }

        // Whether a whole token is a number (the arguments of a map option are numbers)
        bool IsNumber(const std::string& token)
        {
            char* end = nullptr;
            std::strtod(token.c_str(), &end);
            return !token.empty() && end == token.c_str() + token.size();
        }

        // The file name of a map_* statement: options (-s 1 1 1, -bm 0.5, -clamp on, ...) may precede it, and the
        // name itself may hold spaces, so it is whatever remains once the options are read.
        std::string MapFileName(std::istringstream& words)
        {
            for (;;)
            {
                words >> std::ws;
                if (words.peek() != '-')
                {
                    break;
                }
                std::string option;
                words >> option;
                // -o/-s/-t take up to three numbers, -mm two, the rest one value of their own
                const bool variable = option == "-o" || option == "-s" || option == "-t";
                const int count = variable ? 3 : (option == "-mm" ? 2 : 1);
                for (int i = 0; i < count; ++i)
                {
                    const auto before = words.tellg();
                    std::string argument;
                    if (!(words >> argument) || (variable && !IsNumber(argument)))
                    {
                        words.clear();
                        words.seekg(before);
                        break;
                    }
                }
            }
            return RestOfLine(words);
        }

        // The next statement of an OBJ or MTL text: a line ending in a backslash continues on the next one.
        bool NextStatement(std::istringstream& lines, std::string& statement)
        {
            if (!std::getline(lines, statement))
            {
                return false;
            }
            for (;;)
            {
                const auto last = statement.find_last_not_of(" \t\r");
                if (last == std::string::npos || statement[last] != '\\')
                {
                    break;
                }
                statement.erase(last);
                std::string next;
                if (!std::getline(lines, next))
                {
                    break;
                }
                statement += ' ' + next;
            }
            return true;
        }

        // One run of faces sharing a material, plus the index run they produced.
        struct FaceGroup
        {
            std::string material;
            std::uint32_t baseIndex = 0;
            std::uint32_t indexCount = 0;
        };

        // Everything the OBJ text itself holds, before materials become slots.
        struct ObjDocument
        {
            MeshData mesh;
            std::vector<FaceGroup> groups;
            std::vector<std::string> libraries; // mtllib entries, in file order
        };

        /*!***********************************************************************
        @brief          Reads one MTL text and appends the materials it declares.
                        Kd/d/Tr fill the base colour, Ke the emissive tint, and
                        map_Kd/map_Bump the texture paths.
        @param          text          Entire MTL file contents.
        @param          out           Material list to append to.
        *************************************************************************/
        void ParseMtl(const std::string& text, std::vector<MaterialData>& out)
        {
            std::istringstream lines(text);
            std::string line;
            MaterialData* current = nullptr;
            while (NextStatement(lines, line))
            {
                std::istringstream words(line);
                std::string tag;
                words >> tag;
                if (tag.empty() || tag[0] == '#')
                {
                    continue;
                }

                if (tag == "newmtl")
                {
                    out.push_back(MaterialData{});
                    current = &out.back();
                    current->name = RestOfLine(words);
                }
                else if (current == nullptr)
                {
                    continue; // a statement before the first newmtl belongs to no material
                }
                else if (tag == "Kd")
                {
                    glm::vec3 kd(1.0f);
                    words >> kd.r >> kd.g >> kd.b;
                    current->baseColorFactor = glm::vec4(kd, current->baseColorFactor.a);
                }
                else if (tag == "d")
                {
                    float alpha = 1.0f;
                    words >> alpha;
                    current->baseColorFactor.a = alpha;
                }
                else if (tag == "Tr") // the inverse of d, still used by older exporters
                {
                    float transparency = 0.0f;
                    words >> transparency;
                    current->baseColorFactor.a = 1.0f - transparency;
                }
                else if (tag == "Ke")
                {
                    glm::vec3 ke(0.0f);
                    words >> ke.r >> ke.g >> ke.b;
                    current->emissiveFactor = ke;
                    current->emissiveStrength = (ke.r > 0.0f || ke.g > 0.0f || ke.b > 0.0f) ? 1.0f : 0.0f;
                }
                else if (tag == "map_Kd")
                {
                    current->baseColorTexture = MapFileName(words);
                }
                else if (tag == "map_Bump" || tag == "bump" || tag == "norm")
                {
                    current->normalTexture = MapFileName(words);
                }
            }
        }

        /*!***********************************************************************
        @brief          Reads the OBJ statements: vertices, faces (split into one
                        run per material name) and the mtllib list.
        @param          text          Entire OBJ file contents.
        @return         Parsed document, or nullopt when a face index is out of
                        range or no triangles were produced.
        *************************************************************************/
        std::optional<ObjDocument> ParseObjDocument(const std::string& text)
        {
            std::vector<std::array<float, 3>> positions;
            std::vector<std::array<float, 2>> uvs;
            std::vector<std::array<float, 3>> normals;
            ObjDocument doc;

            std::string material; // usemtl name; empty until the file says otherwise
            std::string group;    // g/o name, used for files that never say usemtl

            std::istringstream lines(text);
            std::string line;
            bool anyNormal = false; // some corner named a vn

            while (NextStatement(lines, line))
            {
                std::istringstream words(line);
                std::string tag;
                words >> tag;

                if (tag == "v")
                {
                    std::array<float, 3> v{};
                    words >> v[0] >> v[1] >> v[2];
                    positions.push_back(v);
                }
                else if (tag == "vt")
                {
                    std::array<float, 2> uv{};
                    words >> uv[0] >> uv[1];
                    uvs.push_back(uv);
                }
                else if (tag == "vn")
                {
                    std::array<float, 3> n{};
                    words >> n[0] >> n[1] >> n[2];
                    normals.push_back(n);
                }
                else if (tag == "mtllib")
                {
                    std::string library;
                    while (words >> library)
                    {
                        doc.libraries.push_back(library);
                    }
                }
                else if (tag == "usemtl")
                {
                    material = RestOfLine(words);
                }
                else if (tag == "g" || tag == "o")
                {
                    group = RestOfLine(words);
                }
                else if (tag == "f")
                {
                    // Every face corner becomes its own vertex so a shared position can carry
                    // different UVs/normals; small models do not justify a dedupe pass.
                    const std::string key = material.empty() ? group : material;
                    const auto first = static_cast<std::uint32_t>(doc.mesh.positions.size() / 3);
                    const auto runStart = static_cast<std::uint32_t>(doc.mesh.indices.size());
                    std::uint32_t corners = 0;
                    std::string corner;
                    while (words >> corner)
                    {
                        const auto slash = corner.find('/');
                        const long v = ObjIndex(corner.substr(0, slash), positions.size());
                        if (v < 0)
                        {
                            return std::nullopt;
                        }
                        long vt = -1;
                        long vn = -1;
                        if (slash != std::string::npos)
                        {
                            const auto slash2 = corner.find('/', slash + 1);
                            vt = ObjIndex(corner.substr(slash + 1, slash2 - slash - 1), uvs.size());
                            if (slash2 != std::string::npos)
                            {
                                vn = ObjIndex(corner.substr(slash2 + 1), normals.size());
                            }
                        }

                        const auto& p = positions[static_cast<std::size_t>(v)];
                        doc.mesh.positions.insert(doc.mesh.positions.end(), p.begin(), p.end());
                        const std::array<float, 2> uv =
                            vt >= 0 ? uvs[static_cast<std::size_t>(vt)] : std::array{0.f, 0.f};
                        doc.mesh.uvs.insert(doc.mesh.uvs.end(), uv.begin(), uv.end());
                        // One normal per vertex keeps the arrays aligned; a corner without a vn gets a zero
                        // normal, which the uploader replaces by one derived from its faces.
                        std::array<float, 3> n{};
                        if (vn >= 0)
                        {
                            n = normals[static_cast<std::size_t>(vn)];
                        }
                        doc.mesh.normals.insert(doc.mesh.normals.end(), n.begin(), n.end());
                        anyNormal = anyNormal || vn >= 0;

                        if (++corners >= 3)
                        {
                            doc.mesh.indices.insert(doc.mesh.indices.end(),
                                                    {first, first + corners - 2, first + corners - 1});
                        }
                    }

                    // Faces of one material stay in one run; a new usemtl opens the next one.
                    const auto produced = static_cast<std::uint32_t>(doc.mesh.indices.size()) - runStart;
                    if (produced == 0)
                    {
                        continue;
                    }
                    if (doc.groups.empty() || doc.groups.back().material != key)
                    {
                        doc.groups.push_back(FaceGroup{key, runStart, 0});
                    }
                    doc.groups.back().indexCount += produced;
                }
            }

            if (doc.mesh.indices.empty())
            {
                return std::nullopt;
            }
            if (!anyNormal)
            {
                doc.mesh.normals.clear(); // nothing authored: the uploader derives them all
            }
            return doc;
        }
    }

    /*!***********************************************************************
    @brief          Parses OBJ text (v, vt, vn, f) into a MeshData.
    @param          text          Entire OBJ file contents.
    @return         Populated MeshData, or nullopt when a face index is out of
                    range or no triangles were produced.
    *************************************************************************/
    std::optional<MeshData> ParseObj(const std::string& text)
    {
        auto doc = ParseObjDocument(text);
        if (!doc)
        {
            return std::nullopt;
        }
        // A plain mesh has no material list for submeshes to point at, so the
        // whole index buffer stays one implicit run.
        doc->mesh.subMeshes.clear();
        return std::move(doc->mesh);
    }

    /*!***********************************************************************
    @brief          Parses OBJ text plus its MTL libraries into a ModelData.
    @param          text          Entire OBJ file contents.
    @param          sourceDir     Folder the OBJ was read from.
    @return         Populated ModelData, or nullopt when a face index is out of
                    range or no triangles were produced.
    *************************************************************************/
    std::optional<ModelData> ParseObjModel(const std::string& text, const std::filesystem::path& sourceDir)
    {
        auto doc = ParseObjDocument(text);
        if (!doc)
        {
            return std::nullopt;
        }

        ModelData model;

        // Materials the mtllib files declare come first, in the order the files list them;
        // one name seen twice keeps its first declaration.
        for (const std::string& library : doc->libraries)
        {
            const std::filesystem::path libPath(library);
            const auto mtlText = Core::ReadText(libPath.is_absolute() ? libPath : sourceDir / libPath);
            if (!mtlText)
            {
                continue;
            }
            std::vector<MaterialData> declared;
            ParseMtl(*mtlText, declared);
            for (MaterialData& material : declared)
            {
                const bool known =
                    std::any_of(model.materials.begin(), model.materials.end(),
                                [&material](const MaterialData& other) { return other.name == material.name; });
                if (!known)
                {
                    model.materials.push_back(std::move(material));
                }
            }
        }

        // Then every face run: one submesh per run, pointing at the slot with its own name.
        // A material the libraries never declared (or any face with no usemtl at all) gets
        // an empty slot of its own, so nothing is dropped.
        for (const FaceGroup& group : doc->groups)
        {
            const std::string name = group.material.empty() ? "default" : group.material;
            std::size_t slot = 0;
            while (slot < model.materials.size() && model.materials[slot].name != name)
            {
                ++slot;
            }
            if (slot == model.materials.size())
            {
                MaterialData material;
                material.name = name;
                model.materials.push_back(std::move(material));
            }
            doc->mesh.subMeshes.push_back(
                SubMesh{group.baseIndex, group.indexCount, 0, static_cast<std::uint32_t>(slot)});
        }

        model.meshes.push_back(std::move(doc->mesh));
        return model;
    }
}
