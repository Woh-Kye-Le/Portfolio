/* !
@file    	    GameObjectJson.hpp
@author  	    sung.h@digipen.edu [70%]
         	    c.kojat@digipen.edu [30%]
@date    		20/10/2026
@brief
    Internal to the engine: reading and writing game objects in the JSON layout
    that scene files and prefab files share.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/logic/Blocks.hpp"
#include "engine/scene/SceneFile.hpp"
#include "engine/serializer/JsonSerializer.hpp"

#include <string>
#include <vector>

namespace Engine::Scene::Detail
{
    // Writes "objects": [ ... ] into the current JSON object, each one laid out as SceneFile.hpp shows
    void WriteObjects(Serializer::JSONSerializer& json, const std::vector<GameObject>& objects);

    // Reads the "objects" list of the current JSON object (the caller checks it is there). Forgiving, like
    // LoadSceneFile: entries that are not objects are skipped, bad fields keep their defaults, ids are made unique and
    // a parent that does not exist (or would make a loop) is dropped, each with a warning starting with what
    // ("object 3 (Crate): ...").
    std::vector<GameObject> ReadObjects(Serializer::JSONSerializer& json, SceneLoadReport& report,
                                        const std::string& what = "object");

    // A list of blocks under key, as a scene's "logic" writes and reads its scripts (the block library's file uses
    // them too). Reading is as forgiving as for a scene: blocks that are unknown or cannot be where they are are left
    // out, and ids are made unique, each with a warning starting with where.
    void WriteBlockList(Serializer::JSONSerializer& json, const std::string& key,
                        const std::vector<Logic::Block>& blocks);
    std::vector<Logic::Block> ReadBlockList(Serializer::JSONSerializer& json, const std::string& key,
                                            SceneLoadReport& report, const std::string& where);

    // One object as the file writes it, and back. Used to compare objects property by property (prefab overrides).
    nlohmann::json ToJson(const GameObject& object);
    GameObject FromJson(const nlohmann::json& node);
}
