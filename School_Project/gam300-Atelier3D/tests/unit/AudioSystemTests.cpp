#include "engine/audio/AudioSystem.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

using Engine::Audio::AudioSystem;

// Playing a sound file through AudioSystem, as the editor's Diagnostics panel does. Pass on both backends: FMOD
// (Windows) plays for real (at volume 0, so tests are silent); the stub (Linux / CI) reports that there is no sound.

namespace
{
    bool HasFmod()
    {
        return std::string(AudioSystem::BackendName()) == "FMOD";
    }

    template <typename T> void Write(std::ofstream& out, T value)
    {
        out.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    // Writes a short silent 16-bit mono WAV file, so the tests need no sound assets. One file per test, so tests
    // running in parallel (CTest -j, sharded CI runs) never rewrite a file another test is playing
    std::filesystem::path MakeWav()
    {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        const auto path = std::filesystem::temp_directory_path() /
                          ("atelier_audio_test_" + std::string(info->name()) + ".wav");
        constexpr std::uint32_t kSampleRate = 22050;
        constexpr std::uint32_t kSamples = kSampleRate / 10; // 0.1 s
        constexpr std::uint32_t kDataBytes = kSamples * 2;

        std::ofstream out(path, std::ios::binary);
        out.write("RIFF", 4);
        Write<std::uint32_t>(out, 36 + kDataBytes);
        out.write("WAVEfmt ", 8);
        Write<std::uint32_t>(out, 16); // fmt chunk size
        Write<std::uint16_t>(out, 1);  // PCM
        Write<std::uint16_t>(out, 1);  // mono
        Write<std::uint32_t>(out, kSampleRate);
        Write<std::uint32_t>(out, kSampleRate * 2); // bytes per second
        Write<std::uint16_t>(out, 2);               // block align
        Write<std::uint16_t>(out, 16);              // bits per sample
        out.write("data", 4);
        Write<std::uint32_t>(out, kDataBytes);
        for (std::uint32_t i = 0; i < kSamples; ++i)
        {
            Write<std::int16_t>(out, 0);
        }
        return path;
    }
}

// Before Init() nothing works, but calling Stop/Shutdown anyway must not crash.
TEST(AudioSystem, NothingPlaysBeforeInit)
{
    AudioSystem audio;
    EXPECT_FALSE(audio.IsAvailable());
    EXPECT_FALSE(audio.LoadSound("anything.wav").IsValid());
    EXPECT_FALSE(audio.Play({}).IsValid());
    audio.Stop({});   // safe
    audio.Shutdown(); // safe
}

// Init() only succeeds when the build has FMOD, and calling it twice is fine.
TEST(AudioSystem, InitMatchesTheBackend)
{
    AudioSystem audio;
    EXPECT_EQ(audio.Init(), HasFmod());
    EXPECT_EQ(audio.IsAvailable(), HasFmod());
    EXPECT_EQ(audio.Init(), HasFmod()); // twice is fine
}

// Load a generated wav, play it (muted), check it plays, then stop it. Loading the same path twice gives back the same
// sound.
TEST(AudioSystem, PlaysAndStopsASoundFile)
{
    const auto wav = MakeWav();
    AudioSystem audio;
    audio.Init();

    const auto sound = audio.LoadSound(wav.string());
    EXPECT_EQ(sound.IsValid(), HasFmod());
    EXPECT_EQ(audio.LoadSound(wav.string()), sound); // the same path gives the same clip
    const auto voice = audio.Play(sound);
    audio.SetVoiceVolume(voice, 0.0f);
    audio.Update();
    EXPECT_EQ(audio.IsPlaying(voice), HasFmod());

    audio.Stop(voice);
    audio.Update();
    EXPECT_FALSE(audio.IsPlaying(voice));

    audio.Shutdown();
    std::filesystem::remove(wav);
}

// A file that does not exist gives an invalid sound, and playing that gives no voice (no crash).
TEST(AudioSystem, MissingFileGivesNoSound)
{
    AudioSystem audio;
    audio.Init();

    const auto sound = audio.LoadSound("definitely/not/a/real/file.wav");
    EXPECT_FALSE(sound.IsValid());
    EXPECT_FALSE(audio.Play(sound).IsValid());
}
