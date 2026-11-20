/* !
@file    	    Resources.cpp
@author  	    j.cai@digipen.edu [100%]
@date                   26/10/2026
@brief
    Implements the audio half of the asset loaders: .wav through our own RIFF
    parser, .ogg through stb_vorbis, plus the silent placeholder. Mesh, texture
    and shader loading live in engine/assets/loaders.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/assets/Resources.hpp"

// Engine Core Headers
#include "engine/core/FileSystem.hpp"

// Declarations only: the implementation is compiled from stb_vorbis.c itself (see the engine CMakeLists.txt)
#define STB_VORBIS_HEADER_ONLY
#include <stb/stb_vorbis.c> // NOLINT(bugprone-suspicious-include)

// Standard Library Headers
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace Engine::Assets
{
    namespace
    {
        // WAV files are little-endian, read byte by byte so it works on any CPU
        std::uint32_t ReadU32(const std::uint8_t* p)
        {
            return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
                   (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
        }

        std::uint16_t ReadU16(const std::uint8_t* p)
        {
            return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
        }

    }

    // .wav (our own parser) and .ogg (stb_vorbis). This is only for the asset list - FMOD does the real playback.
    std::optional<AudioClip> LoadAudioFile(const std::filesystem::path& path)
    {
        const auto bytes = Core::ReadBytes(path);
        if (!bytes)
        {
            return std::nullopt;
        }

        const auto ext = Core::Extension(path);
        if (ext == ".wav")
        {
            return ParseWav(*bytes);
        }
        if (ext == ".ogg")
        {
            int channels = 0;
            int sampleRate = 0;
            short* output = nullptr;
            const int frames = stb_vorbis_decode_memory(bytes->data(), static_cast<int>(bytes->size()), &channels,
                                                        &sampleRate, &output);
            if (frames < 0 || !output)
            {
                return std::nullopt;
            }
            AudioClip clip{.channels = channels, .sampleRate = sampleRate};
            clip.samples.assign(output, output + static_cast<std::size_t>(frames) * static_cast<std::size_t>(channels));
            std::free(output);
            return clip;
        }
        return std::nullopt;
    }

    // PCM16 RIFF/WAVE only
    std::optional<AudioClip> ParseWav(const std::vector<std::uint8_t>& bytes)
    {
        if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
            std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
        {
            return std::nullopt;
        }

        AudioClip clip;
        int bitsPerSample = 0;
        bool hasFormat = false;

        std::size_t offset = 12;
        while (offset + 8 <= bytes.size())
        {
            const std::uint8_t* chunk = bytes.data() + offset;
            const std::size_t size = ReadU32(chunk + 4);
            const std::size_t body = offset + 8;
            if (size > bytes.size() - body)
            {
                return std::nullopt;
            }

            if (std::memcmp(chunk, "fmt ", 4) == 0 && size >= 16)
            {
                const auto format = ReadU16(chunk + 8);
                clip.channels = ReadU16(chunk + 10);
                clip.sampleRate = static_cast<int>(ReadU32(chunk + 12));
                bitsPerSample = ReadU16(chunk + 22);
                hasFormat = format == 1;
            }
            else if (std::memcmp(chunk, "data", 4) == 0)
            {
                if (!hasFormat || bitsPerSample != 16 || clip.channels <= 0)
                {
                    return std::nullopt;
                }
                clip.samples.resize(size / 2);
                std::memcpy(clip.samples.data(), bytes.data() + body, clip.samples.size() * 2);
                return clip;
            }

            offset = body + size + (size & 1); // chunks are padded to an even size
        }
        return std::nullopt;
    }

    // 0.1 s of silence
    AudioClip PlaceholderAudio()
    {
        return AudioClip{.channels = 1, .sampleRate = 44100, .samples = std::vector<std::int16_t>(4410, 0)};
    }
}
