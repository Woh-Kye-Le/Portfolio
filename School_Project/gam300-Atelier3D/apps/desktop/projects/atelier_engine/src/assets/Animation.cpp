/* !
@file               Animation.cpp
@author             c.kojat@digipen.edu [100%]
@date               02/11/2026
@brief
    Implements the skeletal animation maths: keyframe sampling, transform
    decomposition and bone matrices.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/assets/Animation.hpp"

// Third-Party Headers
#include <glm/gtc/matrix_transform.hpp>

// Standard Library Headers
#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <unordered_map>

namespace Engine::Assets
{
    namespace
    {
        // The keys either side of time, and how far it is from the first to the second (0..1)
        struct Between
        {
            std::size_t from = 0;
            std::size_t to = 0;
            float t = 0.0f;
        };

        Between Find(const std::vector<float>& times, float time, AnimationChannel::Interpolation interpolation)
        {
            if (times.size() < 2 || time <= times.front())
            {
                return {};
            }
            if (time >= times.back())
            {
                return {times.size() - 1, times.size() - 1, 0.0f};
            }
            const auto next =
                static_cast<std::size_t>(std::upper_bound(times.begin(), times.end(), time) - times.begin());
            const std::size_t from = next - 1;
            if (interpolation == AnimationChannel::Interpolation::Step)
            {
                return {from, from, 0.0f};
            }
            const float span = times[next] - times[from];
            return {from, next, span > 0.0f ? (time - times[from]) / span : 0.0f};
        }

        glm::vec3 Vec3At(const std::vector<float>& values, std::size_t key)
        {
            return glm::vec3(values[key * 3], values[key * 3 + 1], values[key * 3 + 2]);
        }

        glm::quat QuatAt(const std::vector<float>& values, std::size_t key)
        {
            // Stored x, y, z, w (as glTF); glm's constructor takes w first
            return glm::quat(values[key * 4 + 3], values[key * 4], values[key * 4 + 1], values[key * 4 + 2]);
        }
    }

    NodePose Decompose(const glm::mat4& transform)
    {
        NodePose pose;
        pose.translation = glm::vec3(transform[3]);
        glm::vec3 x(transform[0]);
        const glm::vec3 y(transform[1]);
        const glm::vec3 z(transform[2]);
        pose.scale = glm::vec3(glm::length(x), glm::length(y), glm::length(z));
        if (glm::dot(glm::cross(x, y), z) < 0.0f)
        {
            pose.scale.x = -pose.scale.x; // mirrored
        }
        if (pose.scale.x == 0.0f || pose.scale.y == 0.0f || pose.scale.z == 0.0f)
        {
            return pose; // flattened: no rotation can be read from it
        }
        const glm::mat3 rotation(glm::vec3(transform[0]) / pose.scale.x, y / pose.scale.y, z / pose.scale.z);
        pose.rotation = glm::normalize(glm::quat_cast(rotation));
        return pose;
    }

    glm::mat4 Compose(const NodePose& pose)
    {
        return glm::translate(glm::mat4(1.0f), pose.translation) * glm::mat4_cast(pose.rotation) *
               glm::scale(glm::mat4(1.0f), pose.scale);
    }

    int FindClip(const std::vector<AnimationClip>& clips, std::string_view name)
    {
        const auto same = [](std::string_view a, std::string_view b)
        {
            return a.size() == b.size() &&
                   std::equal(a.begin(), a.end(), b.begin(),
                              [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
        };
        for (std::size_t i = 0; i < clips.size(); ++i)
        {
            if (same(clips[i].name, name))
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    std::vector<glm::mat4> SampleClip(const AnimationClip& clip, float time, const std::vector<glm::mat4>& rest)
    {
        // Each animated node starts from its rest parts, and the channels replace the parts they move
        std::vector<NodePose> poses(rest.size());
        std::vector<bool> moved(rest.size(), false);
        for (const AnimationChannel& channel : clip.channels)
        {
            if (channel.node >= rest.size() || channel.times.empty())
            {
                continue;
            }
            const std::size_t width = channel.path == AnimationChannel::Path::Rotation ? 4 : 3;
            if (channel.values.size() < channel.times.size() * width)
            {
                continue;
            }
            if (!moved[channel.node])
            {
                poses[channel.node] = Decompose(rest[channel.node]);
                moved[channel.node] = true;
            }
            const Between keys = Find(channel.times, time, channel.interpolation);
            NodePose& pose = poses[channel.node];
            switch (channel.path)
            {
            case AnimationChannel::Path::Translation:
                pose.translation = glm::mix(Vec3At(channel.values, keys.from), Vec3At(channel.values, keys.to), keys.t);
                break;
            case AnimationChannel::Path::Scale:
                pose.scale = glm::mix(Vec3At(channel.values, keys.from), Vec3At(channel.values, keys.to), keys.t);
                break;
            case AnimationChannel::Path::Rotation:
            {
                const glm::quat a = QuatAt(channel.values, keys.from);
                glm::quat b = QuatAt(channel.values, keys.to);
                if (glm::dot(a, b) < 0.0f)
                {
                    b = -b; // the short way round
                }
                pose.rotation = glm::normalize(glm::slerp(a, b, keys.t));
                break;
            }
            }
        }

        std::vector<glm::mat4> locals = rest;
        for (std::size_t i = 0; i < rest.size(); ++i)
        {
            if (moved[i])
            {
                locals[i] = Compose(poses[i]);
            }
        }
        return locals;
    }

    namespace
    {
        // Each mesh of the model as it stands at rest, its vertices placed: by its node, or a skinned one bent by its
        // skeleton's rest pose (which is how the model is drawn without a clip)
        void ForEachRestMesh(const ModelData& model,
                             const std::function<void(const MeshData&, const std::vector<glm::vec3>&)>& visit)
        {
            const auto place = [&](const MeshData& mesh, const glm::mat4& where, const std::vector<glm::mat4>* bones)
            {
                const std::size_t vertices = mesh.positions.size() / 3;
                const bool skinned = bones && mesh.joints.size() == vertices * 4 && mesh.weights.size() == vertices * 4;
                std::vector<glm::vec3> placed;
                placed.reserve(vertices);
                for (std::size_t v = 0; v < vertices; ++v)
                {
                    const glm::vec4 point(mesh.positions[v * 3], mesh.positions[v * 3 + 1], mesh.positions[v * 3 + 2],
                                          1.0f);
                    glm::mat4 skin(0.0f);
                    float total = 0.0f;
                    for (std::size_t k = 0; skinned && k < 4; ++k)
                    {
                        const float weight = mesh.weights[v * 4 + k];
                        const std::uint16_t joint = mesh.joints[v * 4 + k];
                        if (weight > 0.0f && joint < bones->size())
                        {
                            skin += weight * (*bones)[joint];
                            total += weight;
                        }
                    }
                    placed.emplace_back((total > 0.0f ? skin : where) * point);
                }
                visit(mesh, placed);
            };

            if (model.nodes.empty())
            {
                for (const MeshData& mesh : model.meshes)
                {
                    place(mesh, glm::mat4(1.0f), nullptr);
                }
                return;
            }

            // The roots, worked out from the children lists when the file gives none
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
            std::vector<glm::mat4> rest;
            rest.reserve(model.nodes.size());
            for (const ModelNode& node : model.nodes)
            {
                rest.push_back(node.localTransform);
            }
            const std::vector<glm::mat4> worlds = NodeWorlds(model.nodes, roots, rest);
            std::vector<std::vector<glm::mat4>> bones;
            for (const Skin& skin : model.skins)
            {
                bones.push_back(SkinMatrices(skin, worlds));
            }
            for (std::size_t n = 0; n < model.nodes.size(); ++n)
            {
                for (const std::uint32_t index : model.nodes[n].meshIndices)
                {
                    if (index >= model.meshes.size())
                    {
                        continue;
                    }
                    const MeshData& mesh = model.meshes[index];
                    const bool skinned = mesh.skin >= 0 && static_cast<std::size_t>(mesh.skin) < bones.size();
                    // A skinned mesh is drawn from the model's root, bent by its bones
                    place(mesh, skinned ? glm::mat4(1.0f) : worlds[n],
                          skinned ? &bones[static_cast<std::size_t>(mesh.skin)] : nullptr);
                }
            }
        }
    }

    std::optional<BoundingBox> RestBounds(const ModelData& model)
    {
        std::optional<BoundingBox> box;
        ForEachRestMesh(model,
                        [&box](const MeshData&, const std::vector<glm::vec3>& placed)
                        {
                            for (const glm::vec3& point : placed)
                            {
                                box = box ? BoundingBox{glm::min(box->min, point), glm::max(box->max, point)}
                                          : BoundingBox{point, point};
                            }
                        });
        return box;
    }

    std::vector<glm::vec3> RestTriangles(const ModelData& model)
    {
        std::vector<glm::vec3> triangles;
        ForEachRestMesh(model,
                        [&triangles](const MeshData& mesh, const std::vector<glm::vec3>& placed)
                        {
                            if (mesh.indices.empty())
                            {
                                // No index list: the vertices are the triangles, three by three
                                for (std::size_t v = 0; v + 2 < placed.size(); v += 3)
                                {
                                    triangles.insert(triangles.end(), {placed[v], placed[v + 1], placed[v + 2]});
                                }
                                return;
                            }
                            for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
                            {
                                const std::uint32_t a = mesh.indices[i];
                                const std::uint32_t b = mesh.indices[i + 1];
                                const std::uint32_t c = mesh.indices[i + 2];
                                if (a < placed.size() && b < placed.size() && c < placed.size())
                                {
                                    triangles.insert(triangles.end(), {placed[a], placed[b], placed[c]});
                                }
                            }
                        });
        return triangles;
    }

    AnimationClip Retarget(const AnimationClip& clip, const std::vector<ModelNode>& from,
                           const std::vector<ModelNode>& to)
    {
        std::unordered_map<std::string, std::uint32_t> byName;
        for (std::uint32_t i = 0; i < to.size(); ++i)
        {
            byName.emplace(to[i].name, i); // the first of a name
        }
        AnimationClip moved{.name = clip.name, .duration = clip.duration, .channels = {}};
        for (const AnimationChannel& channel : clip.channels)
        {
            if (channel.node >= from.size())
            {
                continue;
            }
            const auto found = byName.find(from[channel.node].name);
            if (found == byName.end())
            {
                continue;
            }
            AnimationChannel copy = channel;
            copy.node = found->second;
            moved.channels.push_back(std::move(copy));
        }
        return moved;
    }

    BoundingBox Turned(const BoundingBox& box, float degrees)
    {
        if (degrees == 0.0f)
        {
            return box;
        }
        const glm::mat4 turn = glm::rotate(glm::mat4(1.0f), glm::radians(degrees), glm::vec3(0.0f, 1.0f, 0.0f));
        std::optional<BoundingBox> turned;
        for (int corner = 0; corner < 8; ++corner)
        {
            const glm::vec3 point((corner & 1) ? box.max.x : box.min.x, (corner & 2) ? box.max.y : box.min.y,
                                  (corner & 4) ? box.max.z : box.min.z);
            const glm::vec3 moved(turn * glm::vec4(point, 1.0f));
            if (!turned)
            {
                turned = BoundingBox{moved, moved};
            }
            turned->min = glm::min(turned->min, moved);
            turned->max = glm::max(turned->max, moved);
        }
        return *turned;
    }

    std::vector<glm::mat4> SkinMatrices(const Skin& skin, const std::vector<glm::mat4>& worlds)
    {
        const std::size_t count = std::min(skin.joints.size(), kMaxBones);
        std::vector<glm::mat4> bones(count, glm::mat4(1.0f));
        for (std::size_t j = 0; j < count; ++j)
        {
            const std::uint32_t node = skin.joints[j];
            const glm::mat4 inverseBind = j < skin.inverseBind.size() ? skin.inverseBind[j] : glm::mat4(1.0f);
            bones[j] = (node < worlds.size() ? worlds[node] : glm::mat4(1.0f)) * inverseBind;
        }
        return bones;
    }
}
