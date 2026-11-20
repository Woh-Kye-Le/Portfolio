/* !
@file    	    SceneFile.cpp
@author  	    sung.h@digipen.edu [94%]
         	    k.thantwin@digipen.edu [3%]
         	    c.kojat@digipen.edu [3%]
@date    		02/10/2026
@brief
    Implements the scene file format: the version check, reading and writing the
    object list (GameObjectJson) and error logging.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/scene/SceneFile.hpp"

#include "GameObjectJson.hpp"

#include "engine/core/Log.hpp"
#include "engine/serializer/JsonSerializer.hpp"

#include <exception>
#include <format>
#include <utility>

namespace Engine::Scene
{
    namespace
    {
        using Serializer::JSONSerializer;

        // nlohmann's messages start with "[json.exception.parse_error.101] "; the rest is the useful part
        std::string WithoutJsonPrefix(const std::string& message)
        {
            const auto end = message.find("] ");
            return message.rfind("[json.exception.", 0) == 0 && end != std::string::npos ? message.substr(end + 2)
                                                                                         : message;
        }

        // The actual loading. Problems that make the whole file unusable (missing, bad JSON, wrong version)
        // end it with fail(); smaller problems just become warnings and we carry on with defaults.
        SceneDescription Parse(const std::filesystem::path& path, SceneLoadReport& report)
        {
            const auto fail = [&](std::string error)
            {
                report.ok = false;
                report.error = std::move(error);
                return SceneDescription{};
            };

            std::error_code ec;
            if (!std::filesystem::is_regular_file(path, ec))
            {
                return fail("file not found: " + path.string());
            }

            JSONSerializer json;
            try
            {
                json.LoadFile(path.string());
            }
            catch (const nlohmann::json::exception& e)
            {
                return fail("not valid JSON - " + WithoutJsonPrefix(e.what()));
            }
            catch (const std::exception& e)
            {
                return fail(e.what()); // e.g. the file could not be opened
            }

            if (!json.GetCurrentNode().is_object())
            {
                return fail("the file must contain a JSON object: { \"version\": ..., \"objects\": [ ... ] }");
            }
            int version = 0;
            if (!json.HasKey("version"))
            {
                return fail(std::format("missing \"version\" (scene files start with {{ \"version\": {}, ...)",
                                        kSceneFormatVersion));
            }
            if (!json.TryReadInt("version", version) || version < kOldestSceneFormatVersion ||
                version > kSceneFormatVersion)
            {
                return fail(std::format("unsupported format version {} (this build reads versions {} to {}; versions 1 "
                                        "to 6 used an older layout)",
                                        json.GetCurrentNode().at("version").dump(), kOldestSceneFormatVersion,
                                        kSceneFormatVersion));
            }
            if (!json.HasArray("objects"))
            {
                return fail("missing the \"objects\" list: { \"objects\": [ ... ] }");
            }

            SceneDescription scene;
            scene.objects = Detail::ReadObjects(json, report);
            report.ok = true;
            return scene;
        }
    }

    // Never throws, see SceneFile.hpp
    SceneLoadReport LoadSceneFile(const std::filesystem::path& path, SceneDescription& scene)
    {
        SceneLoadReport report;
        try
        {
            // only overwrite the caller's scene if the load worked, so a bad file doesn't wipe what's open
            SceneDescription loaded = Parse(path, report);
            if (report.ok)
            {
                scene = std::move(loaded);
            }
        }
        catch (const std::exception& e) // never expected; a scene load must not take the app down
        {
            report.ok = false;
            report.error = e.what();
        }

        // Every scene load goes through here, so a scene that is not used exactly as written is always in the log
        if (!report.ok)
        {
            LOG_ERROR(IO, "Scene {}: {}", path.string(), report.error);
        }
        for (const auto& warning : report.warnings)
        {
            LOG_ERROR(IO, "Scene {}: {}", path.string(), warning);
        }
        return report;
    }

    // Only the components an object has are written, so files stay small
    void SaveSceneFile(const std::filesystem::path& path, const SceneDescription& scene)
    {
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }

        // { "version": 8, "objects": [ { "id", "name", <only the components the object has> } ] }
        JSONSerializer json;
        json.ParseJSON("{}");
        json.WriteInt("version", kSceneFormatVersion);
        Detail::WriteObjects(json, scene.objects);
        json.SaveFile(path.string()); // throws std::runtime_error if it cannot be written
    }
}
