/* !
@file               MaterialFile.cpp
@author             c.kojat@digipen.edu [100%]
@date               02/11/2026
@brief
    Implements reading and writing material files (.amat, JSON).

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/assets/MaterialFile.hpp"

// Third-Party Headers
#include <nlohmann/json.hpp>

// Standard Library Headers
#include <fstream>
#include <sstream>
#include <system_error>

namespace Engine::Assets
{
    namespace
    {
        using nlohmann::json;
        constexpr int kVersion = 1;

        template <int N> void ReadFloats(const json& root, const char* key, float* values)
        {
            if (root.contains(key) && root[key].is_array() && root[key].size() == N)
            {
                for (int i = 0; i < N; ++i)
                {
                    if (root[key][static_cast<std::size_t>(i)].is_number())
                    {
                        values[i] = root[key][static_cast<std::size_t>(i)].get<float>();
                    }
                }
            }
        }
    }

    std::optional<MaterialFile> ReadMaterialFile(const std::filesystem::path& path, std::string* error)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            if (error)
            {
                *error = "cannot open " + path.generic_string();
            }
            return std::nullopt;
        }
        std::stringstream text;
        text << in.rdbuf();
        json root;
        try
        {
            root = json::parse(text.str());
        }
        catch (const json::exception& e)
        {
            if (error)
            {
                *error = path.filename().string() + " is not a material: " + e.what();
            }
            return std::nullopt;
        }
        if (!root.is_object())
        {
            if (error)
            {
                *error = path.filename().string() + " is not a material";
            }
            return std::nullopt;
        }

        MaterialFile material;
        const auto readText = [&root](const char* key, std::string& value)
        {
            if (root.contains(key) && root[key].is_string())
            {
                value = root[key].get<std::string>();
            }
        };
        const auto readNumber = [&root](const char* key, float& value)
        {
            if (root.contains(key) && root[key].is_number())
            {
                value = root[key].get<float>();
            }
        };
        readText("shader", material.shader);
        readText("texture", material.texture);
        ReadFloats<4>(root, "color", &material.color[0]);
        ReadFloats<3>(root, "emissive", &material.emissive[0]);
        readNumber("metallic", material.metallic);
        readNumber("roughness", material.roughness);
        readNumber("emissive_strength", material.emissiveStrength);
        if (root.contains("values") && root["values"].is_object())
        {
            for (const auto& [name, value] : root["values"].items())
            {
                if (value.is_number())
                {
                    material.values.emplace_back(name, value.get<float>());
                }
            }
        }
        return material;
    }

    bool WriteMaterialFile(const std::filesystem::path& path, const MaterialFile& material, std::string* error)
    {
        json root;
        root["version"] = kVersion;
        root["shader"] = material.shader;
        root["texture"] = material.texture;
        root["color"] = {material.color.r, material.color.g, material.color.b, material.color.a};
        root["metallic"] = material.metallic;
        root["roughness"] = material.roughness;
        root["emissive"] = {material.emissive.r, material.emissive.g, material.emissive.b};
        root["emissive_strength"] = material.emissiveStrength;
        json values = json::object();
        for (const auto& [name, value] : material.values)
        {
            values[name] = value;
        }
        root["values"] = values;

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            if (error)
            {
                *error = "cannot write " + path.generic_string();
            }
            return false;
        }
        out << root.dump(2);
        return static_cast<bool>(out);
    }
}
