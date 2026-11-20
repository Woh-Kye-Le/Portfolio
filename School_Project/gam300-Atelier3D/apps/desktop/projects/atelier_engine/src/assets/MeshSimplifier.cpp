/* !
@file               MeshSimplifier.cpp
@author             w.kyele@digipen.edu [100%]
@date               04/11/2026
@brief
    Implements the mesh simplifier: quadric error metrics with the collapse of
    an edge onto one of its ends, a priority queue of the cheapest collapses,
    border planes so open edges keep their place, locked seam vertices, and a
    check that no triangle turns over.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/assets/MeshSimplifier.hpp"

// Engine Assets Headers
#include "engine/assets/loaders/AtelierModel.hpp"

// Standard Library Headers
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <queue>
#include <sstream>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace Engine::Assets
{
    namespace
    {
        struct Vec3
        {
            double x = 0.0;
            double y = 0.0;
            double z = 0.0;
        };

        Vec3 Sub(const Vec3& a, const Vec3& b)
        {
            return {a.x - b.x, a.y - b.y, a.z - b.z};
        }

        Vec3 Cross(const Vec3& a, const Vec3& b)
        {
            return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
        }

        double Dot(const Vec3& a, const Vec3& b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        double Length(const Vec3& a)
        {
            return std::sqrt(Dot(a, a));
        }

        // The symmetric 4x4 matrix of a sum of squared distances to planes: aa ab ac ad bb bc bd cc cd dd
        using Quadric = std::array<double, 10>;

        void AddPlane(Quadric& q, const Vec3& n, double d, double weight)
        {
            const double a = n.x;
            const double b = n.y;
            const double c = n.z;
            const std::array<double, 10> terms{a * a, a * b, a * c, a * d, b * b, b * c, b * d, c * c, c * d, d * d};
            for (std::size_t i = 0; i < terms.size(); ++i)
            {
                q[i] += terms[i] * weight;
            }
        }

        // The summed squared distance from p to the quadric's planes
        double Error(const Quadric& q, const Vec3& p)
        {
            const double x = p.x;
            const double y = p.y;
            const double z = p.z;
            return q[0] * x * x + 2 * q[1] * x * y + 2 * q[2] * x * z + 2 * q[3] * x + q[4] * y * y + 2 * q[5] * y * z +
                   2 * q[6] * y + q[7] * z * z + 2 * q[8] * z + q[9];
        }

        struct Triangle
        {
            std::array<std::uint32_t, 3> v{};
            std::uint32_t run = 0; // which material run it is in
            bool alive = true;
        };

        // One possible collapse: vertex from goes onto vertex to. stampFrom says which version of from's neighbourhood
        // the cost was worked out for; a collapse whose vertex has changed since is stale.
        struct Candidate
        {
            double cost = 0.0;
            std::uint32_t from = 0;
            std::uint32_t to = 0;
            std::uint32_t stampFrom = 0;
            std::uint32_t stampTo = 0;

            bool operator>(const Candidate& other) const { return cost > other.cost; }
        };

        // How much more a border plane counts than a face plane: borders keep their shape
        constexpr double kBorderWeight = 100.0;
        // A collapse is refused when a triangle's facing would turn by more than this (cosine of about 78 degrees)
        constexpr double kMinFacingCosine = 0.2;
    }

    std::size_t TriangleCount(const MeshData& mesh)
    {
        return mesh.indices.size() / 3;
    }

    std::size_t TriangleCount(const ModelData& model, int level)
    {
        const std::vector<MeshData>& meshes = level <= 0 || static_cast<std::size_t>(level) > model.lods.size()
                                                  ? model.meshes
                                                  : model.lods[static_cast<std::size_t>(level) - 1];
        std::size_t count = 0;
        for (const MeshData& mesh : meshes)
        {
            count += TriangleCount(mesh);
        }
        return count;
    }

    MeshData SimplifyMesh(const MeshData& mesh, float ratio)
    {
        const std::size_t vertexCount = mesh.positions.size() / 3;
        const auto position = [&mesh](std::uint32_t v)
        {
            const std::size_t at = std::size_t{3} * v;
            return Vec3{mesh.positions[at], mesh.positions[at + 1], mesh.positions[at + 2]};
        };
        const bool hasUvs = mesh.uvs.size() >= vertexCount * 2;

        // Points: the vertices at one place (within a millionth of the mesh's size). Loaders often give each
        // triangle its own copies of its corners, so the shape is worked on point by point, and each point keeps
        // the vertices (its corners, with their own texture coordinates and normals) that are at it.
        std::vector<std::uint32_t> pointOf(vertexCount, 0);
        std::vector<std::vector<std::uint32_t>> cornersAt;
        {
            Vec3 low{1e300, 1e300, 1e300};
            Vec3 high{-1e300, -1e300, -1e300};
            for (std::uint32_t v = 0; v < vertexCount; ++v)
            {
                const Vec3 p = position(v);
                low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
                high = {std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z)};
            }
            const double cell = std::max(Length(Sub(high, low)) * 1e-6, 1e-12);
            std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> buckets; // hash -> points with it
            for (std::uint32_t v = 0; v < vertexCount; ++v)
            {
                const Vec3 p = position(v);
                const std::int64_t qx = std::llround(p.x / cell);
                const std::int64_t qy = std::llround(p.y / cell);
                const std::int64_t qz = std::llround(p.z / cell);
                const std::uint64_t hash =
                    std::uint64_t(qx) * 73856093ULL ^ std::uint64_t(qy) * 19349663ULL ^ std::uint64_t(qz) * 83492791ULL;
                std::vector<std::uint32_t>& bucket = buckets[hash];
                const auto same = std::find_if(bucket.begin(), bucket.end(),
                                               [&](std::uint32_t point)
                                               {
                                                   const Vec3 q = position(cornersAt[point].front());
                                                   return std::llround(q.x / cell) == qx &&
                                                          std::llround(q.y / cell) == qy &&
                                                          std::llround(q.z / cell) == qz;
                                               });
                if (same == bucket.end())
                {
                    bucket.push_back(static_cast<std::uint32_t>(cornersAt.size()));
                    pointOf[v] = static_cast<std::uint32_t>(cornersAt.size());
                    cornersAt.push_back({v});
                }
                else
                {
                    pointOf[v] = *same;
                    cornersAt[*same].push_back(v);
                }
            }
        }
        const std::size_t pointCount = cornersAt.size();
        const auto place = [&](std::uint32_t point) { return position(cornersAt[point].front()); };
        const auto uvOf = [&](std::uint32_t v)
        {
            const std::size_t at = std::size_t{2} * v;
            return hasUvs ? std::array<double, 2>{mesh.uvs[at], mesh.uvs[at + 1]} : std::array<double, 2>{};
        };

        // A point where the texture coordinates differ (a texture seam) stays: moving one side of a seam without the
        // other would tear the picture. Points whose corners differ only in their normal (a hard edge) can go.
        std::vector<bool> locked(pointCount, false);
        for (std::uint32_t point = 0; point < pointCount; ++point)
        {
            const auto first = uvOf(cornersAt[point].front());
            for (const std::uint32_t v : cornersAt[point])
            {
                const auto uv = uvOf(v);
                if (std::abs(uv[0] - first[0]) > 1e-5 || std::abs(uv[1] - first[1]) > 1e-5)
                {
                    locked[point] = true;
                    break;
                }
            }
        }

        // The material runs, and their triangles (as vertices, with the runs' base vertex applied)
        std::vector<SubMesh> runs = mesh.subMeshes;
        if (runs.empty())
        {
            runs.push_back(SubMesh{0, static_cast<std::uint32_t>(mesh.indices.size()), 0, 0});
        }
        std::vector<Triangle> triangles;
        for (std::uint32_t r = 0; r < runs.size(); ++r)
        {
            const SubMesh& run = runs[r];
            const std::size_t end =
                std::min<std::size_t>(std::size_t(run.baseIndex) + run.indexCount, mesh.indices.size());
            for (std::size_t i = run.baseIndex; i + 2 < end; i += 3)
            {
                Triangle t;
                bool valid = true;
                for (int k = 0; k < 3; ++k)
                {
                    const std::int64_t index = std::int64_t(mesh.indices[i + k]) + run.baseVertex;
                    valid = valid && index >= 0 && std::size_t(index) < vertexCount;
                    t.v[k] = valid ? static_cast<std::uint32_t>(index) : 0;
                }
                t.run = r;
                if (valid && pointOf[t.v[0]] != pointOf[t.v[1]] && pointOf[t.v[1]] != pointOf[t.v[2]] &&
                    pointOf[t.v[0]] != pointOf[t.v[2]])
                {
                    triangles.push_back(t);
                }
            }
        }

        // Each point's quadric: the planes of the triangles around it, weighted by their area, and for an edge on the
        // border a plane through the edge standing up from its triangle
        std::vector<Quadric> quadrics(pointCount, Quadric{});
        std::vector<std::vector<std::uint32_t>> around(pointCount);
        std::unordered_map<std::uint64_t, int> edgeUses;
        const auto edgeKey = [](std::uint32_t a, std::uint32_t b)
        { return (std::uint64_t(std::min(a, b)) << 32) | std::max(a, b); };
        for (std::uint32_t t = 0; t < triangles.size(); ++t)
        {
            for (int k = 0; k < 3; ++k)
            {
                around[pointOf[triangles[t].v[k]]].push_back(t);
                ++edgeUses[edgeKey(pointOf[triangles[t].v[k]], pointOf[triangles[t].v[(k + 1) % 3]])];
            }
        }
        for (const Triangle& t : triangles)
        {
            const std::array<std::uint32_t, 3> p{pointOf[t.v[0]], pointOf[t.v[1]], pointOf[t.v[2]]};
            const Vec3 p0 = place(p[0]);
            const Vec3 normal = Cross(Sub(place(p[1]), p0), Sub(place(p[2]), p0));
            const double twiceArea = Length(normal);
            if (twiceArea <= 0.0)
            {
                continue;
            }
            const Vec3 n{normal.x / twiceArea, normal.y / twiceArea, normal.z / twiceArea};
            const double d = -Dot(n, p0);
            for (const std::uint32_t point : p)
            {
                AddPlane(quadrics[point], n, d, twiceArea * 0.5);
            }
            for (int k = 0; k < 3; ++k)
            {
                const std::uint32_t a = p[k];
                const std::uint32_t b = p[(k + 1) % 3];
                if (edgeUses[edgeKey(a, b)] != 1)
                {
                    continue;
                }
                const Vec3 edge = Sub(place(b), place(a));
                const Vec3 side = Cross(edge, n);
                const double length = Length(side);
                if (length <= 0.0)
                {
                    continue;
                }
                const Vec3 sn{side.x / length, side.y / length, side.z / length};
                const double sd = -Dot(sn, place(a));
                AddPlane(quadrics[a], sn, sd, kBorderWeight * Dot(edge, edge));
                AddPlane(quadrics[b], sn, sd, kBorderWeight * Dot(edge, edge));
            }
        }

        std::vector<std::uint32_t> stamps(pointCount, 0);
        std::vector<bool> removed(pointCount, false);
        std::priority_queue<Candidate, std::vector<Candidate>, std::greater<>> queue;
        // A point's cheapest collapse: onto the neighbour where the two quadrics together cost least. One per point in
        // the queue (with the point's stamp), worked out again whenever its neighbourhood changes.
        const auto offer = [&](std::uint32_t point)
        {
            if (locked[point] || removed[point])
            {
                return;
            }
            double best = std::numeric_limits<double>::max();
            std::uint32_t bestTo = point;
            for (const std::uint32_t t : around[point])
            {
                if (!triangles[t].alive)
                {
                    continue;
                }
                for (const std::uint32_t v : triangles[t].v)
                {
                    const std::uint32_t other = pointOf[v];
                    if (other == point)
                    {
                        continue;
                    }
                    Quadric sum = quadrics[point];
                    for (std::size_t i = 0; i < sum.size(); ++i)
                    {
                        sum[i] += quadrics[other][i];
                    }
                    const double cost = Error(sum, place(other));
                    if (cost < best)
                    {
                        best = cost;
                        bestTo = other;
                    }
                }
            }
            if (bestTo != point)
            {
                queue.push({best, point, bestTo, stamps[point], 0});
            }
        };
        for (std::uint32_t point = 0; point < pointCount; ++point)
        {
            offer(point);
        }

        // The corner of point to that best stands in for a corner of the removed point: the nearest texture
        // coordinates (the same ones, away from seams)
        const auto standIn = [&](std::uint32_t to, std::uint32_t corner)
        {
            const auto want = uvOf(corner);
            std::uint32_t best = cornersAt[to].front();
            double bestDistance = std::numeric_limits<double>::max();
            for (const std::uint32_t v : cornersAt[to])
            {
                const auto uv = uvOf(v);
                const double distance = (uv[0] - want[0]) * (uv[0] - want[0]) + (uv[1] - want[1]) * (uv[1] - want[1]);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = v;
                }
            }
            return best;
        };

        const std::size_t target =
            static_cast<std::size_t>(std::max(0.0f, std::min(1.0f, ratio)) * static_cast<float>(triangles.size()));
        std::size_t alive = triangles.size();
        std::vector<std::uint32_t> neighbours;
        while (alive > target && !queue.empty())
        {
            const Candidate c = queue.top();
            queue.pop();
            if (removed[c.from] || removed[c.to] || stamps[c.from] != c.stampFrom)
            {
                continue; // stale: its neighbourhood has changed since
            }
            const auto touches = [&](const Triangle& tri, std::uint32_t point)
            { return pointOf[tri.v[0]] == point || pointOf[tri.v[1]] == point || pointOf[tri.v[2]] == point; };

            // Refused if a triangle that stays would turn over, or shrink to nothing; the point is offered again
            // when its neighbourhood next changes
            bool flips = false;
            for (const std::uint32_t t : around[c.from])
            {
                const Triangle& tri = triangles[t];
                if (!tri.alive || touches(tri, c.to))
                {
                    continue;
                }
                std::array<Vec3, 3> before{position(tri.v[0]), position(tri.v[1]), position(tri.v[2])};
                std::array<Vec3, 3> after = before;
                for (int k = 0; k < 3; ++k)
                {
                    if (pointOf[tri.v[k]] == c.from)
                    {
                        after[k] = place(c.to);
                    }
                }
                const Vec3 n0 = Cross(Sub(before[1], before[0]), Sub(before[2], before[0]));
                const Vec3 n1 = Cross(Sub(after[1], after[0]), Sub(after[2], after[0]));
                const double l0 = Length(n0);
                const double l1 = Length(n1);
                if (l1 <= 1e-12 || (l0 > 0.0 && Dot(n0, n1) < kMinFacingCosine * l0 * l1))
                {
                    flips = true;
                    break;
                }
            }
            if (flips)
            {
                continue;
            }

            // Collapse: triangles on the edge go; the others' corners at from move to a corner of to
            for (const std::uint32_t t : around[c.from])
            {
                Triangle& tri = triangles[t];
                if (!tri.alive)
                {
                    continue;
                }
                if (touches(tri, c.to))
                {
                    tri.alive = false;
                    --alive;
                    continue;
                }
                for (std::uint32_t& v : tri.v)
                {
                    v = pointOf[v] == c.from ? standIn(c.to, v) : v;
                }
                around[c.to].push_back(t);
            }
            for (std::size_t i = 0; i < quadrics[c.to].size(); ++i)
            {
                quadrics[c.to][i] += quadrics[c.from][i];
            }
            removed[c.from] = true;
            around[c.from].clear();

            // The kept point's list is pruned of dead triangles, and it and its neighbours are offered again
            auto& list = around[c.to];
            std::erase_if(list, [&triangles](std::uint32_t t) { return !triangles[t].alive; });
            std::sort(list.begin(), list.end());
            list.erase(std::unique(list.begin(), list.end()), list.end());
            neighbours.clear();
            for (const std::uint32_t t : list)
            {
                for (const std::uint32_t v : triangles[t].v)
                {
                    neighbours.push_back(pointOf[v]);
                }
            }
            std::sort(neighbours.begin(), neighbours.end());
            neighbours.erase(std::unique(neighbours.begin(), neighbours.end()), neighbours.end());
            for (const std::uint32_t point : neighbours)
            {
                ++stamps[point];
                offer(point);
            }
        }

        // The result: the triangles left, run by run, with only the vertices they use
        MeshData result;
        result.name = mesh.name;
        result.skin = mesh.skin;
        std::vector<std::int64_t> newIndex(vertexCount, -1);
        const bool hasNormals = mesh.normals.size() >= vertexCount * 3;
        const bool hasSkin = mesh.joints.size() >= vertexCount * 4 && mesh.weights.size() >= vertexCount * 4;
        const auto use = [&](std::uint32_t v)
        {
            if (newIndex[v] < 0)
            {
                newIndex[v] = static_cast<std::int64_t>(result.positions.size() / 3);
                const auto index = static_cast<std::ptrdiff_t>(v);
                result.positions.insert(result.positions.end(), mesh.positions.begin() + index * 3,
                                        mesh.positions.begin() + index * 3 + 3);
                if (hasUvs)
                {
                    result.uvs.insert(result.uvs.end(), mesh.uvs.begin() + index * 2, mesh.uvs.begin() + index * 2 + 2);
                }
                if (hasNormals)
                {
                    result.normals.insert(result.normals.end(), mesh.normals.begin() + index * 3,
                                          mesh.normals.begin() + index * 3 + 3);
                }
                if (hasSkin)
                {
                    result.joints.insert(result.joints.end(), mesh.joints.begin() + index * 4,
                                         mesh.joints.begin() + index * 4 + 4);
                    result.weights.insert(result.weights.end(), mesh.weights.begin() + index * 4,
                                          mesh.weights.begin() + index * 4 + 4);
                }
            }
            return static_cast<std::uint32_t>(newIndex[v]);
        };
        for (std::uint32_t r = 0; r < runs.size(); ++r)
        {
            const auto first = static_cast<std::uint32_t>(result.indices.size());
            for (const Triangle& t : triangles)
            {
                if (t.alive && t.run == r)
                {
                    for (const std::uint32_t v : t.v)
                    {
                        result.indices.push_back(use(v));
                    }
                }
            }
            const auto count = static_cast<std::uint32_t>(result.indices.size()) - first;
            if (count > 0)
            {
                result.subMeshes.push_back(SubMesh{first, count, 0, runs[r].materialIndex});
            }
        }
        return result;
    }

    void GenerateLods(ModelData& model)
    {
        model.lods.clear();
        std::size_t previous = TriangleCount(model);
        if (previous < kLodMinimumTriangles)
        {
            return;
        }
        float kept = 1.0f; // the share of the full model the level before kept
        for (const float ratio : kLodTriangleRatios)
        {
            // Each level is made from the one before (quicker than from the full model every time)
            const std::vector<MeshData>& from = model.lods.empty() ? model.meshes : model.lods.back();
            std::vector<MeshData> level;
            level.reserve(from.size());
            for (const MeshData& mesh : from)
            {
                // Small parts (an eye, a button) are kept as they are
                level.push_back(TriangleCount(mesh) < 64 ? mesh : SimplifyMesh(mesh, ratio / kept));
                level.back().rendererCacheId = 0;
            }
            kept = ratio;
            std::size_t count = 0;
            for (const MeshData& mesh : level)
            {
                count += TriangleCount(mesh);
            }
            if (count * 10 > previous * 9)
            {
                return; // not a tenth fewer than the level before: not worth another version
            }
            model.lods.push_back(std::move(level));
            previous = count;
        }
    }

    namespace
    {
        // Bump when the simplifier changes what it makes, so older cached versions are made again
        constexpr int kLodCacheVersion = 1;

        // What a cached file must have been made from: the model file (where, how big, when saved) and how
        std::string CacheKey(const std::filesystem::path& source)
        {
            std::error_code ec;
            const auto full = std::filesystem::absolute(source, ec).lexically_normal();
            const auto size = std::filesystem::file_size(source, ec);
            const auto written =
                static_cast<long long>(std::filesystem::last_write_time(source, ec).time_since_epoch().count());
            std::ostringstream key;
            key << "atelier-lod " << kLodCacheVersion << " " << kLodTriangleRatios[0] << " " << kLodTriangleRatios[1]
                << " " << kLodMinimumTriangles << " " << size << " " << written << " " << full.generic_string();
            return key.str();
        }

        std::filesystem::path CacheFile(const std::string& key)
        {
            std::error_code ec;
            const auto temp = std::filesystem::temp_directory_path(ec);
            std::ostringstream name;
            name << std::hex << std::hash<std::string>{}(key) << ".lod";
            return ec ? std::filesystem::path{} : temp / "atelier_lod_cache" / name.str();
        }

        // The file: the key on its own line, then each level as its byte count and a .amodel of its meshes
        bool ReadCache(const std::filesystem::path& file, const std::string& key, ModelData& model)
        {
            std::ifstream in(file, std::ios::binary);
            std::string header;
            if (!in || !std::getline(in, header) || header != key)
            {
                return false;
            }
            std::uint32_t levels = 0;
            in.read(reinterpret_cast<char*>(&levels), sizeof levels);
            std::vector<std::vector<MeshData>> lods;
            for (std::uint32_t i = 0; in && i < levels && i < kLodTriangleRatios.size(); ++i)
            {
                std::uint64_t length = 0;
                in.read(reinterpret_cast<char*>(&length), sizeof length);
                if (!in || length > (std::uint64_t(1) << 32))
                {
                    return false;
                }
                std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
                in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length));
                auto level = in ? ParseAtelierModel(bytes) : std::nullopt;
                if (!level || level->meshes.size() != model.meshes.size())
                {
                    return false;
                }
                lods.push_back(std::move(level->meshes));
            }
            if (!in || lods.size() != levels)
            {
                return false;
            }
            model.lods = std::move(lods);
            return true;
        }

        void WriteCache(const std::filesystem::path& file, const std::string& key, const ModelData& model)
        {
            std::error_code ec;
            std::filesystem::create_directories(file.parent_path(), ec);
            const auto partial = std::filesystem::path(file).concat(".part");
            {
                std::ofstream out(partial, std::ios::binary | std::ios::trunc);
                if (!out)
                {
                    return;
                }
                out << key << '\n';
                const auto levels = static_cast<std::uint32_t>(model.lods.size());
                out.write(reinterpret_cast<const char*>(&levels), sizeof levels);
                for (const std::vector<MeshData>& level : model.lods)
                {
                    // The same model with the level's meshes, so its material, skin and node links check out
                    ModelData copy;
                    copy.meshes = level;
                    copy.materials = model.materials;
                    copy.nodes = model.nodes;
                    copy.roots = model.roots;
                    copy.skins = model.skins;
                    copy.name = model.name;
                    const std::vector<std::uint8_t> bytes = WriteAtelierModel(copy);
                    const auto length = static_cast<std::uint64_t>(bytes.size());
                    out.write(reinterpret_cast<const char*>(&length), sizeof length);
                    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                }
                if (!out)
                {
                    return;
                }
            }
            std::filesystem::rename(partial, file, ec); // whole or not at all, if two editors write at once
        }
    }

    void LoadOrGenerateLods(ModelData& model, const std::filesystem::path& source)
    {
        if (TriangleCount(model) < kLodMinimumTriangles)
        {
            model.lods.clear();
            return;
        }
        const std::string key = CacheKey(source);
        const std::filesystem::path file = CacheFile(key);
        if (!file.empty() && ReadCache(file, key, model))
        {
            return;
        }
        GenerateLods(model);
        if (!file.empty())
        {
            WriteCache(file, key, model);
        }
    }
}
