/* !
@file               AtelierModel.cpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Implements the .amodel writer and reader. The reader is defensive: a file
    comes from outside the engine, so every count and index is checked before
    it is used and any inconsistency rejects the whole file.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/assets/loaders/AtelierModel.hpp"

// Third-Party Headers
#include <glm/glm.hpp>

// Standard Library Headers
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <string>
#include <utility>

namespace Engine::Assets
{
    namespace
    {
        constexpr std::array<char, 4> kMagic{'A', 'M', 'D', 'L'};
        constexpr std::uint8_t kHasUvs = 1;
        constexpr std::uint8_t kHasNormals = 2;
        constexpr std::uint8_t kHasSkin = 4; // version 2: the mesh's skin, and 4 joints and weights per vertex

        // FNV-1a over the bytes before the checksum: enough to catch truncation and bit flips
        std::uint32_t Checksum(const std::uint8_t* data, std::size_t size)
        {
            std::uint32_t hash = 2166136261u;
            for (std::size_t i = 0; i < size; ++i)
            {
                hash = (hash ^ data[i]) * 16777619u;
            }
            return hash;
        }

        // Appends values to a byte buffer, always little-endian
        class Writer
        {
        public:
            void U8(std::uint8_t value) { bytes.push_back(value); }

            void U16(std::uint16_t value)
            {
                bytes.push_back(static_cast<std::uint8_t>(value));
                bytes.push_back(static_cast<std::uint8_t>(value >> 8));
            }

            void U32(std::uint32_t value)
            {
                for (int shift = 0; shift < 32; shift += 8)
                {
                    bytes.push_back(static_cast<std::uint8_t>(value >> shift));
                }
            }

            void I32(std::int32_t value) { U32(static_cast<std::uint32_t>(value)); }
            void F32(float value) { U32(std::bit_cast<std::uint32_t>(value)); }
            void Count(std::size_t value) { U32(static_cast<std::uint32_t>(value)); }

            void String(const std::string& text)
            {
                Count(text.size());
                bytes.insert(bytes.end(), text.begin(), text.end());
            }

            void Floats(const std::vector<float>& values)
            {
                for (const float value : values)
                {
                    F32(value);
                }
            }

            void Matrix(const glm::mat4& matrix)
            {
                for (int column = 0; column < 4; ++column)
                {
                    for (int row = 0; row < 4; ++row)
                    {
                        F32(matrix[column][row]);
                    }
                }
            }

            void Indices(const std::vector<std::uint32_t>& values)
            {
                Count(values.size());
                for (const std::uint32_t value : values)
                {
                    U32(value);
                }
            }

            std::vector<std::uint8_t> bytes;
        };

        // Reads values from a byte buffer; once anything is missing every further read fails and ok() stays false
        class Reader
        {
        public:
            Reader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

            bool ok() const { return ok_; }
            std::size_t remaining() const { return size_ - position_; }

            std::uint8_t U8()
            {
                if (!Take(1))
                {
                    return 0;
                }
                return data_[position_ - 1];
            }

            std::uint32_t U32()
            {
                if (!Take(4))
                {
                    return 0;
                }
                const std::uint8_t* p = data_ + position_ - 4;
                return static_cast<std::uint32_t>(p[0]) | static_cast<std::uint32_t>(p[1]) << 8 |
                       static_cast<std::uint32_t>(p[2]) << 16 | static_cast<std::uint32_t>(p[3]) << 24;
            }

            std::uint16_t U16()
            {
                if (!Take(2))
                {
                    return 0;
                }
                const std::uint8_t* p = data_ + position_ - 2;
                return static_cast<std::uint16_t>(p[0] | p[1] << 8);
            }

            std::int32_t I32() { return static_cast<std::int32_t>(U32()); }
            float F32() { return std::bit_cast<float>(U32()); }

            glm::mat4 Matrix()
            {
                glm::mat4 matrix(1.0f);
                for (int column = 0; column < 4; ++column)
                {
                    for (int row = 0; row < 4; ++row)
                    {
                        matrix[column][row] = F32();
                    }
                }
                return matrix;
            }

            // A count of items that each need at least minBytes: more than the file can hold is corrupt
            std::size_t Count(std::size_t minBytes)
            {
                const std::size_t count = U32();
                if (ok_ && count > remaining() / (minBytes == 0 ? 1 : minBytes))
                {
                    ok_ = false;
                }
                return ok_ ? count : 0;
            }

            std::string String()
            {
                const std::size_t length = Count(1);
                if (!Take(length))
                {
                    return {};
                }
                return std::string(reinterpret_cast<const char*>(data_ + position_ - length), length);
            }

            std::vector<float> Floats(std::size_t count)
            {
                std::vector<float> values;
                if (!ok_ || count > remaining() / 4)
                {
                    ok_ = false;
                    return values;
                }
                values.reserve(count);
                for (std::size_t i = 0; i < count; ++i)
                {
                    values.push_back(F32());
                }
                return values;
            }

            std::vector<std::uint32_t> Indices()
            {
                const std::size_t count = Count(4);
                std::vector<std::uint32_t> values;
                values.reserve(count);
                for (std::size_t i = 0; i < count; ++i)
                {
                    values.push_back(U32());
                }
                return values;
            }

        private:
            bool Take(std::size_t size)
            {
                if (!ok_ || size > remaining())
                {
                    ok_ = false;
                    return false;
                }
                position_ += size;
                return true;
            }

            const std::uint8_t* data_;
            std::size_t size_;
            std::size_t position_ = 0;
            bool ok_ = true;
        };

        // Whether every value of a list is below limit
        bool AllBelow(const std::vector<std::uint32_t>& values, std::size_t limit)
        {
            for (const std::uint32_t value : values)
            {
                if (value >= limit)
                {
                    return false;
                }
            }
            return true;
        }
    }

    /*!***********************************************************************
    @brief          Serializes a ModelData into the .amodel binary format.
    @param          model         Model to serialize.
    @return         The bytes of the file.
    *************************************************************************/
    std::vector<std::uint8_t> WriteAtelierModel(const ModelData& model)
    {
        Writer out;
        out.bytes.insert(out.bytes.end(), kMagic.begin(), kMagic.end());
        out.U32(kAtelierModelVersion);
        out.String(model.name);

        out.Count(model.materials.size());
        for (const MaterialData& material : model.materials)
        {
            out.String(material.name);
            out.F32(material.baseColorFactor.r);
            out.F32(material.baseColorFactor.g);
            out.F32(material.baseColorFactor.b);
            out.F32(material.baseColorFactor.a);
            out.F32(material.metallic);
            out.F32(material.roughness);
            out.F32(material.emissiveFactor.r);
            out.F32(material.emissiveFactor.g);
            out.F32(material.emissiveFactor.b);
            out.F32(material.emissiveStrength);
            out.String(material.baseColorTexture);
            out.String(material.normalTexture);
            out.String(material.metallicRoughnessTexture);
        }

        out.Count(model.meshes.size());
        for (const MeshData& mesh : model.meshes)
        {
            const std::size_t vertexCount = mesh.positions.size() / 3;
            const bool uvs = mesh.uvs.size() == vertexCount * 2;
            const bool normals = mesh.normals.size() == vertexCount * 3;
            const bool skinned =
                mesh.skin >= 0 && mesh.joints.size() == vertexCount * 4 && mesh.weights.size() == vertexCount * 4;
            out.String(mesh.name);
            out.Count(vertexCount);
            out.U8(static_cast<std::uint8_t>((uvs ? kHasUvs : 0) | (normals ? kHasNormals : 0) |
                                             (skinned ? kHasSkin : 0)));
            out.Floats(std::vector<float>(mesh.positions.begin(),
                                          mesh.positions.begin() + static_cast<std::ptrdiff_t>(vertexCount * 3)));
            if (uvs)
            {
                out.Floats(mesh.uvs);
            }
            if (normals)
            {
                out.Floats(mesh.normals);
            }
            if (skinned)
            {
                out.I32(mesh.skin);
                for (const std::uint16_t joint : mesh.joints)
                {
                    out.U16(joint);
                }
                out.Floats(mesh.weights);
            }
            out.Indices(mesh.indices);
            out.Count(mesh.subMeshes.size());
            for (const SubMesh& run : mesh.subMeshes)
            {
                out.U32(run.baseIndex);
                out.U32(run.indexCount);
                out.I32(run.baseVertex);
                out.U32(run.materialIndex);
            }
        }

        out.Count(model.nodes.size());
        for (const ModelNode& node : model.nodes)
        {
            out.String(node.name);
            out.Matrix(node.localTransform);
            out.Indices(node.meshIndices);
            out.Indices(node.children);
        }
        out.Indices(model.roots);

        out.Count(model.skins.size());
        for (const Skin& skin : model.skins)
        {
            out.String(skin.name);
            out.Indices(skin.joints);
            for (std::size_t j = 0; j < skin.joints.size(); ++j)
            {
                out.Matrix(j < skin.inverseBind.size() ? skin.inverseBind[j] : glm::mat4(1.0f));
            }
        }

        out.Count(model.animations.size());
        for (const AnimationClip& clip : model.animations)
        {
            out.String(clip.name);
            out.F32(clip.duration);
            out.Count(clip.channels.size());
            for (const AnimationChannel& channel : clip.channels)
            {
                const std::size_t width = channel.path == AnimationChannel::Path::Rotation ? 4 : 3;
                const std::size_t keys = std::min(channel.times.size(), channel.values.size() / width);
                out.U32(channel.node);
                out.U8(static_cast<std::uint8_t>(channel.path));
                out.U8(static_cast<std::uint8_t>(channel.interpolation));
                out.Count(keys);
                out.Floats(std::vector<float>(channel.times.begin(),
                                              channel.times.begin() + static_cast<std::ptrdiff_t>(keys)));
                out.Floats(std::vector<float>(channel.values.begin(),
                                              channel.values.begin() + static_cast<std::ptrdiff_t>(keys * width)));
            }
        }

        out.U32(Checksum(out.bytes.data(), out.bytes.size()));
        return std::move(out.bytes);
    }

    /*!***********************************************************************
    @brief          Reads a .amodel file back into a ModelData, rejecting anything inconsistent.
    @param          bytes         Entire file contents.
    @return         The model, or nullopt when the data is not a valid .amodel file of a version this engine can
                    read.
    *************************************************************************/
    std::optional<ModelData> ParseAtelierModel(const std::vector<std::uint8_t>& bytes)
    {
        constexpr std::size_t kHeaderAndChecksum = 4 + 4 + 4;
        if (bytes.size() < kHeaderAndChecksum || std::memcmp(bytes.data(), kMagic.data(), kMagic.size()) != 0)
        {
            return std::nullopt;
        }
        const std::size_t bodySize = bytes.size() - 4;
        Reader tail(bytes.data() + bodySize, 4);
        if (tail.U32() != Checksum(bytes.data(), bodySize))
        {
            return std::nullopt;
        }

        Reader in(bytes.data() + kMagic.size(), bodySize - kMagic.size());
        const std::uint32_t version = in.U32();
        if (version == 0 || version > kAtelierModelVersion)
        {
            return std::nullopt;
        }

        ModelData model;
        model.name = in.String();

        const std::size_t materialCount = in.Count(4 * 10 + 4 * 4);
        model.materials.reserve(materialCount);
        for (std::size_t i = 0; i < materialCount && in.ok(); ++i)
        {
            MaterialData material;
            material.name = in.String();
            material.baseColorFactor.r = in.F32();
            material.baseColorFactor.g = in.F32();
            material.baseColorFactor.b = in.F32();
            material.baseColorFactor.a = in.F32();
            material.metallic = in.F32();
            material.roughness = in.F32();
            material.emissiveFactor.r = in.F32();
            material.emissiveFactor.g = in.F32();
            material.emissiveFactor.b = in.F32();
            material.emissiveStrength = in.F32();
            material.baseColorTexture = in.String();
            material.normalTexture = in.String();
            material.metallicRoughnessTexture = in.String();
            model.materials.push_back(std::move(material));
        }

        const std::size_t meshCount = in.Count(4 + 4 + 1 + 4 + 4);
        model.meshes.reserve(meshCount);
        for (std::size_t i = 0; i < meshCount && in.ok(); ++i)
        {
            MeshData mesh;
            mesh.name = in.String();
            const std::size_t vertexCount = in.Count(12);
            const std::uint8_t flags = in.U8();
            const std::uint8_t known = version >= 2 ? (kHasUvs | kHasNormals | kHasSkin) : (kHasUvs | kHasNormals);
            if ((flags & ~known) != 0)
            {
                return std::nullopt;
            }
            mesh.positions = in.Floats(vertexCount * 3);
            if ((flags & kHasUvs) != 0)
            {
                mesh.uvs = in.Floats(vertexCount * 2);
            }
            if ((flags & kHasNormals) != 0)
            {
                mesh.normals = in.Floats(vertexCount * 3);
            }
            if ((flags & kHasSkin) != 0)
            {
                mesh.skin = in.I32();
                if (mesh.skin < 0 || vertexCount * 4 > in.remaining() / 2)
                {
                    return std::nullopt;
                }
                mesh.joints.reserve(vertexCount * 4);
                for (std::size_t j = 0; j < vertexCount * 4; ++j)
                {
                    mesh.joints.push_back(in.U16());
                }
                mesh.weights = in.Floats(vertexCount * 4);
            }
            mesh.indices = in.Indices();
            if (!in.ok() || !AllBelow(mesh.indices, vertexCount) || mesh.indices.size() % 3 != 0)
            {
                return std::nullopt;
            }

            const std::size_t runCount = in.Count(16);
            for (std::size_t r = 0; r < runCount && in.ok(); ++r)
            {
                SubMesh run;
                run.baseIndex = in.U32();
                run.indexCount = in.U32();
                run.baseVertex = in.I32();
                run.materialIndex = in.U32();
                const std::size_t end = static_cast<std::size_t>(run.baseIndex) + run.indexCount;
                if (end > mesh.indices.size() || run.materialIndex >= model.materials.size())
                {
                    return std::nullopt;
                }
                mesh.subMeshes.push_back(run);
            }
            model.meshes.push_back(std::move(mesh));
        }

        const std::size_t nodeCount = in.Count(4 + 64 + 4 + 4);
        model.nodes.reserve(nodeCount);
        for (std::size_t i = 0; i < nodeCount && in.ok(); ++i)
        {
            ModelNode node;
            node.name = in.String();
            node.localTransform = in.Matrix();
            node.meshIndices = in.Indices();
            node.children = in.Indices();
            model.nodes.push_back(std::move(node));
        }
        model.roots = in.Indices();

        if (version >= 2)
        {
            const std::size_t skinCount = in.Count(4 + 4);
            for (std::size_t s = 0; s < skinCount && in.ok(); ++s)
            {
                Skin skin;
                skin.name = in.String();
                skin.joints = in.Indices();
                if (!in.ok() || skin.joints.size() > in.remaining() / 64)
                {
                    return std::nullopt;
                }
                for (std::size_t j = 0; j < skin.joints.size(); ++j)
                {
                    skin.inverseBind.push_back(in.Matrix());
                }
                model.skins.push_back(std::move(skin));
            }

            const std::size_t clipCount = in.Count(4 + 4 + 4);
            for (std::size_t a = 0; a < clipCount && in.ok(); ++a)
            {
                AnimationClip clip;
                clip.name = in.String();
                clip.duration = in.F32();
                const std::size_t channelCount = in.Count(4 + 1 + 1 + 4);
                for (std::size_t c = 0; c < channelCount && in.ok(); ++c)
                {
                    AnimationChannel channel;
                    channel.node = in.U32();
                    const std::uint8_t path = in.U8();
                    const std::uint8_t interpolation = in.U8();
                    if (path > static_cast<std::uint8_t>(AnimationChannel::Path::Scale) ||
                        interpolation > static_cast<std::uint8_t>(AnimationChannel::Interpolation::Step))
                    {
                        return std::nullopt;
                    }
                    channel.path = static_cast<AnimationChannel::Path>(path);
                    channel.interpolation = static_cast<AnimationChannel::Interpolation>(interpolation);
                    const std::size_t width = channel.path == AnimationChannel::Path::Rotation ? 4 : 3;
                    const std::size_t keys = in.Count(4 * (1 + width));
                    channel.times = in.Floats(keys);
                    channel.values = in.Floats(keys * width);
                    clip.channels.push_back(std::move(channel));
                }
                model.animations.push_back(std::move(clip));
            }
        }

        if (!in.ok() || in.remaining() != 0)
        {
            return std::nullopt;
        }
        for (const ModelNode& node : model.nodes)
        {
            if (!AllBelow(node.meshIndices, model.meshes.size()) || !AllBelow(node.children, model.nodes.size()))
            {
                return std::nullopt;
            }
        }
        if (!AllBelow(model.roots, model.nodes.size()))
        {
            return std::nullopt;
        }
        for (const Skin& skin : model.skins)
        {
            if (!AllBelow(skin.joints, model.nodes.size()))
            {
                return std::nullopt;
            }
        }
        for (const MeshData& mesh : model.meshes)
        {
            if (mesh.skin < 0)
            {
                continue;
            }
            if (static_cast<std::size_t>(mesh.skin) >= model.skins.size())
            {
                return std::nullopt;
            }
            const std::size_t joints = model.skins[static_cast<std::size_t>(mesh.skin)].joints.size();
            for (const std::uint16_t joint : mesh.joints)
            {
                if (joint >= joints)
                {
                    return std::nullopt;
                }
            }
        }
        for (const AnimationClip& clip : model.animations)
        {
            for (const AnimationChannel& channel : clip.channels)
            {
                if (channel.node >= model.nodes.size())
                {
                    return std::nullopt;
                }
            }
        }
        return model;
    }
}
