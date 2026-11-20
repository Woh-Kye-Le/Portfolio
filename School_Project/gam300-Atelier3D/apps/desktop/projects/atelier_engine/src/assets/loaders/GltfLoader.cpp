/* !
@file               GltfLoader.cpp
@author             c.kojat@digipen.edu [100%]
@date               02/11/2026
@brief
    Implements the glTF 2.0 reader for .gltf and .glb files.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/assets/loaders/GltfLoader.hpp"

// Engine Core Headers
#include "engine/core/FileSystem.hpp"

// Third-Party Headers
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

// Standard Library Headers
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

namespace Engine::Assets
{
    namespace
    {
        using nlohmann::json;

        constexpr std::uint32_t kGlbMagic = 0x46546C67;  // "glTF"
        constexpr std::uint32_t kChunkJson = 0x4E4F534A; // "JSON"
        constexpr std::uint32_t kChunkBin = 0x004E4942;  // "BIN\0"
        constexpr int kTriangles = 4;

        // A parsed file: its JSON and the bytes of each of its buffers
        struct Document
        {
            json root;
            std::vector<std::vector<std::uint8_t>> buffers;
            std::filesystem::path folder;
        };

        std::uint32_t ReadU32(const std::vector<std::uint8_t>& bytes, std::size_t at)
        {
            std::uint32_t value = 0;
            std::memcpy(&value, bytes.data() + at, sizeof(value));
            return value; // glTF is little-endian, as are the platforms the engine runs on
        }

        std::optional<std::vector<std::uint8_t>> DecodeBase64(std::string_view text)
        {
            const auto valueOf = [](char c) -> int
            {
                if (c >= 'A' && c <= 'Z')
                    return c - 'A';
                if (c >= 'a' && c <= 'z')
                    return c - 'a' + 26;
                if (c >= '0' && c <= '9')
                    return c - '0' + 52;
                if (c == '+' || c == '-')
                    return 62;
                if (c == '/' || c == '_')
                    return 63;
                return -1;
            };
            std::vector<std::uint8_t> out;
            out.reserve(text.size() * 3 / 4);
            std::uint32_t bits = 0;
            int count = 0;
            for (const char c : text)
            {
                if (c == '=' || std::isspace(static_cast<unsigned char>(c)))
                {
                    continue;
                }
                const int value = valueOf(c);
                if (value < 0)
                {
                    return std::nullopt;
                }
                bits = (bits << 6) | static_cast<std::uint32_t>(value);
                count += 6;
                if (count >= 8)
                {
                    count -= 8;
                    out.push_back(static_cast<std::uint8_t>((bits >> count) & 0xFF));
                }
            }
            return out;
        }

        // A file named by a URI, relative to the model's folder ("my%20texture.png" is "my texture.png")
        std::filesystem::path FileOfUri(const std::string& uri)
        {
            std::string decoded;
            for (std::size_t i = 0; i < uri.size(); ++i)
            {
                if (uri[i] == '%' && i + 2 < uri.size() && std::isxdigit(static_cast<unsigned char>(uri[i + 1])) &&
                    std::isxdigit(static_cast<unsigned char>(uri[i + 2])))
                {
                    decoded.push_back(static_cast<char>(std::stoi(uri.substr(i + 1, 2), nullptr, 16)));
                    i += 2;
                }
                else
                {
                    decoded.push_back(uri[i]);
                }
            }
            return std::filesystem::path(std::u8string(decoded.begin(), decoded.end()));
        }

        // The bytes a URI holds: a data: URI's own, or a file's
        std::optional<std::vector<std::uint8_t>> ReadUri(const std::string& uri, const std::filesystem::path& folder)
        {
            if (uri.starts_with("data:"))
            {
                const auto comma = uri.find(',');
                if (comma == std::string::npos || uri.substr(0, comma).find(";base64") == std::string::npos)
                {
                    return std::nullopt;
                }
                return DecodeBase64(std::string_view(uri).substr(comma + 1));
            }
            return Core::ReadBytes(folder / FileOfUri(uri));
        }

        std::optional<Document> ReadDocument(const std::filesystem::path& path)
        {
            const auto bytes = Core::ReadBytes(path);
            if (!bytes)
            {
                return std::nullopt;
            }
            Document document;
            document.folder = path.parent_path();
            std::vector<std::uint8_t> binChunk;
            bool hasBin = false;
            try
            {
                if (bytes->size() >= 12 && ReadU32(*bytes, 0) == kGlbMagic)
                {
                    // .glb: a header, then chunks of (length, type, data); the JSON first, then maybe the binary one
                    if (ReadU32(*bytes, 4) != 2)
                    {
                        return std::nullopt;
                    }
                    const std::size_t end = std::min<std::size_t>(ReadU32(*bytes, 8), bytes->size());
                    bool hasJson = false;
                    for (std::size_t at = 12; at + 8 <= end;)
                    {
                        const std::size_t length = ReadU32(*bytes, at);
                        const std::uint32_t type = ReadU32(*bytes, at + 4);
                        const std::size_t data = at + 8;
                        if (length > end - data)
                        {
                            return std::nullopt;
                        }
                        if (type == kChunkJson && !hasJson)
                        {
                            document.root = json::parse(bytes->begin() + static_cast<std::ptrdiff_t>(data),
                                                        bytes->begin() + static_cast<std::ptrdiff_t>(data + length));
                            hasJson = true;
                        }
                        else if (type == kChunkBin && !hasBin)
                        {
                            binChunk.assign(bytes->begin() + static_cast<std::ptrdiff_t>(data),
                                            bytes->begin() + static_cast<std::ptrdiff_t>(data + length));
                            hasBin = true;
                        }
                        at = data + ((length + 3) & ~std::size_t{3});
                    }
                    if (!hasJson)
                    {
                        return std::nullopt;
                    }
                }
                else
                {
                    document.root = json::parse(bytes->begin(), bytes->end());
                }
            }
            catch (const json::exception&)
            {
                return std::nullopt;
            }
            if (!document.root.is_object())
            {
                return std::nullopt;
            }

            // Buffers: a .glb's first buffer without a URI is its binary chunk; the others are files or data: URIs
            const json& buffers = document.root.value("buffers", json::array());
            for (std::size_t i = 0; buffers.is_array() && i < buffers.size(); ++i)
            {
                const json& buffer = buffers[i];
                if (buffer.contains("uri") && buffer["uri"].is_string())
                {
                    document.buffers.push_back(ReadUri(buffer["uri"].get<std::string>(), document.folder)
                                                   .value_or(std::vector<std::uint8_t>{}));
                }
                else
                {
                    document.buffers.push_back(i == 0 && hasBin ? std::exchange(binChunk, {})
                                                                : std::vector<std::uint8_t>{});
                }
            }
            return document;
        }

        const json* At(const json& root, const char* list, const json& index)
        {
            if (!index.is_number_integer() || !root.contains(list) || !root[list].is_array())
            {
                return nullptr;
            }
            const auto i = index.get<long long>();
            return i >= 0 && static_cast<std::size_t>(i) < root[list].size() ? &root[list][static_cast<std::size_t>(i)]
                                                                             : nullptr;
        }

        int ComponentCount(const std::string& type)
        {
            static constexpr std::array<std::pair<std::string_view, int>, 7> kTypes{
                {{"SCALAR", 1}, {"VEC2", 2}, {"VEC3", 3}, {"VEC4", 4}, {"MAT2", 4}, {"MAT3", 9}, {"MAT4", 16}}};
            for (const auto& [name, count] : kTypes)
            {
                if (name == type)
                {
                    return count;
                }
            }
            return 0;
        }

        std::size_t ComponentSize(int componentType)
        {
            switch (componentType)
            {
            case 5120: // byte
            case 5121: // unsigned byte
                return 1;
            case 5122: // short
            case 5123: // unsigned short
                return 2;
            case 5125: // unsigned int
            case 5126: // float
                return 4;
            default: return 0;
            }
        }

        // One component as a number; integers are scaled to 0..1 (or -1..1) when the accessor is normalized
        double ReadComponent(const std::uint8_t* at, int componentType, bool normalized)
        {
            switch (componentType)
            {
            case 5120:
            {
                std::int8_t v = 0;
                std::memcpy(&v, at, 1);
                return normalized ? std::max(v / 127.0, -1.0) : v;
            }
            case 5121: return normalized ? *at / 255.0 : *at;
            case 5122:
            {
                std::int16_t v = 0;
                std::memcpy(&v, at, 2);
                return normalized ? std::max(v / 32767.0, -1.0) : v;
            }
            case 5123:
            {
                std::uint16_t v = 0;
                std::memcpy(&v, at, 2);
                return normalized ? v / 65535.0 : v;
            }
            case 5125:
            {
                std::uint32_t v = 0;
                std::memcpy(&v, at, 4);
                return v;
            }
            case 5126:
            {
                float v = 0.0f;
                std::memcpy(&v, at, 4);
                return v;
            }
            default: return 0.0;
            }
        }

        // An accessor's elements, `components` numbers each (an accessor of another size is not read); nullopt when
        // it does not fit in its buffer. An accessor with no buffer view is all zeros, as the format says.
        std::optional<std::vector<double>> ReadAccessor(const Document& document, const json& index, int components)
        {
            const json* accessor = At(document.root, "accessors", index);
            if (!accessor || !accessor->is_object())
            {
                return std::nullopt;
            }
            const int componentType = accessor->value("componentType", 0);
            const std::size_t count = accessor->value("count", std::size_t{0});
            const bool normalized = accessor->value("normalized", false);
            const std::size_t size = ComponentSize(componentType);
            if (size == 0 || ComponentCount(accessor->value("type", std::string{})) != components)
            {
                return std::nullopt;
            }
            std::vector<double> values(count * static_cast<std::size_t>(components), 0.0);
            const json* view = accessor->contains("bufferView")
                                   ? At(document.root, "bufferViews", (*accessor)["bufferView"])
                                   : nullptr;
            if (!view)
            {
                return accessor->contains("bufferView") ? std::nullopt : std::optional(std::move(values));
            }
            const std::size_t buffer = view->value("buffer", std::size_t{0});
            if (buffer >= document.buffers.size())
            {
                return std::nullopt;
            }
            const std::vector<std::uint8_t>& bytes = document.buffers[buffer];
            const std::size_t element = size * static_cast<std::size_t>(components);
            const std::size_t stride = std::max(view->value("byteStride", std::size_t{0}), element);
            const std::size_t start =
                view->value("byteOffset", std::size_t{0}) + accessor->value("byteOffset", std::size_t{0});
            const std::size_t viewEnd =
                view->value("byteOffset", std::size_t{0}) + view->value("byteLength", std::size_t{0});
            if (count > 0 && (start + (count - 1) * stride + element > std::min(viewEnd, bytes.size())))
            {
                return std::nullopt;
            }
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::uint8_t* at = bytes.data() + start + i * stride;
                for (int c = 0; c < components; ++c)
                {
                    values[i * static_cast<std::size_t>(components) + static_cast<std::size_t>(c)] =
                        ReadComponent(at + static_cast<std::size_t>(c) * size, componentType, normalized);
                }
            }
            return values;
        }

        // A texture slot ({"index": n}) as the path of its image: a file beside the model, or "<file>#<image>"
        std::string TexturePath(const Document& document, const json& slot, const std::string& fileName)
        {
            if (!slot.is_object() || !slot.contains("index"))
            {
                return {};
            }
            const json* texture = At(document.root, "textures", slot["index"]);
            if (!texture || !texture->contains("source"))
            {
                return {};
            }
            const json& source = (*texture)["source"];
            const json* image = At(document.root, "images", source);
            if (!image)
            {
                return {};
            }
            if (image->contains("uri") && (*image)["uri"].is_string())
            {
                const std::string uri = (*image)["uri"].get<std::string>();
                if (!uri.starts_with("data:"))
                {
                    return FileOfUri(uri).generic_string();
                }
            }
            return fileName + "#" + std::to_string(source.get<long long>());
        }

        MaterialData ReadMaterial(const Document& document, const json& source, std::size_t index,
                                  const std::string& fileName)
        {
            MaterialData material;
            material.name = source.value("name", std::string{});
            if (material.name.empty())
            {
                material.name = "material_" + std::to_string(index);
            }
            // glTF's defaults: fully metallic and rough unless the file says otherwise
            material.metallic = 1.0f;
            material.roughness = 1.0f;
            if (source.contains("pbrMetallicRoughness") && source["pbrMetallicRoughness"].is_object())
            {
                const json& pbr = source["pbrMetallicRoughness"];
                if (pbr.contains("baseColorFactor") && pbr["baseColorFactor"].is_array() &&
                    pbr["baseColorFactor"].size() == 4)
                {
                    for (int c = 0; c < 4; ++c)
                    {
                        material.baseColorFactor[c] = pbr["baseColorFactor"][static_cast<std::size_t>(c)].get<float>();
                    }
                }
                material.metallic = pbr.value("metallicFactor", 1.0f);
                material.roughness = pbr.value("roughnessFactor", 1.0f);
                material.baseColorTexture = TexturePath(document, pbr.value("baseColorTexture", json{}), fileName);
                material.metallicRoughnessTexture =
                    TexturePath(document, pbr.value("metallicRoughnessTexture", json{}), fileName);
            }
            material.normalTexture = TexturePath(document, source.value("normalTexture", json{}), fileName);
            if (source.contains("emissiveFactor") && source["emissiveFactor"].is_array() &&
                source["emissiveFactor"].size() == 3)
            {
                for (int c = 0; c < 3; ++c)
                {
                    material.emissiveFactor[c] = source["emissiveFactor"][static_cast<std::size_t>(c)].get<float>();
                }
            }
            if (material.emissiveFactor != glm::vec3(0.0f))
            {
                material.emissiveStrength = 1.0f;
                if (const json* strength = source.contains("extensions") ? &source["extensions"] : nullptr;
                    strength && strength->contains("KHR_materials_emissive_strength"))
                {
                    material.emissiveStrength =
                        (*strength)["KHR_materials_emissive_strength"].value("emissiveStrength", 1.0f);
                }
            }
            return material;
        }

        // A glTF mesh: its triangle primitives one after another, each a SubMesh of its own material.
        // `defaultMaterial` is used by primitives that name none.
        std::optional<MeshData> ReadMesh(const Document& document, const json& source, std::uint32_t defaultMaterial,
                                         bool& usedDefault)
        {
            MeshData mesh;
            mesh.name = source.value("name", std::string{});
            const json& primitives = source.value("primitives", json::array());
            bool anyUvs = false;
            bool anyNormals = false;
            bool anySkin = false;
            struct Part
            {
                std::vector<double> positions, uvs, normals, joints, weights;
                std::vector<std::uint32_t> indices;
                std::uint32_t material = 0;
            };
            std::vector<Part> parts;
            for (const json& primitive : primitives)
            {
                if (!primitive.is_object() || primitive.value("mode", kTriangles) != kTriangles ||
                    !primitive.contains("attributes"))
                {
                    continue; // points and lines are not drawn
                }
                const json& attributes = primitive["attributes"];
                Part part;
                auto positions =
                    attributes.contains("POSITION") ? ReadAccessor(document, attributes["POSITION"], 3) : std::nullopt;
                if (!positions || positions->empty())
                {
                    continue;
                }
                part.positions = std::move(*positions);
                const std::size_t vertices = part.positions.size() / 3;
                if (attributes.contains("TEXCOORD_0"))
                {
                    if (auto uvs = ReadAccessor(document, attributes["TEXCOORD_0"], 2);
                        uvs && uvs->size() == vertices * 2)
                    {
                        part.uvs = std::move(*uvs);
                        anyUvs = true;
                    }
                }
                if (attributes.contains("NORMAL"))
                {
                    if (auto normals = ReadAccessor(document, attributes["NORMAL"], 3);
                        normals && normals->size() == vertices * 3)
                    {
                        part.normals = std::move(*normals);
                        anyNormals = true;
                    }
                }
                // Skinning: the four bones each corner follows (indices into the skin's joints) and how much
                if (attributes.contains("JOINTS_0") && attributes.contains("WEIGHTS_0"))
                {
                    auto joints = ReadAccessor(document, attributes["JOINTS_0"], 4);
                    auto weights = ReadAccessor(document, attributes["WEIGHTS_0"], 4);
                    if (joints && weights && joints->size() == vertices * 4 && weights->size() == vertices * 4)
                    {
                        part.joints = std::move(*joints);
                        part.weights = std::move(*weights);
                        anySkin = true;
                    }
                }
                if (primitive.contains("indices"))
                {
                    const auto indices = ReadAccessor(document, primitive["indices"], 1);
                    if (!indices)
                    {
                        continue;
                    }
                    for (const double index : *indices)
                    {
                        if (index < 0.0 || index >= static_cast<double>(vertices))
                        {
                            part.indices.clear();
                            break;
                        }
                        part.indices.push_back(static_cast<std::uint32_t>(index));
                    }
                }
                else
                {
                    for (std::uint32_t i = 0; i < vertices; ++i)
                    {
                        part.indices.push_back(i);
                    }
                }
                part.indices.resize(part.indices.size() / 3 * 3);
                if (part.indices.empty())
                {
                    continue;
                }
                const json* material =
                    primitive.contains("material") ? At(document.root, "materials", primitive["material"]) : nullptr;
                if (material)
                {
                    part.material = primitive["material"].get<std::uint32_t>();
                }
                else
                {
                    part.material = defaultMaterial;
                    usedDefault = true;
                }
                parts.push_back(std::move(part));
            }
            if (parts.empty())
            {
                return std::nullopt;
            }

            for (Part& part : parts)
            {
                const auto first = static_cast<std::uint32_t>(mesh.positions.size() / 3);
                const std::size_t vertices = part.positions.size() / 3;
                for (const double value : part.positions)
                {
                    mesh.positions.push_back(static_cast<float>(value));
                }
                if (anyUvs)
                {
                    // glTF's images start at the top, the engine's (as OpenGL's) at the bottom
                    for (std::size_t v = 0; v < vertices; ++v)
                    {
                        const bool has = !part.uvs.empty();
                        mesh.uvs.push_back(has ? static_cast<float>(part.uvs[v * 2]) : 0.0f);
                        mesh.uvs.push_back(has ? 1.0f - static_cast<float>(part.uvs[v * 2 + 1]) : 0.0f);
                    }
                }
                if (anyNormals)
                {
                    if (part.normals.empty())
                    {
                        // This part has none but another has: each corner gets the normal of the faces around it
                        std::vector<glm::vec3> normals(vertices, glm::vec3(0.0f));
                        const auto corner = [&part](std::uint32_t i)
                        {
                            const std::size_t at = std::size_t{3} * i;
                            return glm::vec3(part.positions[at], part.positions[at + 1], part.positions[at + 2]);
                        };
                        for (std::size_t t = 0; t + 2 < part.indices.size(); t += 3)
                        {
                            const std::uint32_t a = part.indices[t], b = part.indices[t + 1], c = part.indices[t + 2];
                            const glm::vec3 face = glm::cross(corner(b) - corner(a), corner(c) - corner(a));
                            normals[a] += face;
                            normals[b] += face;
                            normals[c] += face;
                        }
                        for (const glm::vec3& n : normals)
                        {
                            const float length = glm::length(n);
                            const glm::vec3 unit = length > 0.0f ? n / length : glm::vec3(0.0f, 1.0f, 0.0f);
                            mesh.normals.insert(mesh.normals.end(), {unit.x, unit.y, unit.z});
                        }
                    }
                    else
                    {
                        for (const double value : part.normals)
                        {
                            mesh.normals.push_back(static_cast<float>(value));
                        }
                    }
                }
                if (anySkin)
                {
                    // Weights are made to add up to 1; a corner with none (or a part without skinning) stays put
                    for (std::size_t v = 0; v < vertices; ++v)
                    {
                        double total = 0.0;
                        for (std::size_t k = 0; !part.weights.empty() && k < 4; ++k)
                        {
                            total += std::max(part.weights[v * 4 + k], 0.0);
                        }
                        for (std::size_t k = 0; k < 4; ++k)
                        {
                            const double joint = part.joints.empty() ? 0.0 : part.joints[v * 4 + k];
                            const double weight = total > 0.0 ? std::max(part.weights[v * 4 + k], 0.0) / total : 0.0;
                            mesh.joints.push_back(static_cast<std::uint16_t>(std::clamp(joint, 0.0, 65535.0)));
                            mesh.weights.push_back(static_cast<float>(weight));
                        }
                    }
                }
                SubMesh run;
                run.baseIndex = static_cast<std::uint32_t>(mesh.indices.size());
                run.indexCount = static_cast<std::uint32_t>(part.indices.size());
                run.materialIndex = part.material;
                for (const std::uint32_t index : part.indices)
                {
                    mesh.indices.push_back(first + index);
                }
                mesh.subMeshes.push_back(run);
            }
            return mesh;
        }

        glm::mat4 NodeTransform(const json& node)
        {
            if (node.contains("matrix") && node["matrix"].is_array() && node["matrix"].size() == 16)
            {
                std::array<float, 16> m{};
                for (std::size_t i = 0; i < 16; ++i)
                {
                    m[i] = node["matrix"][i].get<float>();
                }
                return glm::make_mat4(m.data()); // both column-major
            }
            const auto vec = [&node](const char* key, std::size_t size, std::array<float, 4> fallback)
            {
                if (node.contains(key) && node[key].is_array() && node[key].size() == size)
                {
                    for (std::size_t i = 0; i < size; ++i)
                    {
                        fallback[i] = node[key][i].get<float>();
                    }
                }
                return fallback;
            };
            const auto t = vec("translation", 3, {0.0f, 0.0f, 0.0f, 0.0f});
            const auto r = vec("rotation", 4, {0.0f, 0.0f, 0.0f, 1.0f}); // x, y, z, w
            const auto s = vec("scale", 3, {1.0f, 1.0f, 1.0f, 0.0f});
            return glm::translate(glm::mat4(1.0f), glm::vec3(t[0], t[1], t[2])) *
                   glm::mat4_cast(glm::quat(r[3], r[0], r[1], r[2])) *
                   glm::scale(glm::mat4(1.0f), glm::vec3(s[0], s[1], s[2]));
        }

        // The skins (skeletons) of the file; each mesh a skinned node carries is bent by that node's skin
        void ReadSkins(const Document& document, ModelData& model)
        {
            const json& root = document.root;
            const json& skins = root.value("skins", json::array());
            for (std::size_t s = 0; s < skins.size(); ++s)
            {
                const json& source = skins[s];
                Skin skin;
                skin.name = source.value("name", "Skin " + std::to_string(s + 1));
                for (const json& joint : source.value("joints", json::array()))
                {
                    if (!At(root, "nodes", joint))
                    {
                        skin.joints.clear();
                        break;
                    }
                    skin.joints.push_back(joint.get<std::uint32_t>());
                }
                // Without its inverse bind matrices every bone is at the model's origin at rest
                const auto matrices = source.contains("inverseBindMatrices")
                                          ? ReadAccessor(document, source["inverseBindMatrices"], 16)
                                          : std::nullopt;
                for (std::size_t j = 0; j < skin.joints.size(); ++j)
                {
                    glm::mat4 inverseBind(1.0f);
                    if (matrices && matrices->size() >= (j + 1) * 16)
                    {
                        for (std::size_t c = 0; c < 16; ++c)
                        {
                            inverseBind[static_cast<int>(c / 4)][static_cast<int>(c % 4)] =
                                static_cast<float>((*matrices)[j * 16 + c]); // column-major, as glm
                        }
                    }
                    skin.inverseBind.push_back(inverseBind);
                }
                model.skins.push_back(std::move(skin));
            }

            for (const json& node : root.value("nodes", json::array()))
            {
                if (!node.contains("skin") || !node.contains("mesh") || !At(root, "skins", node["skin"]) ||
                    !At(root, "meshes", node["mesh"]))
                {
                    continue;
                }
                MeshData& mesh = model.meshes[node["mesh"].get<std::size_t>()];
                const auto skin = node["skin"].get<std::int32_t>();
                const std::size_t joints = model.skins[static_cast<std::size_t>(skin)].joints.size();
                const bool fits = std::all_of(mesh.joints.begin(), mesh.joints.end(),
                                              [joints](std::uint16_t joint) { return joint < joints; });
                if (!mesh.joints.empty() && fits)
                {
                    mesh.skin = skin;
                }
            }
            // A mesh no skin was given to (or one whose bones are out of range) is drawn as it is
            for (MeshData& mesh : model.meshes)
            {
                if (mesh.skin < 0)
                {
                    mesh.joints.clear();
                    mesh.weights.clear();
                }
            }
        }

        // The file's animations; morph target ("weights") channels are not read
        void ReadAnimations(const Document& document, ModelData& model)
        {
            const json& root = document.root;
            const json& animations = root.value("animations", json::array());
            for (std::size_t a = 0; a < animations.size(); ++a)
            {
                const json& source = animations[a];
                AnimationClip clip;
                clip.name = source.value("name", std::string{});
                if (clip.name.empty())
                {
                    clip.name = "Animation " + std::to_string(a + 1);
                }
                const json& samplers = source.value("samplers", json::array());
                for (const json& channelSource : source.value("channels", json::array()))
                {
                    const json& target = channelSource.value("target", json::object());
                    const std::string path = target.value("path", std::string{});
                    AnimationChannel channel;
                    if (path == "translation")
                    {
                        channel.path = AnimationChannel::Path::Translation;
                    }
                    else if (path == "rotation")
                    {
                        channel.path = AnimationChannel::Path::Rotation;
                    }
                    else if (path == "scale")
                    {
                        channel.path = AnimationChannel::Path::Scale;
                    }
                    else
                    {
                        continue;
                    }
                    if (!target.contains("node") || !At(root, "nodes", target["node"]) ||
                        !channelSource.contains("sampler") || !channelSource["sampler"].is_number_integer())
                    {
                        continue;
                    }
                    const auto samplerIndex = channelSource["sampler"].get<std::size_t>();
                    if (samplerIndex >= samplers.size())
                    {
                        continue;
                    }
                    const json& sampler = samplers[samplerIndex];
                    const std::string interpolation = sampler.value("interpolation", std::string("LINEAR"));
                    const int width = channel.path == AnimationChannel::Path::Rotation ? 4 : 3;
                    const auto times =
                        sampler.contains("input") ? ReadAccessor(document, sampler["input"], 1) : std::nullopt;
                    const auto values =
                        sampler.contains("output") ? ReadAccessor(document, sampler["output"], width) : std::nullopt;
                    if (!times || !values || times->empty())
                    {
                        continue;
                    }
                    // A cubic spline stores (in-tangent, value, out-tangent) per key: only the values are kept, and
                    // played linearly
                    const bool cubic = interpolation == "CUBICSPLINE";
                    const auto components = static_cast<std::size_t>(width);
                    const std::size_t perKey = components * (cubic ? 3 : 1);
                    if (values->size() < times->size() * perKey)
                    {
                        continue;
                    }
                    channel.node = target["node"].get<std::uint32_t>();
                    channel.interpolation = interpolation == "STEP" ? AnimationChannel::Interpolation::Step
                                                                    : AnimationChannel::Interpolation::Linear;
                    const auto& keyTimes = *times;
                    const auto& keyValues = *values;
                    for (std::size_t k = 0; k < keyTimes.size(); ++k)
                    {
                        channel.times.push_back(static_cast<float>(keyTimes[k]));
                        const std::size_t first = k * perKey + (cubic ? components : 0);
                        for (std::size_t c = 0; c < components; ++c)
                        {
                            channel.values.push_back(static_cast<float>(keyValues[first + c]));
                        }
                    }
                    clip.duration = std::max(clip.duration, channel.times.back());
                    clip.channels.push_back(std::move(channel));
                }
                if (!clip.channels.empty())
                {
                    model.animations.push_back(std::move(clip));
                }
            }
        }

        // glTF models face +Z, and the engine's forward (where Motion blocks walk) is -Z: a node above the file's
        // roots turns the whole model round, skeleton and animations with it (no clip moves the new node)
        void FaceForward(ModelData& model)
        {
            if (model.nodes.empty())
            {
                return;
            }
            ModelNode turn;
            turn.name = "Turned to face forward";
            turn.localTransform = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            turn.children = model.roots;
            if (turn.children.empty())
            {
                std::vector<bool> isChild(model.nodes.size(), false);
                for (const ModelNode& node : model.nodes)
                {
                    for (const std::uint32_t child : node.children)
                    {
                        isChild[child] = true;
                    }
                }
                for (std::uint32_t i = 0; i < model.nodes.size(); ++i)
                {
                    if (!isChild[i])
                    {
                        turn.children.push_back(i);
                    }
                }
            }
            model.roots = {static_cast<std::uint32_t>(model.nodes.size())};
            model.nodes.push_back(std::move(turn));
        }

        // "car.glb#3" -> (car.glb, 3)
        std::optional<std::pair<std::filesystem::path, std::size_t>> SplitPacked(const std::filesystem::path& path)
        {
            const std::string text = path.generic_string();
            const auto hash = text.rfind('#');
            if (hash == std::string::npos || hash + 1 >= text.size())
            {
                return std::nullopt;
            }
            const std::filesystem::path file = std::filesystem::path(text.substr(0, hash));
            const std::string extension = Core::Extension(file);
            const std::string number = text.substr(hash + 1);
            if ((extension != ".glb" && extension != ".gltf") ||
                !std::all_of(number.begin(), number.end(), [](unsigned char c) { return std::isdigit(c); }) ||
                number.size() > 9)
            {
                return std::nullopt;
            }
            return std::pair{file, static_cast<std::size_t>(std::stoul(number))};
        }
    }

    std::optional<ModelData> LoadGltfModel(const std::filesystem::path& path)
    {
        const auto document = ReadDocument(path);
        if (!document)
        {
            return std::nullopt;
        }
        const json& root = document->root;
        const std::string fileName = path.filename().string();

        ModelData model;
        model.name = path.stem().string();
        try
        {
            const json& materials = root.value("materials", json::array());
            for (std::size_t i = 0; i < materials.size(); ++i)
            {
                model.materials.push_back(ReadMaterial(*document, materials[i], i, fileName));
            }
            const auto defaultMaterial = static_cast<std::uint32_t>(model.materials.size());
            bool usedDefault = false;

            // A mesh with nothing to draw stays as an empty MeshData so the nodes' mesh indices still match
            const json& meshes = root.value("meshes", json::array());
            bool anyTriangles = false;
            for (const json& source : meshes)
            {
                auto mesh = ReadMesh(*document, source, defaultMaterial, usedDefault);
                anyTriangles = anyTriangles || mesh.has_value();
                model.meshes.push_back(mesh ? std::move(*mesh) : MeshData{});
            }
            if (!anyTriangles)
            {
                return std::nullopt;
            }
            if (usedDefault)
            {
                MaterialData material;
                material.name = model.name + "_material";
                model.materials.push_back(std::move(material));
            }

            const json& nodes = root.value("nodes", json::array());
            for (const json& source : nodes)
            {
                ModelNode node;
                node.name = source.value("name", std::string{});
                node.localTransform = NodeTransform(source);
                if (source.contains("mesh") && At(root, "meshes", source["mesh"]))
                {
                    node.meshIndices.push_back(source["mesh"].get<std::uint32_t>());
                }
                for (const json& child : source.value("children", json::array()))
                {
                    if (At(root, "nodes", child))
                    {
                        node.children.push_back(child.get<std::uint32_t>());
                    }
                }
                model.nodes.push_back(std::move(node));
            }
            const json* scene = At(root, "scenes", root.value("scene", json(0)));
            if (scene && scene->contains("nodes"))
            {
                for (const json& index : (*scene)["nodes"])
                {
                    if (At(root, "nodes", index))
                    {
                        model.roots.push_back(index.get<std::uint32_t>());
                    }
                }
            }
            ReadSkins(*document, model);
            ReadAnimations(*document, model);
            FaceForward(model);
        }
        catch (const json::exception&)
        {
            return std::nullopt;
        }
        return model;
    }

    std::filesystem::path GltfContainerOf(const std::filesystem::path& path)
    {
        const auto packed = SplitPacked(path);
        return packed ? packed->first : path;
    }

    std::optional<std::vector<std::uint8_t>> ReadGltfImage(const std::filesystem::path& path)
    {
        const auto packed = SplitPacked(path);
        if (!packed)
        {
            return std::nullopt;
        }
        const auto document = ReadDocument(packed->first);
        if (!document)
        {
            return std::nullopt;
        }
        try
        {
            const json* image = At(document->root, "images", json(packed->second));
            if (!image)
            {
                return std::nullopt;
            }
            if (image->contains("uri") && (*image)["uri"].is_string())
            {
                return ReadUri((*image)["uri"].get<std::string>(), document->folder);
            }
            const json* view =
                image->contains("bufferView") ? At(document->root, "bufferViews", (*image)["bufferView"]) : nullptr;
            if (!view)
            {
                return std::nullopt;
            }
            const std::size_t buffer = view->value("buffer", std::size_t{0});
            const std::size_t offset = view->value("byteOffset", std::size_t{0});
            const std::size_t length = view->value("byteLength", std::size_t{0});
            if (buffer >= document->buffers.size() || offset > document->buffers[buffer].size() ||
                length > document->buffers[buffer].size() - offset)
            {
                return std::nullopt;
            }
            const auto& bytes = document->buffers[buffer];
            return std::vector<std::uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                             bytes.begin() + static_cast<std::ptrdiff_t>(offset + length));
        }
        catch (const json::exception&)
        {
            return std::nullopt;
        }
    }
}
