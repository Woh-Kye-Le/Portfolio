/* !
@file    	    SceneFile.hpp
@author  	    sung.h@digipen.edu [94%]
         	    k.thantwin@digipen.edu [3%]
         	    c.kojat@digipen.edu [3%]
@date    		02/10/2026
@brief
    Declares LoadSceneFile and SaveSceneFile for the versioned JSON scene
    format, and the load report.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/scene/SceneDescription.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace Engine::Scene
{
    // Scene files (data/scenes/*.json), read and written with Serializer::JSONSerializer. The editor and the game
    // share this one format:
    //   { "version": 8,
    //     "objects": [ { "id": 1, "name": "Crate", "parent": 3,
    //                    "transform": { "x": 0, "y": 0, "z": 0, "rotation_x": 0, "rotation": 0, "rotation_z": 0,
    //                                   "size": 1, "scale_x": 1, "scale_y": 1, "scale_z": 1 },
    //                    "renderable": { "shape": "cube", "rotation_speed": 60,
    //                                    "mesh": "...", "texture": "...", "shader": "...",
    //                                    "albedo_r": 1, "albedo_g": 1, "albedo_b": 1, "albedo_a": 1,
    //                                    "shape_size_x": 1, "shape_size_y": 1, "shape_size_z": 1,
    //                                    "shape_center_x": 0, "shape_center_y": 0, "shape_center_z": 0 },
    //                    "audio_source": { "clip": "...", "volume": 1, "loop": true, "play_on_start": true,
    //                                      "spatial": true, "min_distance": 2, "max_distance": 50, "bus": "sfx" },
    //                    "player": { "speed": 3, "fast_multiplier": 2, "camera_offset_y": 1, "camera_offset_z": 4,
    //                                "follow_camera": true },
    //                    "camera": { "field_of_view": 60, "near_plane": 0.05, "far_plane": 1000, "primary": true },
    //                    "light": { "type": "point", "enabled": true, "color_r": 1, "color_g": 1, "color_b": 1,
    //                               "intensity": 1, "range": 10, "spot_inner": 25, "spot_outer": 35 },
    //                    "health": { "health": 100, "max_health": 100 },
    //                    "rigid_body": { "type": "dynamic", "mass": 1, "gravity_scale": 1, "friction": 0.5,
    //                                    "bounciness": 0, "start_velocity_x": 0, "start_velocity_y": 0,
    //                                    "start_velocity_z": 0, "shape": "auto", "center_x": 0, "center_y": 0,
    //                                    "center_z": 0, "size_x": 1, "size_y": 1, "size_z": 1, "radius": 0.5,
    //                                    "height": 2 },
    //                    "screen": { "kind": "text", "text": "Score", "picture": "", "bar_object": "",
    //                                "bar_variable": "", "bar_maximum": 100, "anchor": "top_left", "x": 4, "y": 5,
    //                                "height": 6, "width": 0, "color_r": 1, "color_g": 1, "color_b": 1, "layer": 0,
    //                                "visible": true },
    //                    "prefab": { "path": "assets/prefabs/Enemy.prefab", "object": 2,
    //                                "overrides": [ "health/health" ] } } ] }
    // Each component is written only if the object has it, and parent only if the object has one (its transform is
    // then relative to the parent; see SceneDescription.hpp). Asset paths and the tint are optional; every other field
    // is always written. Writing is deterministic, so saving a scene that was just loaded gives the same file. Prefab
    // files (Prefab.hpp) list their objects the same way.
    //
    // Bump the version when the layout changes in a way older builds cannot read:
    //   7: each object lists its components (versions 1 to 6 were a flat one-line-per-object layout, no longer read)
    //   8: parents, with child transforms relative to them; Health; prefab instances. Version 7 files still load.
    inline constexpr int kSceneFormatVersion = 8;
    inline constexpr int kOldestSceneFormatVersion = 7;

    // What LoadSceneFile found in a scene file
    struct SceneLoadReport
    {
        bool ok = false;                   // read, valid JSON, a supported version, laid out as { "objects": [ ... ] }
        std::string error;                 // why not: missing file, invalid JSON (with line / column), unsupported
                                           // version, wrong layout
        std::vector<std::string> warnings; // entries skipped or fields that fell back to their defaults
    };

    // Reads a scene file. Never throws. Forgiving: an entry that is not an object is skipped, a missing or mistyped
    // field keeps its default, unknown components are skipped, and a parent that does not exist (or would make a loop
    // of parents) is dropped. The error and each warning are also written to
    // the engine log at LogLevel::Error, since each means data in the file was not used as written.
    // scene is only changed when the report is ok.
    SceneLoadReport LoadSceneFile(const std::filesystem::path& path, SceneDescription& scene);

    // Writes a scene file, creating its folder if needed. Throws std::runtime_error if the file cannot be written.
    void SaveSceneFile(const std::filesystem::path& path, const SceneDescription& scene);
}
