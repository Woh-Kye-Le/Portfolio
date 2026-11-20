/* !
@file    	    Resources.hpp
@author  	    j.cai@digipen.edu [100%]
@date    		27/10/2026
@brief
    Temporary compatibility shim. The mesh, texture and shader data and their
    loaders now live in Engine::Assets (engine/assets/data, engine/assets/loaders),
    so this header only re-exports the short names the resource pool and the GPU
    cache still ask for. Delete once those call sites use the domain names
    directly. Audio decoding still lives here.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Assets Headers
#include "engine/assets/data/MeshData.hpp"
#include "engine/assets/data/ModelData.hpp"
#include "engine/assets/data/ShaderData.hpp"
#include "engine/assets/data/TextureData.hpp"
#include "engine/assets/loaders/MeshLoader.hpp"
#include "engine/assets/loaders/ModelLoader.hpp"
#include "engine/assets/loaders/ObjLoader.hpp"
#include "engine/assets/loaders/ShaderLoader.hpp"
#include "engine/assets/loaders/TextureLoader.hpp"

// Standard Library Headers
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace Engine::Assets
{
    // Short names for the resource pools: a "Mesh/Texture/Shader/Model" asset is
    // the CPU-side blob, not the Graphics::* GPU object built from it.
    using Mesh = MeshData;
    using Texture = TextureData;
    using Shader = ShaderData;
    using Model = ModelData;

    // Decoded sound (only used for the asset list and tests, FMOD loads its own copy to play it)
    struct AudioClip
    {
        int channels = 0;
        int sampleRate = 0;
        std::vector<std::int16_t> samples; // interleaved PCM16
    };

    // Each returns nullopt if the file is missing or cannot be parsed. Format is picked by extension.
    std::optional<AudioClip> LoadAudioFile(const std::filesystem::path& path); // .wav .ogg

    // Parser split out from the file loader so tests can feed it bytes directly
    std::optional<AudioClip> ParseWav(const std::vector<std::uint8_t>& bytes);

    // Stand-in used when a sound is missing
    AudioClip PlaceholderAudio(); // silence
}
