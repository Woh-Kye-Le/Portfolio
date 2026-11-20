/* !
@file               AssetConverter.cpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Implements the model converter: imports with assimp, maps the aiScene onto
    the engine's ModelData (meshes, material slots, node hierarchy), copies the
    textures beside the result and writes the .amodel file. A folder or
    .zip given as input is searched for its model first.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "tool/AssetConverter.hpp"
#include "tool/ZipReader.hpp"

// Engine Assets Headers
#include "engine/assets/data/ModelData.hpp"
#include "engine/assets/loaders/AtelierModel.hpp"

// Third-Party Headers
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// Standard Library Headers
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>

namespace Atelier::Tool
{
    namespace
    {
        namespace fs = std::filesystem;
        using Engine::Assets::MaterialData;
        using Engine::Assets::MeshData;
        using Engine::Assets::ModelData;
        using Engine::Assets::ModelNode;
        using Engine::Assets::SubMesh;

        // assimp matrices are row-major with the translation in the last column; glm's are column-major
        glm::mat4 ToGlm(const aiMatrix4x4& m)
        {
            return glm::mat4(m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3, m.c3, m.d3, m.a4, m.b4, m.c4,
                             m.d4);
        }

        std::string ToLower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        // assimp wants UTF-8 paths (it converts them itself on Windows); path::string() would be the ANSI code page
        std::string Utf8(const fs::path& path)
        {
            const auto text = path.u8string();
            return std::string(text.begin(), text.end());
        }

        fs::path FromUtf8(const std::string& text)
        {
            return fs::path(std::u8string(text.begin(), text.end()));
        }

        std::string Forward(std::string text)
        {
            std::replace(text.begin(), text.end(), '\\', '/');
            return text;
        }

        std::optional<std::vector<char>> ReadAll(const fs::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                return std::nullopt;
            }
            return std::vector<char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }

        bool WriteAll(const fs::path& path, const char* data, std::size_t size)
        {
            std::ofstream file(path, std::ios::binary);
            file.write(data, static_cast<std::streamsize>(size));
            return static_cast<bool>(file);
        }

        // Writes bytes as <leaf> in dir (or the first free <leaf>_N), unless an identical file is already there.
        // Returns the file that holds them (empty when it could not be written); created says whether it was
        // written now.
        fs::path PlaceBytes(const fs::path& dir, const std::string& leaf, const std::vector<char>& bytes, bool& created)
        {
            created = false;
            std::error_code ec;
            fs::create_directories(dir, ec);
            const fs::path wanted = FromUtf8(leaf);
            fs::path target = dir / wanted;
            for (int n = 2; fs::exists(target, ec); ++n)
            {
                if (ReadAll(target) == std::optional<std::vector<char>>(bytes))
                {
                    return target;
                }
                target = dir / FromUtf8(Utf8(wanted.stem()) + "_" + std::to_string(n) + Utf8(wanted.extension()));
            }
            if (!WriteAll(target, bytes.data(), bytes.size()))
            {
                return {};
            }
            created = true;
            return target;
        }

        // Copies the textures of one conversion next to the model and answers with the path to store for each one
        class TextureCollector
        {
        public:
            TextureCollector(const aiScene& scene, const fs::path& inputDir, const fs::path& modelDir,
                             const fs::path& texturesDir, const std::string& stem, ConvertResult& result)
                : scene_(scene), inputDir_(inputDir), modelDir_(modelDir), texturesDir_(texturesDir), stem_(stem),
                  result_(result)
            {
            }

            /*!***********************************************************************
            @brief          Finds the texture a material slot names, brings it next to the model and returns the
                            path to store, relative to the model's folder.
            @param          name          The path the file gave, or "*N" for texture N embedded in the file.
            @return         Path relative to the model with forward slashes; empty when the slot has no texture;
                            the name the file gave (and a warning) when the image cannot be found.
            *************************************************************************/
            std::string Bring(const std::string& name)
            {
                if (name.empty())
                {
                    return {};
                }
                const auto known = brought_.find(name);
                if (known != brought_.end())
                {
                    return known->second;
                }
                std::string stored = BringUncached(name);
                brought_.emplace(name, stored);
                return stored;
            }

        private:
            std::string BringUncached(const std::string& name)
            {
                if (const aiTexture* embedded = scene_.GetEmbeddedTexture(name.c_str()))
                {
                    return WriteEmbedded(*embedded, name);
                }

                const std::string path = Forward(name);
                const std::string leaf = path.substr(path.find_last_of('/') + 1);
                std::vector<fs::path> candidates;
                const fs::path asNamed = FromUtf8(path);
                candidates.push_back(asNamed.is_absolute() ? asNamed : inputDir_ / asNamed);
                if (!leaf.empty())
                {
                    const fs::path leafPath = FromUtf8(leaf);
                    candidates.push_back(inputDir_ / leafPath);
                    candidates.push_back(inputDir_ / "textures" / leafPath);
                    candidates.push_back(inputDir_ / "Textures" / leafPath);
                    candidates.push_back(inputDir_ / ".." / "textures" / leafPath);
                    // A model imported again from a game: art/models/<name>/ has its images in art/textures/<name>/
                    candidates.push_back(inputDir_ / ".." / ".." / "textures" / inputDir_.filename() / leafPath);
                    candidates.push_back(inputDir_ / FromUtf8(stem_ + ".fbm") /
                                         leafPath); // where FBX tools unpack media
                }
                for (const fs::path& candidate : candidates)
                {
                    std::error_code ec;
                    if (fs::is_regular_file(candidate, ec))
                    {
                        return Copy(candidate, leaf);
                    }
                }
                result_.warnings.push_back("texture not found: " + name);
                return path;
            }

            // Copies a texture file into the textures folder; the same file twice is one copy, a different file with
            // the same name gets a number
            std::string Copy(const fs::path& source, const std::string& leaf)
            {
                const auto bytes = ReadAll(source);
                if (!bytes)
                {
                    result_.warnings.push_back("texture could not be read: " + source.string());
                    return Forward(source.string());
                }
                return Place(leaf, *bytes);
            }

            // Writes bytes as <leaf> in the textures folder (or the first free <leaf>_N), unless an identical file is
            // already there; returns the path to store
            std::string Place(const std::string& leaf, const std::vector<char>& bytes)
            {
                bool created = false;
                const fs::path target = PlaceBytes(texturesDir_, leaf, bytes, created);
                if (target.empty())
                {
                    result_.warnings.push_back("texture could not be written: " +
                                               (texturesDir_ / FromUtf8(leaf)).string());
                    return Forward(leaf);
                }
                if (created)
                {
                    result_.textures.push_back(target);
                }
                return Stored(target);
            }

            std::string Stored(const fs::path& target) const
            {
                const fs::path relative = target.lexically_normal().lexically_relative(modelDir_.lexically_normal());
                const auto text = (relative.empty() ? target : relative).generic_u8string();
                return std::string(text.begin(), text.end());
            }

            // An image packed inside the model file: compressed ones are written as they are, raw pixels as a TGA
            std::string WriteEmbedded(const aiTexture& texture, const std::string& name)
            {
                const std::string base = stem_ + "_tex" + (name.size() > 1 ? name.substr(1) : std::string{"0"});
                if (texture.mHeight == 0)
                {
                    std::string extension = ToLower(texture.achFormatHint);
                    extension = extension.empty() ? "png" : (extension == "jpeg" ? "jpg" : extension);
                    const auto* data = reinterpret_cast<const char*>(texture.pcData);
                    return Place(base + "." + extension, std::vector<char>(data, data + texture.mWidth));
                }

                // 32-bit uncompressed TGA, bottom-left origin flag clear = top-down rows as stored
                std::vector<char> tga(18, 0);
                tga[2] = 2;
                tga[12] = static_cast<char>(texture.mWidth & 0xFF);
                tga[13] = static_cast<char>((texture.mWidth >> 8) & 0xFF);
                tga[14] = static_cast<char>(texture.mHeight & 0xFF);
                tga[15] = static_cast<char>((texture.mHeight >> 8) & 0xFF);
                tga[16] = 32;
                tga[17] = 0x28; // 8 alpha bits, rows from the top
                for (unsigned i = 0; i < texture.mWidth * texture.mHeight; ++i)
                {
                    const aiTexel& texel = texture.pcData[i];
                    tga.insert(tga.end(), {static_cast<char>(texel.b), static_cast<char>(texel.g),
                                           static_cast<char>(texel.r), static_cast<char>(texel.a)});
                }
                return Place(base + ".tga", tga);
            }

            const aiScene& scene_;
            fs::path inputDir_;
            fs::path modelDir_;
            fs::path texturesDir_;
            std::string stem_;
            ConvertResult& result_;
            std::map<std::string, std::string> brought_;
        };

        // The path of the first texture of a type, or empty
        std::string TexturePath(const aiMaterial& material, aiTextureType type)
        {
            aiString path;
            if (material.GetTextureCount(type) > 0 && material.GetTexture(type, 0, &path) == AI_SUCCESS)
            {
                return path.C_Str();
            }
            return {};
        }

        /*!***********************************************************************
        @brief          Maps one assimp material onto a material slot of the engine.
        @param          source        assimp material.
        @param          textures      Collector that brings the texture files next to the model.
        @return         The slot: colour and opacity, emissive, metallic and roughness, three texture paths.
        *************************************************************************/
        MaterialData ToMaterial(const aiMaterial& source, TextureCollector& textures)
        {
            MaterialData material;
            aiString name;
            if (source.Get(AI_MATKEY_NAME, name) == AI_SUCCESS)
            {
                material.name = name.C_Str();
            }

            aiColor4D base;
            aiColor3D diffuse(1.0f, 1.0f, 1.0f);
            if (source.Get(AI_MATKEY_BASE_COLOR, base) == AI_SUCCESS)
            {
                material.baseColorFactor = glm::vec4(base.r, base.g, base.b, base.a);
            }
            else if (source.Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS)
            {
                material.baseColorFactor = glm::vec4(diffuse.r, diffuse.g, diffuse.b, 1.0f);
            }
            float opacity = 1.0f;
            if (source.Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS)
            {
                material.baseColorFactor.a = std::clamp(opacity, 0.0f, 1.0f);
            }

            aiColor3D emissive(0.0f, 0.0f, 0.0f);
            if (source.Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS)
            {
                material.emissiveFactor = glm::vec3(emissive.r, emissive.g, emissive.b);
                const bool glows = emissive.r > 0.0f || emissive.g > 0.0f || emissive.b > 0.0f;
                material.emissiveStrength = glows ? 1.0f : 0.0f;
            }

            float metallic = 0.0f;
            float roughness = 0.5f;
            float shininess = 0.0f;
            if (source.Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS)
            {
                material.metallic = std::clamp(metallic, 0.0f, 1.0f);
            }
            if (source.Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS)
            {
                material.roughness = std::clamp(roughness, 0.0f, 1.0f);
            }
            else if (source.Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS && shininess > 0.0f)
            {
                // Phong shininess to roughness: the exponent of a Blinn-Phong lobe is 2 / roughness^2 - 2
                material.roughness = std::clamp(std::sqrt(2.0f / (shininess + 2.0f)), 0.0f, 1.0f);
            }

            std::string albedo = TexturePath(source, aiTextureType_BASE_COLOR);
            if (albedo.empty())
            {
                albedo = TexturePath(source, aiTextureType_DIFFUSE);
            }
            std::string normal = TexturePath(source, aiTextureType_NORMALS);
            if (normal.empty())
            {
                normal = TexturePath(source, aiTextureType_HEIGHT); // OBJ and FBX bump maps arrive as height maps
            }
            material.baseColorTexture = textures.Bring(albedo);
            material.normalTexture = textures.Bring(normal);
            // glTF packs metallic (B) and roughness (G) in one image, which assimp reports as the "unknown" type
            std::string metallicRoughness = TexturePath(source, aiTextureType_UNKNOWN);
            if (metallicRoughness.empty())
            {
                metallicRoughness = TexturePath(source, aiTextureType_METALNESS);
            }
            material.metallicRoughnessTexture = textures.Bring(metallicRoughness);
            return material;
        }

        bool IsImage(const fs::path& file)
        {
            const std::string extension = ToLower(file.extension().string());
            return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" ||
                   extension == ".tga";
        }

        // Image files beside a model and in a textures folder next to it (names only, sorted)
        std::vector<std::string> ImagesAround(const fs::path& folder)
        {
            std::set<std::string> names;
            for (const fs::path& dir : {folder, folder / "textures", folder / "Textures"})
            {
                std::error_code ec;
                for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
                {
                    if (it->is_regular_file(ec) && IsImage(it->path()))
                    {
                        names.insert(Utf8(it->path().filename()));
                    }
                }
            }
            return {names.begin(), names.end()};
        }

        // Where a format comes in kModelExtensions (lower is better), or none when the tool does not read it
        std::optional<std::size_t> ModelRank(const fs::path& file)
        {
            const std::string extension = ToLower(file.extension().string());
            const auto found = std::find(kModelExtensions.begin(), kModelExtensions.end(), extension);
            if (found == kModelExtensions.end())
            {
                return std::nullopt;
            }
            return static_cast<std::size_t>(found - kModelExtensions.begin());
        }

        /*!***********************************************************************
        @brief          Finds the model in a folder (a model pack): every model file in it and its subfolders is
                        looked at; they must all be one model (the same name, perhaps in several formats), and the
                        best format of it is picked.
        @param          folder        The folder to search.
        @param          error         Why no model was picked, when none is.
        @return         The model file, or nothing.
        *************************************************************************/
        std::optional<fs::path> FindModel(const fs::path& folder, std::string& error)
        {
            constexpr int kDeepest = 4; // packs keep models a folder or two down ("fbx/", "Models/OBJ/")
            std::vector<fs::path> models;
            std::error_code ec;
            for (fs::recursive_directory_iterator it(folder, fs::directory_options::skip_permission_denied, ec), end;
                 !ec && it != end; it.increment(ec))
            {
                if (it->is_directory(ec))
                {
                    if (it.depth() >= kDeepest || Utf8(it->path().filename()) == "__MACOSX")
                    {
                        it.disable_recursion_pending();
                    }
                }
                else if (it->is_regular_file(ec) && ModelRank(it->path()))
                {
                    models.push_back(it->path());
                }
            }
            if (models.empty())
            {
                error = "no model file (.glb, .gltf, .fbx, .dae, .obj, .3ds, .ply, .stl) found";
                return std::nullopt;
            }

            std::set<std::string> names;
            for (const fs::path& model : models)
            {
                names.insert(ToLower(Utf8(model.stem())));
            }
            std::sort(models.begin(), models.end(),
                      [](const fs::path& a, const fs::path& b)
                      {
                          const std::size_t ra = *ModelRank(a);
                          const std::size_t rb = *ModelRank(b);
                          return ra != rb ? ra < rb : a < b;
                      });
            if (names.size() > 1)
            {
                std::string list;
                for (std::size_t i = 0; i < models.size() && i < 4; ++i)
                {
                    list += (i > 0 ? ", " : "") + Utf8(models[i].filename());
                }
                error = "more than one model (" + list + (models.size() > 4 ? ", ..." : "") +
                        "): import the one you want on its own";
                return std::nullopt;
            }
            return models.front();
        }

        // "a, b, c, ..." for up to four names
        std::string NameList(const std::vector<std::string>& names)
        {
            std::string list;
            for (std::size_t i = 0; i < names.size() && i < 4; ++i)
            {
                list += (i > 0 ? ", " : "") + names[i];
            }
            return list + (names.size() > 4 ? ", ..." : "");
        }

        std::string Trim(const std::string& text)
        {
            const auto first = text.find_first_not_of(" \t\r\n");
            return first == std::string::npos ? std::string{}
                                              : text.substr(first, text.find_last_not_of(" \t\r\n") + 1 - first);
        }

        // glTF writes spaces in file names as %20
        std::string PercentDecode(const std::string& text)
        {
            std::string decoded;
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const auto hex = [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; };
                if (text[i] == '%' && i + 2 < text.size() && hex(text[i + 1]) && hex(text[i + 2]))
                {
                    decoded += static_cast<char>(std::stoi(text.substr(i + 1, 2), nullptr, 16));
                    i += 2;
                }
                else
                {
                    decoded += text[i];
                }
            }
            return decoded;
        }

        // The files a model file points to that are not images: an OBJ's material libraries (mtllib) and a glTF's
        // buffers. Only the ones that exist beside the model are returned.
        std::vector<fs::path> CompanionFiles(const fs::path& model)
        {
            std::vector<fs::path> found;
            const auto add = [&](const std::string& name)
            {
                const std::string clean = Forward(Trim(name));
                if (clean.empty() || clean.rfind("data:", 0) == 0)
                {
                    return;
                }
                const fs::path path = model.parent_path() / FromUtf8(clean);
                std::error_code ec;
                if (!IsImage(path) && fs::is_regular_file(path, ec) &&
                    std::find(found.begin(), found.end(), path) == found.end())
                {
                    found.push_back(path);
                }
            };

            const std::string extension = ToLower(model.extension().string());
            if (extension == ".obj")
            {
                std::ifstream obj(model);
                for (std::string line; std::getline(obj, line);)
                {
                    if (line.rfind("mtllib", 0) != 0)
                    {
                        continue;
                    }
                    const std::string names = Trim(line.substr(6));
                    const std::size_t before = found.size();
                    add(names); // one file name, spaces and all
                    if (found.size() == before)
                    {
                        std::istringstream words(names); // or several
                        for (std::string word; words >> word;)
                        {
                            add(word);
                        }
                    }
                }
            }
            else if (extension == ".gltf")
            {
                const auto bytes = ReadAll(model);
                const std::string text = bytes ? std::string(bytes->begin(), bytes->end()) : std::string{};
                for (std::size_t at = text.find("\"uri\""); at != std::string::npos; at = text.find("\"uri\"", at + 5))
                {
                    std::size_t open = text.find(':', at + 5);
                    open = open == std::string::npos ? open : text.find_first_not_of(" \t\r\n", open + 1);
                    if (open == std::string::npos || text[open] != '"')
                    {
                        continue;
                    }
                    const std::size_t close = text.find('"', open + 1);
                    if (close == std::string::npos)
                    {
                        break;
                    }
                    add(PercentDecode(text.substr(open + 1, close - open - 1)));
                }
            }
            return found;
        }

        // Every image in a model pack, a few folders down (hidden files and the __MACOSX leftovers of a Mac's zip are
        // skipped), except previews and thumbnails: nothing can use those
        std::vector<fs::path> ImagesInPack(const fs::path& root)
        {
            constexpr int kDeepest = 4;
            std::vector<fs::path> images;
            std::error_code ec;
            for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
                 !ec && it != end; it.increment(ec))
            {
                const std::string name = Utf8(it->path().filename());
                const bool hidden = !name.empty() && name.front() == '.';
                if (it->is_directory(ec))
                {
                    if (it.depth() >= kDeepest || name == "__MACOSX" || hidden)
                    {
                        it.disable_recursion_pending();
                    }
                }
                else if (it->is_regular_file(ec) && !hidden && IsImage(it->path()))
                {
                    const std::string lower = ToLower(name);
                    if (lower.find("preview") == std::string::npos && lower.find("thumb") == std::string::npos)
                    {
                        images.push_back(it->path());
                    }
                }
            }
            std::sort(images.begin(), images.end());
            return images;
        }

        /*!***********************************************************************
        @brief          The rule for what a model's folder holds besides its .amodel, by the format it came in:
                        - .obj: the .obj, the .mtl files it names, the images they name, and the pack's other images
                          (an OBJ often comes without its .mtl, and then its images are still needed)
                        - .fbx (and .dae, .3ds, .ply, .stl): the file, its images (found beside it or written out of
                          it), and the pack's other images
                        - .gltf: the .gltf, the .bin buffers and the images it names, nothing else: it lists every file
                          it uses
                        - .glb: the .glb and the images written out of it (the engine reads image files), nothing else:
                          everything it uses is inside it
        @param          model         The model file that was converted.
        @return         Whether the pack's images that the model does not name are kept too.
        *************************************************************************/
        bool KeepsPackImages(const fs::path& model)
        {
            const std::string extension = ToLower(model.extension().string());
            return extension != ".glb" && extension != ".gltf";
        }

        /*!***********************************************************************
        @brief          Picks the folder a model is stored in: <root>/<name>, unless a different model already has
                        that name, then <root>/<name>_2 and so on. A folder is the same model when the original file
                        kept in it is identical to this one (importing it again updates the folder); a folder with
                        no original of this name to compare is taken as the same model too.
        @param          root          The game's models folder.
        @param          name          The model's name (its file name without the extension).
        @param          model         The model file being imported.
        @return         The folder to use, which may not exist yet.
        *************************************************************************/
        fs::path PickModelFolder(const fs::path& root, const std::string& name, const fs::path& model)
        {
            for (int n = 1;; ++n)
            {
                const fs::path folder = root / FromUtf8(n == 1 ? name : name + "_" + std::to_string(n));
                std::error_code ec;
                if (!fs::exists(folder, ec))
                {
                    return folder;
                }
                const fs::path kept = folder / model.filename();
                if (!fs::is_regular_file(kept, ec))
                {
                    return folder;
                }
                const auto keptSize = fs::file_size(kept, ec);
                if (!ec && keptSize == fs::file_size(model, ec) && !ec && ReadAll(kept) == ReadAll(model))
                {
                    return folder;
                }
            }
        }

        /*!***********************************************************************
        @brief          Stores what a model was made from beside its .amodel, so it can be imported again later: the
                        file itself, the files it points to (an OBJ's .mtl, a glTF's .bin) and, for a pack, the
                        pack's other images (an OBJ that came without its .mtl still brings its pictures, to be used
                        as the object's Texture or named by a .mtl written later). Files already there unchanged
                        are left alone; the originals replace older ones, as an import of the same model is an update.
        @param          model         The model file that was converted.
        @param          packageRoot   The unpacked folder or zip it came from; empty for a model file given on its own,
                                      whose neighbours may have nothing to do with it.
        @param          modelDir      The model's folder, which gets the original files.
        @param          imagesDir     Where the model's images go (its folder under textures/), which gets the pack's
                                      images.
        @param          result        Gets the stored files in kept, and a warning for each one that failed.
        *************************************************************************/
        void KeepSources(const fs::path& model, const fs::path& packageRoot, const fs::path& modelDir,
                         const fs::path& imagesDir, ConvertResult& result)
        {
            std::error_code ec;
            fs::create_directories(modelDir, ec);
            std::vector<fs::path> originals{model};
            for (fs::path& companion : CompanionFiles(model))
            {
                originals.push_back(std::move(companion));
            }
            for (const fs::path& file : originals)
            {
                const fs::path target = modelDir / file.filename();
                if (fs::exists(target, ec) && fs::equivalent(file, target, ec))
                {
                    continue; // imported from the game's own folder
                }
                fs::copy_file(file, target, fs::copy_options::overwrite_existing, ec);
                if (ec)
                {
                    result.warnings.push_back("could not keep " + Utf8(file.filename()) + ": " + ec.message());
                }
                else
                {
                    result.kept.push_back(target);
                }
            }
            if (packageRoot.empty() || !KeepsPackImages(model))
            {
                return;
            }
            for (const fs::path& image : ImagesInPack(packageRoot))
            {
                const auto bytes = ReadAll(image);
                bool created = false;
                const fs::path target =
                    bytes ? PlaceBytes(imagesDir, Utf8(image.filename()), *bytes, created) : fs::path{};
                if (target.empty())
                {
                    result.warnings.push_back("could not keep " + Utf8(image.filename()));
                }
                else if (created)
                {
                    result.kept.push_back(target);
                }
            }
        }

        // A fresh folder under the system temp folder, removed with everything in it when this goes away
        class TempFolder
        {
        public:
            TempFolder() = default;
            TempFolder(const TempFolder&) = delete;
            TempFolder& operator=(const TempFolder&) = delete;
            ~TempFolder()
            {
                if (!path_.empty())
                {
                    std::error_code ec;
                    fs::remove_all(path_, ec);
                }
            }

            bool Create()
            {
                std::error_code ec;
                const fs::path base = fs::temp_directory_path(ec);
                if (ec)
                {
                    return false;
                }
                std::mt19937_64 random(
                    std::random_device{}() ^
                    static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
                for (int attempt = 0; attempt < 8; ++attempt)
                {
                    const fs::path candidate = base / ("atelier_import_" + std::to_string(random()));
                    if (fs::create_directories(candidate, ec) && !ec)
                    {
                        path_ = candidate;
                        return true;
                    }
                }
                return false;
            }

            const fs::path& Path() const { return path_; }

        private:
            fs::path path_;
        };

        // Texture coordinates further out than this are taken for a damaged file (tiling by 1000 is already extreme)
        constexpr float kLargestUv = 1000.0f;

        // One assimp mesh (already triangulated, one material) as one engine mesh with a single material run
        std::optional<MeshData> ToMesh(const aiMesh& source, std::size_t& badUvs)
        {
            MeshData mesh;
            mesh.name = source.mName.C_Str();
            const bool hasUvs = source.HasTextureCoords(0);
            const bool hasNormals = source.HasNormals();
            mesh.positions.reserve(static_cast<std::size_t>(source.mNumVertices) * 3);
            for (unsigned v = 0; v < source.mNumVertices; ++v)
            {
                const aiVector3D& p = source.mVertices[v];
                mesh.positions.insert(mesh.positions.end(), {p.x, p.y, p.z});
                if (hasUvs)
                {
                    // Some exporters leave a few corners with absurd coordinates (billions); they would smear a
                    // texture over the faces that use them, so they are reset to the corner of the image
                    float u = source.mTextureCoords[0][v].x;
                    float w = source.mTextureCoords[0][v].y;
                    const auto sane = [](float x) { return std::isfinite(x) && std::fabs(x) <= kLargestUv; };
                    if (!sane(u) || !sane(w))
                    {
                        u = 0.0f;
                        w = 0.0f;
                        ++badUvs;
                    }
                    mesh.uvs.insert(mesh.uvs.end(), {u, w});
                }
                if (hasNormals)
                {
                    const aiVector3D& n = source.mNormals[v];
                    mesh.normals.insert(mesh.normals.end(), {n.x, n.y, n.z});
                }
            }
            for (unsigned f = 0; f < source.mNumFaces; ++f)
            {
                const aiFace& face = source.mFaces[f];
                if (face.mNumIndices == 3)
                {
                    mesh.indices.insert(mesh.indices.end(), {face.mIndices[0], face.mIndices[1], face.mIndices[2]});
                }
            }
            if (mesh.indices.empty())
            {
                return std::nullopt;
            }
            mesh.subMeshes.push_back(
                SubMesh{0, static_cast<std::uint32_t>(mesh.indices.size()), 0, source.mMaterialIndex});
            return mesh;
        }
    }

    namespace
    {
        /*!***********************************************************************
        @brief          Gives each skinned assimp mesh its skeleton: a Skin whose joints are the nodes its bones name
                        (each bone's offset matrix is its inverse bind matrix) and, per vertex, the four bones that
                        move it most with their weights (made to add up to 1).
        @param          scene         The imported scene.
        @param          meshSlot      Where each assimp mesh went in model.meshes (none for one left out).
        @param          nodeByName    The model's nodes by name.
        @param          model         Gets the skins, and the skinned meshes their joints and weights.
        @param          result        Gets a warning for a mesh whose bones are not all nodes of the model.
        *************************************************************************/
        void AddSkins(const aiScene& scene, const std::vector<std::optional<std::uint32_t>>& meshSlot,
                      const std::map<std::string, std::uint32_t>& nodeByName, ModelData& model, ConvertResult& result)
        {
            for (unsigned i = 0; i < scene.mNumMeshes; ++i)
            {
                const aiMesh& source = *scene.mMeshes[i];
                if (!source.HasBones() || !meshSlot[i])
                {
                    continue;
                }
                MeshData& mesh = model.meshes[*meshSlot[i]];
                Engine::Assets::Skin skin;
                skin.name = mesh.name;
                bool complete = true;
                for (unsigned b = 0; b < source.mNumBones && complete; ++b)
                {
                    const auto node = nodeByName.find(source.mBones[b]->mName.C_Str());
                    complete = node != nodeByName.end();
                    if (complete)
                    {
                        skin.joints.push_back(node->second);
                        skin.inverseBind.push_back(ToGlm(source.mBones[b]->mOffsetMatrix));
                    }
                }
                if (!complete || skin.joints.size() > std::numeric_limits<std::uint16_t>::max())
                {
                    result.warnings.push_back("mesh " + mesh.name +
                                              ": its skeleton names bones the model does not have; it will not bend");
                    continue;
                }

                // Every vertex's bones, strongest first, of which the first four are kept
                std::vector<std::vector<std::pair<float, std::uint16_t>>> influences(source.mNumVertices);
                for (unsigned b = 0; b < source.mNumBones; ++b)
                {
                    const aiBone& bone = *source.mBones[b];
                    for (unsigned w = 0; w < bone.mNumWeights; ++w)
                    {
                        const aiVertexWeight& weight = bone.mWeights[w];
                        if (weight.mVertexId < source.mNumVertices && weight.mWeight > 0.0f)
                        {
                            influences[weight.mVertexId].emplace_back(weight.mWeight, static_cast<std::uint16_t>(b));
                        }
                    }
                }
                mesh.joints.assign(static_cast<std::size_t>(source.mNumVertices) * 4, 0);
                mesh.weights.assign(static_cast<std::size_t>(source.mNumVertices) * 4, 0.0f);
                for (std::size_t v = 0; v < influences.size(); ++v)
                {
                    auto& list = influences[v];
                    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
                    list.resize(std::min<std::size_t>(list.size(), 4));
                    float total = 0.0f;
                    for (const auto& [weight, joint] : list)
                    {
                        total += weight;
                    }
                    for (std::size_t k = 0; k < list.size(); ++k)
                    {
                        mesh.joints[v * 4 + k] = list[k].second;
                        mesh.weights[v * 4 + k] = total > 0.0f ? list[k].first / total : 0.0f;
                    }
                }
                mesh.skin = static_cast<std::int32_t>(model.skins.size());
                model.skins.push_back(std::move(skin));
            }
        }

        /*!***********************************************************************
        @brief          Puts each skinned mesh's vertices where its skin expects them. The engine draws a skinned mesh
                        from the model's root, bent by its bones (skin matrix = joint at rest x inverse bind), so its
                        vertices have to be in the space the inverse binds start from. Most files give them in the
                        mesh node's own space; some (an FBX from Blender with an axis-turned mesh node) give inverse
                        binds from the root's space instead, and the mesh would bend from the wrong place as soon as
                        a bone moves. Those are told apart at rest: there a joint's skin matrix is the identity for
                        root-space binds, and the mesh node's placement otherwise.
        @param          model         The model, nodes, skins and skinned meshes done.
        *************************************************************************/
        void PlaceSkinnedVertices(ModelData& model)
        {
            std::vector<glm::mat4> worlds(model.nodes.size(), glm::mat4(1.0f));
            std::vector<std::int64_t> meshNode(model.meshes.size(), -1);
            const auto visit = [&](const auto& self, std::uint32_t index, const glm::mat4& parent) -> void
            {
                if (index >= model.nodes.size())
                {
                    return;
                }
                worlds[index] = parent * model.nodes[index].localTransform;
                for (const std::uint32_t mesh : model.nodes[index].meshIndices)
                {
                    if (mesh < meshNode.size() && meshNode[mesh] < 0)
                    {
                        meshNode[mesh] = index;
                    }
                }
                for (const std::uint32_t child : model.nodes[index].children)
                {
                    self(self, child, worlds[index]);
                }
            };
            for (const std::uint32_t root : model.roots)
            {
                visit(visit, root, glm::mat4(1.0f));
            }
            const auto nearly = [](const glm::mat4& a, const glm::mat4& b)
            {
                for (int column = 0; column < 4; ++column)
                {
                    for (int row = 0; row < 4; ++row)
                    {
                        if (std::abs(a[column][row] - b[column][row]) >
                            1e-3f * std::max(1.0f, std::abs(b[column][row])))
                        {
                            return false;
                        }
                    }
                }
                return true;
            };

            for (std::size_t m = 0; m < model.meshes.size(); ++m)
            {
                MeshData& mesh = model.meshes[m];
                if (mesh.skin < 0 || meshNode[m] < 0 || static_cast<std::size_t>(mesh.skin) >= model.skins.size())
                {
                    continue;
                }
                const Engine::Assets::Skin& skin = model.skins[static_cast<std::size_t>(mesh.skin)];
                const glm::mat4& place = worlds[static_cast<std::size_t>(meshNode[m])];
                if (skin.joints.empty() || skin.inverseBind.empty() || nearly(place, glm::mat4(1.0f)) ||
                    mesh.joints.size() != mesh.positions.size() / 3 * 4)
                {
                    continue; // already in the space its skin expects
                }
                // Which space that is: how far the vertices end up from the bones they mostly follow, as they are
                // and with the mesh's node applied. Exporters do either.
                const auto reach = [&](const glm::mat4& first)
                {
                    float total = 0.0f;
                    const std::size_t count = mesh.positions.size() / 3;
                    const std::size_t step = std::max<std::size_t>(count / 500, 1);
                    for (std::size_t v = 0; v < count; v += step)
                    {
                        const std::size_t joint = mesh.joints[v * 4];
                        if (joint >= skin.joints.size() || joint >= skin.inverseBind.size())
                        {
                            continue;
                        }
                        const glm::mat4& bone = worlds[skin.joints[joint]];
                        const glm::vec3 placed(bone * skin.inverseBind[joint] * first *
                                               glm::vec4(mesh.positions[v * 3], mesh.positions[v * 3 + 1],
                                                         mesh.positions[v * 3 + 2], 1.0f));
                        total += glm::length(placed - glm::vec3(bone[3]));
                    }
                    return total;
                };
                if (reach(place) >= reach(glm::mat4(1.0f)))
                {
                    continue;
                }
                const glm::mat3 turn = glm::transpose(glm::inverse(glm::mat3(place)));
                const std::size_t vertices = mesh.positions.size() / 3;
                for (std::size_t v = 0; v < vertices; ++v)
                {
                    const glm::vec3 p = glm::vec3(place * glm::vec4(mesh.positions[v * 3], mesh.positions[v * 3 + 1],
                                                                    mesh.positions[v * 3 + 2], 1.0f));
                    mesh.positions[v * 3] = p.x;
                    mesh.positions[v * 3 + 1] = p.y;
                    mesh.positions[v * 3 + 2] = p.z;
                    if (mesh.normals.size() == vertices * 3)
                    {
                        const glm::vec3 n = glm::normalize(
                            turn * glm::vec3(mesh.normals[v * 3], mesh.normals[v * 3 + 1], mesh.normals[v * 3 + 2]));
                        mesh.normals[v * 3] = n.x;
                        mesh.normals[v * 3 + 1] = n.y;
                        mesh.normals[v * 3 + 2] = n.z;
                    }
                }
            }
        }

        /*!***********************************************************************
        @brief          Leaves out the keys a channel does not need: those that drawing a straight line between the
                        keys kept around them gives back to within a hair (a part held still, or moving evenly).
                        Exporters often bake every frame at a high rate; this keeps a clip as it moves but a
                        fraction of the size.
        @param          channel       A linearly interpolated channel; its first and last keys always stay.
        *************************************************************************/
        void LeaveOutNeedlessKeys(Engine::Assets::AnimationChannel& channel)
        {
            using Engine::Assets::AnimationChannel;
            if (channel.interpolation != AnimationChannel::Interpolation::Linear || channel.times.size() < 3)
            {
                return;
            }
            const std::size_t width = channel.path == AnimationChannel::Path::Rotation ? 4 : 3;
            const std::size_t count = channel.times.size();
            // How far off a left-out key may be: a ten-thousandth of how far the channel moves (a rotation is a unit
            // quaternion, so that is a ten-thousandth of a turn's worth), and never less than a millionth
            float lowest = std::numeric_limits<float>::max();
            float highest = std::numeric_limits<float>::lowest();
            for (const float value : channel.values)
            {
                lowest = std::min(lowest, value);
                highest = std::max(highest, value);
            }
            const float tolerance = std::max((highest - lowest) * 1e-4f, 1e-6f);
            const auto value = [&channel, width](std::size_t key, std::size_t i)
            { return channel.values[key * width + i]; };

            // From the last key kept, go as far as a straight line still passes every key in between (looking back
            // at most kWindow keys, so a long even stretch is not checked over and over)
            constexpr std::size_t kWindow = 64;
            std::vector<std::size_t> kept{0};
            for (std::size_t next = 2; next < count; ++next)
            {
                const std::size_t from = kept.back();
                const float span = channel.times[next] - channel.times[from];
                bool straight = span > 0.0f;
                for (std::size_t between = std::max(from + 1, next > kWindow ? next - kWindow : 0);
                     straight && between < next; ++between)
                {
                    const float t = (channel.times[between] - channel.times[from]) / span;
                    for (std::size_t i = 0; i < width && straight; ++i)
                    {
                        const float line = value(from, i) + (value(next, i) - value(from, i)) * t;
                        straight = std::abs(line - value(between, i)) <= tolerance;
                    }
                }
                if (!straight)
                {
                    kept.push_back(next - 1);
                }
            }
            kept.push_back(count - 1);

            std::vector<float> times;
            std::vector<float> values;
            times.reserve(kept.size());
            values.reserve(kept.size() * width);
            for (const std::size_t key : kept)
            {
                times.push_back(channel.times[key]);
                values.insert(values.end(), channel.values.begin() + static_cast<std::ptrdiff_t>(key * width),
                              channel.values.begin() + static_cast<std::ptrdiff_t>((key + 1) * width));
            }
            channel.times = std::move(times);
            channel.values = std::move(values);
        }

        /*!***********************************************************************
        @brief          Copies the scene's animations: each node channel's position, rotation and scaling keys, in
                        seconds. A clip is named after its animation without the exporter's prefix ("Armature|Walk"
                        is "Walk"); a file of one clip with no name of its own (Blender's "Scene", "Take 001") is
                        named after the file, so a pack of one-animation files ("Rifle_Reload.fbx") gives clips that
                        can be told apart.
        @param          scene         The imported scene.
        @param          nodeByName    The model's nodes by name.
        @param          model         Gets the clips.
        @param          result        Gets a warning for channels that move nodes the model does not have.
        *************************************************************************/
        void AddAnimations(const aiScene& scene, const std::map<std::string, std::uint32_t>& nodeByName,
                           const std::string& fileStem, ModelData& model, ConvertResult& result)
        {
            using Engine::Assets::AnimationChannel;
            for (unsigned a = 0; a < scene.mNumAnimations; ++a)
            {
                const aiAnimation& source = *scene.mAnimations[a];
                const double ticksPerSecond = source.mTicksPerSecond > 0.0 ? source.mTicksPerSecond : 25.0;
                Engine::Assets::AnimationClip clip;
                clip.name = source.mName.C_Str();
                if (const auto bar = clip.name.rfind('|'); bar != std::string::npos)
                {
                    clip.name = clip.name.substr(bar + 1);
                }
                const bool generic = clip.name.empty() || clip.name == "Scene" || clip.name.starts_with("Take ") ||
                                     clip.name == "mixamo.com";
                if (generic && scene.mNumAnimations == 1 && !fileStem.empty())
                {
                    clip.name = fileStem;
                }
                else if (clip.name.empty())
                {
                    clip.name = "Animation " + std::to_string(a + 1);
                }
                std::size_t unknown = 0;
                for (unsigned c = 0; c < source.mNumChannels; ++c)
                {
                    const aiNodeAnim& channel = *source.mChannels[c];
                    const auto node = nodeByName.find(channel.mNodeName.C_Str());
                    if (node == nodeByName.end())
                    {
                        ++unknown;
                        continue;
                    }
                    const auto seconds = [ticksPerSecond](double ticks)
                    { return static_cast<float>(std::max(ticks, 0.0) / ticksPerSecond); };
                    if (channel.mNumPositionKeys > 0)
                    {
                        AnimationChannel out{.node = node->second, .path = AnimationChannel::Path::Translation};
                        for (unsigned k = 0; k < channel.mNumPositionKeys; ++k)
                        {
                            const aiVectorKey& key = channel.mPositionKeys[k];
                            out.times.push_back(seconds(key.mTime));
                            out.values.insert(out.values.end(), {key.mValue.x, key.mValue.y, key.mValue.z});
                        }
                        clip.channels.push_back(std::move(out));
                    }
                    if (channel.mNumRotationKeys > 0)
                    {
                        AnimationChannel out{.node = node->second, .path = AnimationChannel::Path::Rotation};
                        for (unsigned k = 0; k < channel.mNumRotationKeys; ++k)
                        {
                            const aiQuatKey& key = channel.mRotationKeys[k];
                            out.times.push_back(seconds(key.mTime));
                            out.values.insert(out.values.end(),
                                              {key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w});
                        }
                        clip.channels.push_back(std::move(out));
                    }
                    if (channel.mNumScalingKeys > 0)
                    {
                        AnimationChannel out{.node = node->second, .path = AnimationChannel::Path::Scale};
                        for (unsigned k = 0; k < channel.mNumScalingKeys; ++k)
                        {
                            const aiVectorKey& key = channel.mScalingKeys[k];
                            out.times.push_back(seconds(key.mTime));
                            out.values.insert(out.values.end(), {key.mValue.x, key.mValue.y, key.mValue.z});
                        }
                        clip.channels.push_back(std::move(out));
                    }
                }
                if (unknown > 0)
                {
                    result.warnings.push_back("animation " + clip.name + ": " + std::to_string(unknown) +
                                              " parts it moves are not in the model and were left out");
                }
                // Keys must go forwards in time for the engine to find them
                for (AnimationChannel& channel : clip.channels)
                {
                    const std::size_t width = channel.path == AnimationChannel::Path::Rotation ? 4 : 3;
                    for (std::size_t k = 1; k < channel.times.size(); ++k)
                    {
                        if (channel.times[k] < channel.times[k - 1])
                        {
                            channel.times.resize(k);
                            channel.values.resize(k * width);
                            break;
                        }
                    }
                    clip.duration = std::max(clip.duration, channel.times.back());
                    LeaveOutNeedlessKeys(channel);
                }
                clip.duration = std::max(clip.duration, static_cast<float>(source.mDuration / ticksPerSecond));
                if (!clip.channels.empty())
                {
                    model.animations.push_back(std::move(clip));
                }
            }
        }

        /*!***********************************************************************
        @brief          Converts one model file into one .amodel file (both already decided).
        @param          options       Input file, output file and settings.
        @param          packageRoot   The unpacked folder or zip the input came from; empty for a lone file.
        @param          result        Filled in; ok stays false and error says why when nothing was written.
        *************************************************************************/
        void ConvertFile(const ConvertOptions& options, const fs::path& packageRoot, ConvertResult& result)
        {
            std::error_code ec;
            if (!fs::is_regular_file(options.input, ec))
            {
                result.error = "input file not found: " + options.input.string();
                return;
            }
            if (ToLower(options.output.extension().string()) != Engine::Assets::kAtelierModelExtension)
            {
                result.error = "the output must be a .amodel file: " + options.output.string();
                return;
            }

            Assimp::Importer importer;
            // Lines and points are not geometry the engine draws; lights, cameras and colours are not kept. The
            // skeleton (bone weights, at most four a vertex) and the animations are.
            importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
            importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
                                        aiComponent_LIGHTS | aiComponent_CAMERAS | aiComponent_COLORS);
            importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);
            importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);

            const unsigned flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                                   aiProcess_GenSmoothNormals | aiProcess_SortByPType | aiProcess_GenUVCoords |
                                   aiProcess_TransformUVCoords | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
                                   aiProcess_RemoveComponent | aiProcess_ImproveCacheLocality |
                                   aiProcess_LimitBoneWeights | aiProcess_ValidateDataStructure;
            const aiScene* scene = importer.ReadFile(Utf8(options.input), flags);
            if (scene == nullptr || scene->mRootNode == nullptr)
            {
                result.error =
                    std::string("could not read ") + options.input.string() + ": " + importer.GetErrorString();
                return;
            }

            const fs::path modelDir = options.output.parent_path();
            const fs::path texturesDir = options.texturesDir.empty() ? modelDir : options.texturesDir;
            TextureCollector textures(*scene, options.input.parent_path(), modelDir, texturesDir,
                                      Utf8(options.input.stem()), result);

            ModelData model;
            model.name = Utf8(options.input.stem());
            for (unsigned m = 0; m < scene->mNumMaterials; ++m)
            {
                model.materials.push_back(ToMaterial(*scene->mMaterials[m], textures));
            }

            // assimp meshes map to engine meshes one to one, except a mesh with no triangles, which is left out: this
            // table says where each one went so the nodes can still point at it
            std::vector<std::optional<std::uint32_t>> meshSlot(scene->mNumMeshes);
            for (unsigned i = 0; i < scene->mNumMeshes; ++i)
            {
                std::size_t badUvs = 0;
                if (auto mesh = ToMesh(*scene->mMeshes[i], badUvs))
                {
                    if (badUvs > 0)
                    {
                        result.warnings.push_back("mesh " + mesh->name + ": " + std::to_string(badUvs) +
                                                  " damaged texture coordinates were reset");
                    }
                    if (mesh->subMeshes[0].materialIndex >= model.materials.size())
                    {
                        result.warnings.push_back("mesh " + mesh->name + " names a material that does not exist");
                        mesh->subMeshes[0].materialIndex = 0;
                        if (model.materials.empty())
                        {
                            MaterialData fallback;
                            fallback.name = "default";
                            model.materials.push_back(fallback);
                        }
                    }
                    meshSlot[i] = static_cast<std::uint32_t>(model.meshes.size());
                    model.meshes.push_back(std::move(*mesh));
                }
                else
                {
                    result.warnings.push_back(std::string("mesh without triangles skipped: ") +
                                              scene->mMeshes[i]->mName.C_Str());
                }
            }
            // A file of animations alone (a reload, a walk cycle for a rig kept in another file) is kept for its clips;
            // anything else with nothing to draw is an error
            if (model.meshes.empty() && scene->mNumAnimations == 0)
            {
                result.error = "the file holds no triangles: " + options.input.string();
                return;
            }

            // assimp adds a grey "DefaultMaterial" to files that have faces without one, and files list materials no
            // face uses; only the slots some mesh refers to are kept, in their original order
            {
                std::vector<std::uint32_t> remap(model.materials.size(), static_cast<std::uint32_t>(-1));
                for (const MeshData& mesh : model.meshes)
                {
                    remap[mesh.subMeshes[0].materialIndex] = 0;
                }
                std::vector<MaterialData> kept;
                for (std::size_t i = 0; i < model.materials.size(); ++i)
                {
                    if (remap[i] == 0)
                    {
                        remap[i] = static_cast<std::uint32_t>(kept.size());
                        kept.push_back(std::move(model.materials[i]));
                    }
                }
                for (MeshData& mesh : model.meshes)
                {
                    mesh.subMeshes[0].materialIndex = remap[mesh.subMeshes[0].materialIndex];
                }
                model.materials = std::move(kept);
            }

            // The node tree, root first; children are visited in order so a parent always has the lower index
            std::vector<const aiNode*> pending{scene->mRootNode};
            std::vector<std::uint32_t> parentOf{0};
            for (std::size_t next = 0; next < pending.size(); ++next)
            {
                const aiNode* source = pending[next];
                ModelNode node;
                node.name = source->mName.C_Str();
                node.localTransform = ToGlm(source->mTransformation);
                for (unsigned i = 0; i < source->mNumMeshes; ++i)
                {
                    if (const auto& slot = meshSlot[source->mMeshes[i]])
                    {
                        node.meshIndices.push_back(*slot);
                    }
                }
                if (next > 0)
                {
                    model.nodes[parentOf[next]].children.push_back(static_cast<std::uint32_t>(next));
                }
                model.nodes.push_back(std::move(node));
                for (unsigned i = 0; i < source->mNumChildren; ++i)
                {
                    pending.push_back(source->mChildren[i]);
                    parentOf.push_back(static_cast<std::uint32_t>(next));
                }
            }
            model.roots = {0};

            // Bones and animations name the nodes they move
            std::map<std::string, std::uint32_t> nodeByName;
            for (std::size_t n = 0; n < model.nodes.size(); ++n)
            {
                nodeByName.emplace(model.nodes[n].name, static_cast<std::uint32_t>(n)); // the first of a name
            }
            AddSkins(*scene, meshSlot, nodeByName, model, result);
            PlaceSkinnedVertices(model);
            AddAnimations(*scene, nodeByName, Utf8(options.input.stem()), model, result);

            // glTF models face +Z, and the engine's forward (where Motion blocks walk) is -Z: the root is turned round,
            // and so are the keys of any clip that moves the root itself
            const std::string inputExtension = ToLower(options.input.extension().string());
            if (inputExtension == ".gltf" || inputExtension == ".glb")
            {
                const glm::mat4 turn = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                const glm::quat turnRotation = glm::quat_cast(turn);
                model.nodes[0].localTransform = turn * model.nodes[0].localTransform;
                for (Engine::Assets::AnimationClip& clip : model.animations)
                {
                    for (Engine::Assets::AnimationChannel& channel : clip.channels)
                    {
                        using Path = Engine::Assets::AnimationChannel::Path;
                        if (channel.node != 0 || channel.path == Path::Scale)
                        {
                            continue;
                        }
                        const std::size_t width = channel.path == Path::Rotation ? 4 : 3;
                        for (std::size_t k = 0; k + width <= channel.values.size(); k += width)
                        {
                            float* v = &channel.values[k];
                            if (channel.path == Path::Translation)
                            {
                                const glm::vec3 moved(turn * glm::vec4(v[0], v[1], v[2], 1.0f));
                                v[0] = moved.x;
                                v[1] = moved.y;
                                v[2] = moved.z;
                            }
                            else
                            {
                                const glm::quat turned = turnRotation * glm::quat(v[3], v[0], v[1], v[2]);
                                v[0] = turned.x;
                                v[1] = turned.y;
                                v[2] = turned.z;
                                v[3] = turned.w;
                            }
                        }
                    }
                }
            }

            // Models come in whatever size and place their author used (a car in centimetres 76 units from the origin,
            // say), which can leave them far off screen or huge. fitSize and center put the whole model, as its nodes place
            // it, at a known size and place by changing only the root node.
            float scale = options.scale;
            glm::vec3 shift(0.0f);
            if (options.center || options.fitSize > 0.0f)
            {
                glm::vec3 low(std::numeric_limits<float>::max());
                glm::vec3 high(std::numeric_limits<float>::lowest());
                std::vector<glm::mat4> world(model.nodes.size(), glm::mat4(1.0f));
                for (std::size_t n = 0; n < model.nodes.size(); ++n) // parents come before their children
                {
                    const ModelNode& node = model.nodes[n];
                    world[n] = (n == 0 ? glm::mat4(1.0f) : world[parentOf[n]]) * node.localTransform;
                    for (const std::uint32_t meshIndex : node.meshIndices)
                    {
                        const std::vector<float>& positions = model.meshes[meshIndex].positions;
                        for (std::size_t i = 0; i + 2 < positions.size(); i += 3)
                        {
                            const glm::vec3 point(world[n] *
                                                  glm::vec4(positions[i], positions[i + 1], positions[i + 2], 1.0f));
                            low = glm::min(low, point);
                            high = glm::max(high, point);
                        }
                    }
                }
                const glm::vec3 size = high - low;
                const float longest = std::max({size.x, size.y, size.z});
                if (options.fitSize > 0.0f && longest > 0.0f)
                {
                    scale *= options.fitSize / longest;
                }
                if (options.center)
                {
                    shift = -(low + high) * 0.5f;
                }
            }
            if (scale != 1.0f || shift != glm::vec3(0.0f))
            {
                model.nodes[0].localTransform = glm::scale(glm::mat4(1.0f), glm::vec3(scale)) *
                                                glm::translate(glm::mat4(1.0f), shift) * model.nodes[0].localTransform;
            }

            const auto bytes = Engine::Assets::WriteAtelierModel(model);
            if (!Engine::Assets::ParseAtelierModel(bytes))
            {
                result.error = "internal error: the converted model did not pass its own check";
                return;
            }
            fs::create_directories(modelDir, ec);
            if (!WriteAll(options.output, reinterpret_cast<const char*>(bytes.data()), bytes.size()))
            {
                result.error = "could not write " + options.output.string();
                return;
            }

            result.ok = true;
            result.output = options.output;
            result.meshes = model.meshes.size();
            result.materials = model.materials.size();
            result.nodes = model.nodes.size();

            if (options.modelFolder)
            {
                KeepSources(options.input, packageRoot, modelDir, texturesDir, result);
            }

            // A model that names no texture at all while images sit around it: usually an OBJ that came without its .mtl,
            // so nothing says which image goes on which part (the model is drawn in plain colours)
            const bool textured = std::any_of(model.materials.begin(), model.materials.end(),
                                              [](const MaterialData& m) {
                                                  return !m.baseColorTexture.empty() || !m.normalTexture.empty() ||
                                                         !m.metallicRoughnessTexture.empty();
                                              });
            if (!textured)
            {
                // With a model folder the images are kept with the model, so they can still be used
                const std::vector<std::string> images =
                    ImagesAround(options.modelFolder ? texturesDir : options.input.parent_path());
                if (!images.empty() && options.modelFolder)
                {
                    result.warnings.push_back("the model names no textures, so its " + std::to_string(images.size()) +
                                              " image(s) (" + NameList(images) +
                                              ") are stored with it but not on it: use one as the object's Texture, "
                                              "or add a .mtl file that says which image goes on which part");
                }
                else if (!images.empty())
                {
                    result.warnings.push_back(
                        "the model names no textures, so the " + std::to_string(images.size()) +
                        " image(s) beside it (" + NameList(images) +
                        ") are not used; an OBJ needs its .mtl file to say which image goes on which part");
                }
            }
        }
    }

    ConvertResult ConvertModelFile(const ConvertOptions& options)
    {
        ConvertResult result;
        std::error_code ec;
        ConvertOptions resolved = options;
        TempFolder unpacked;
        fs::path package;
        if (fs::is_directory(options.input, ec))
        {
            package = options.input;
        }
        else if (ToLower(options.input.extension().string()) == ".zip" && fs::is_regular_file(options.input, ec))
        {
            std::string error;
            if (!unpacked.Create() || !ExtractZip(options.input, unpacked.Path(), result.warnings, error))
            {
                result.error = "could not unpack " + Utf8(options.input.filename()) + ": " +
                               (error.empty() ? "no temporary folder" : error);
                return result;
            }
            package = unpacked.Path();
        }
        if (!package.empty())
        {
            std::string error;
            const auto model = FindModel(package, error);
            if (!model)
            {
                result.error = error + " in " + Utf8(options.input.filename());
                return result;
            }
            resolved.input = *model;
            const auto inside = model->lexically_relative(package).generic_u8string();
            result.source = std::string(inside.begin(), inside.end());
        }
        else
        {
            result.source = Utf8(options.input.filename());
        }
        if (options.modelFolder)
        {
            if (ToLower(options.output.extension().string()) == Engine::Assets::kAtelierModelExtension)
            {
                result.error = "the output must be the models folder, not a .amodel file, to give the model a folder";
                return result;
            }
            // <models folder>/<name>/<name>.amodel, named after the model
            const fs::path folder = PickModelFolder(options.output, Utf8(resolved.input.stem()), resolved.input);
            resolved.output = folder / FromUtf8(Utf8(folder.filename()) + ".amodel");
            // The images go to <textures folder>/<name>/ when one is given (the game's art/textures), else beside
            resolved.texturesDir = options.texturesDir.empty() ? fs::path{} : options.texturesDir / folder.filename();
        }
        else if (options.output.extension().empty())
        {
            resolved.output = options.output / FromUtf8(Utf8(resolved.input.stem()) + ".amodel");
        }
        ConvertFile(resolved, package, result);
        return result;
    }
}
