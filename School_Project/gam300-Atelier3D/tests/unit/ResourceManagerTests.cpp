#include "engine/assets/ResourceManager.hpp"
#include "engine/core/Log.hpp"
#include "engine/core/LogSinks.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using Engine::Assets::AssetType;
using Engine::Assets::ResourceManager;
using Engine::Assets::TextureHandle;

namespace
{
    // 2x2 RGB PNG: red, blue / blue, red
    const std::vector<std::uint8_t> kPng = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44,
        0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x02, 0x00, 0x00, 0x00, 0xFD,
        0xD4, 0x9A, 0x73, 0x00, 0x00, 0x00, 0x11, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0x63, 0xF8,
        0xCF, 0x00, 0x04, 0x20, 0xE2, 0x3F, 0x90, 0x00, 0x00, 0x1B, 0xF2, 0x03, 0xFD, 0x21, 0x8E,
        0x88, 0x90, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

    // Unit crate with 6 quad faces
    const std::string kCrateObj = "v -0.35 -0.35  0.35\nv  0.35 -0.35  0.35\nv  0.35  0.35  0.35\nv -0.35  0.35  0.35\n"
                                  "v -0.35 -0.35 -0.35\nv  0.35 -0.35 -0.35\nv  0.35  0.35 -0.35\nv -0.35  0.35 -0.35\n"
                                  "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                                  "f 1/1 2/2 3/3 4/4\nf 6/1 5/2 8/3 7/4\nf 5/1 1/2 4/3 8/4\n"
                                  "f 2/1 6/2 7/3 3/4\nf 4/1 3/2 7/3 8/4\nf 5/1 6/2 2/3 1/4\n";

    std::vector<std::uint8_t> MakeWav(std::uint16_t channels, std::uint32_t sampleRate,
                                      const std::vector<std::int16_t>& samples)
    {
        std::vector<std::uint8_t> bytes;
        auto put = [&bytes](std::uint32_t value, int size)
        {
            for (int i = 0; i < size; ++i)
            {
                bytes.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
            }
        };
        auto tag = [&bytes](const char* text) { bytes.insert(bytes.end(), text, text + 4); };

        const auto dataSize = static_cast<std::uint32_t>(samples.size() * 2);
        tag("RIFF");
        put(36 + dataSize, 4);
        tag("WAVE");
        tag("fmt ");
        put(16, 4);
        put(1, 2); // PCM
        put(channels, 2);
        put(sampleRate, 4);
        put(sampleRate * channels * 2, 4);
        put(channels * 2u, 2);
        put(16, 2);
        tag("data");
        put(dataSize, 4);
        for (const auto sample : samples)
        {
            put(static_cast<std::uint16_t>(sample), 2);
        }
        return bytes;
    }

    // Smallest baseline JPEG: one 8x8 greyscale block where every pixel is 200.
    // Quantization is all 1s and each Huffman table holds a single 1-bit code "0", so the scan is just
    // DC category 10 + the 10 bits of (200 - 128) * 8 = 576, then end-of-block.
    std::vector<std::uint8_t> MakeGreyJpeg()
    {
        std::vector<std::uint8_t> bytes = {0xFF, 0xD8}; // start of image
        auto segment = [&bytes](std::uint8_t marker, const std::vector<std::uint8_t>& body)
        {
            const auto length = static_cast<std::uint16_t>(body.size() + 2);
            bytes.insert(bytes.end(), {0xFF, marker, static_cast<std::uint8_t>(length >> 8),
                                       static_cast<std::uint8_t>(length & 0xFF)});
            bytes.insert(bytes.end(), body.begin(), body.end());
        };
        auto huffmanTable = [](std::uint8_t classAndId, std::uint8_t symbol)
        {
            std::vector<std::uint8_t> body = {classAndId, 1}; // one code of length 1
            body.resize(17, 0);
            body.push_back(symbol);
            return body;
        };

        std::vector<std::uint8_t> quantization = {0x00};
        quantization.resize(65, 1);
        segment(0xDB, quantization);
        segment(0xC0, {8, 0, 8, 0, 8, 1, 1, 0x11, 0}); // 8-bit, 8x8, one component
        segment(0xC4, huffmanTable(0x00, 10));         // DC table: category 10
        segment(0xC4, huffmanTable(0x10, 0x00));       // AC table: end-of-block
        segment(0xDA, {1, 1, 0x00, 0, 63, 0});
        bytes.insert(bytes.end(), {0x48, 0x0F}); // bits 0 1001000000 0 + 1111 padding
        bytes.insert(bytes.end(), {0xFF, 0xD9}); // end of image
        return bytes;
    }

    // ASCII FBX 7.4: one quad mesh (4 corners, UVs per polygon vertex) attached to a model under the root
    const std::string kQuadFbx = R"(; FBX 7.4.0 project file
FBXHeaderExtension:  {
    FBXHeaderVersion: 1003
    FBXVersion: 7400
}
Objects:  {
    Geometry: 1000, "Geometry::Quad", "Mesh" {
        Vertices: *12 {
            a: -1,-1,0,1,-1,0,1,1,0,-1,1,0
        }
        PolygonVertexIndex: *4 {
            a: 0,1,2,-4
        }
        LayerElementUV: 0 {
            MappingInformationType: "ByPolygonVertex"
            ReferenceInformationType: "IndexToDirect"
            UV: *8 {
                a: 0,0,1,0,1,1,0,1
            }
            UVIndex: *4 {
                a: 0,1,2,3
            }
        }
    }
    Model: 2000, "Model::Quad", "Mesh" {
    }
}
Connections:  {
    C: "OO",1000,2000
    C: "OO",2000,0
}
)";

    // Each test gets an empty game folder; the manager resolves paths relative to it
    class ResourceManagerTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            root_ = std::filesystem::temp_directory_path() / "atelier_tests" / info->name();
            std::filesystem::remove_all(root_);
            std::filesystem::create_directories(root_);
            logSink_ = std::make_shared<Engine::Core::MemorySink>(1000);
            Engine::Core::Logger::Get().AddSink(logSink_);
        }

        void TearDown() override
        {
            Engine::Core::Logger::Get().RemoveSink(logSink_);
            std::filesystem::remove_all(root_);
        }

        // Whether a warning mentioning text was logged during this test
        bool LoggedWarningContaining(const std::string& text)
        {
            logSink_->Drain(logged_);
            return std::any_of(logged_.begin(), logged_.end(),
                               [&](const Engine::Core::LogEntry& entry) {
                                   return entry.level == Engine::Core::LogLevel::Warning &&
                                          entry.message.find(text) != std::string::npos;
                               });
        }

        void WriteFile(const std::string& relativePath, const std::vector<std::uint8_t>& bytes) const
        {
            const auto path = root_ / relativePath;
            std::filesystem::create_directories(path.parent_path());
            std::ofstream(path, std::ios::binary)
                .write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }

        void WriteFile(const std::string& relativePath, const std::string& text) const
        {
            WriteFile(relativePath, std::vector<std::uint8_t>(text.begin(), text.end()));
        }

        std::filesystem::path root_;
        std::shared_ptr<Engine::Core::MemorySink> logSink_;
        std::vector<Engine::Core::LogEntry> logged_;
    };
}

// --- Caching and reference counting ---

TEST_F(ResourceManagerTest, SameTextureOnTenObjectsIsReadOnceWithRefCountTen)
{
    WriteFile("assets/textures/crate.png", kPng);
    ResourceManager resources(root_);

    std::vector<TextureHandle> handles;
    for (int i = 0; i < 10; ++i)
    {
        handles.push_back(resources.LoadTexture("assets/textures/crate.png"));
    }

    const auto assets = resources.ListAssets();
    ASSERT_EQ(assets.size(), 1u);
    EXPECT_EQ(assets[0].path, "assets/textures/crate.png");
    EXPECT_EQ(assets[0].type, AssetType::Texture);
    EXPECT_EQ(assets[0].refCount, 10u);
    EXPECT_EQ(assets[0].diskReads, 1u);
    EXPECT_FALSE(assets[0].isPlaceholder);

    for (const auto& handle : handles)
    {
        EXPECT_EQ(resources.Get(handle), resources.Get(handles[0]));
    }
    ASSERT_NE(resources.Get(handles[0]), nullptr);
    EXPECT_EQ(resources.Get(handles[0])->width, 2);
    EXPECT_EQ(resources.Get(handles[0])->height, 2);
}

// Two different spellings of the same path load the file once, with 2 references.
TEST_F(ResourceManagerTest, EquivalentPathsShareOneAsset)
{
    WriteFile("assets/textures/crate.png", kPng);
    ResourceManager resources(root_);

    resources.LoadTexture("assets/textures/crate.png");
    resources.LoadTexture("assets/models/../textures/./crate.png");

    const auto assets = resources.ListAssets();
    ASSERT_EQ(assets.size(), 1u);
    EXPECT_EQ(assets[0].refCount, 2u);
}

// Releasing one of two references keeps the asset; releasing the last one unloads it.
TEST_F(ResourceManagerTest, AssetIsFreedWhenLastReferenceIsReleased)
{
    WriteFile("a.png", kPng);
    ResourceManager resources(root_);

    const auto first = resources.LoadTexture("a.png");
    const auto second = resources.LoadTexture("a.png");

    resources.Release(first);
    ASSERT_EQ(resources.ListAssets().size(), 1u);
    EXPECT_EQ(resources.ListAssets()[0].refCount, 1u);
    EXPECT_NE(resources.Get(second), nullptr);

    resources.Release(second);
    EXPECT_TRUE(resources.ListAssets().empty());
    EXPECT_EQ(resources.Get(second), nullptr);
}

// A handle to a freed asset must not suddenly point at the new file that reused its slot.
TEST_F(ResourceManagerTest, StaleHandleStaysInvalidWhenSlotIsReused)
{
    WriteFile("a.png", kPng);
    WriteFile("b.png", kPng);
    ResourceManager resources(root_);

    const auto a = resources.LoadTexture("a.png");
    resources.Release(a);
    const auto b = resources.LoadTexture("b.png");

    EXPECT_EQ(a.index, b.index);
    EXPECT_EQ(resources.Get(a), nullptr);
    EXPECT_NE(resources.Get(b), nullptr);

    // Releasing through the stale handle must not touch b
    resources.Release(a);
    EXPECT_NE(resources.Get(b), nullptr);
}

// Once unloaded, loading the file again reads it from disk again (fresh slot, 1 disk read).
TEST_F(ResourceManagerTest, ReloadingAfterFreeReadsDiskAgain)
{
    WriteFile("a.png", kPng);
    ResourceManager resources(root_);

    resources.Release(resources.LoadTexture("a.png"));
    resources.LoadTexture("a.png");

    ASSERT_EQ(resources.ListAssets().size(), 1u);
    EXPECT_EQ(resources.ListAssets()[0].diskReads, 1u);
}

// An empty handle gives nullptr, and releasing it is harmless.
TEST_F(ResourceManagerTest, DefaultHandleIsInvalid)
{
    ResourceManager resources(root_);
    EXPECT_EQ(resources.Get(TextureHandle{}), nullptr);
    resources.Release(TextureHandle{});
}

// --- Placeholders ---

TEST_F(ResourceManagerTest, MissingTextureGivesMagentaPlaceholderAndWarning)
{
    ResourceManager resources(root_);
    const auto handle = resources.LoadTexture("assets/textures/missing.png");

    const auto* texture = resources.Get(handle);
    ASSERT_NE(texture, nullptr);
    ASSERT_EQ(texture->rgba.size(), 16u);
    EXPECT_EQ(texture->rgba[0], 255);
    EXPECT_EQ(texture->rgba[1], 0);
    EXPECT_EQ(texture->rgba[2], 255);

    ASSERT_EQ(resources.ListAssets().size(), 1u);
    EXPECT_TRUE(resources.ListAssets()[0].isPlaceholder);
    EXPECT_EQ(resources.ListAssets()[0].diskReads, 0u);
    EXPECT_TRUE(LoggedWarningContaining("assets/textures/missing.png"));
}

// A missing model becomes the placeholder cube (24 vertices, 36 indices) and logs a warning.
TEST_F(ResourceManagerTest, MissingMeshGivesCube)
{
    ResourceManager resources(root_);
    const auto* mesh = resources.Get(resources.LoadMesh("missing.obj"));

    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->indices.size(), 36u);
    EXPECT_EQ(mesh->positions.size(), 24u * 3);
    EXPECT_TRUE(LoggedWarningContaining("missing.obj"));
}

// A file that exists but cannot be read becomes a placeholder with a "Could not read" warning.
TEST_F(ResourceManagerTest, CorruptFileGivesPlaceholder)
{
    WriteFile("broken.png", std::string("not a png"));
    ResourceManager resources(root_);

    resources.LoadTexture("broken.png");

    EXPECT_TRUE(resources.ListAssets()[0].isPlaceholder);
    EXPECT_TRUE(LoggedWarningContaining("Could not read"));
}

// Renaming a texture while it is loaded turns it magenta with a warning; renaming it back loads it again.
TEST_F(ResourceManagerTest, RenamedFileSwapsToPlaceholderAndBack)
{
    WriteFile("crate.png", kPng);
    ResourceManager resources(root_);
    const auto handle = resources.LoadTexture("crate.png");

    std::filesystem::rename(root_ / "crate.png", root_ / "crate_renamed.png");
    resources.CheckFiles();

    EXPECT_TRUE(resources.ListAssets()[0].isPlaceholder);
    EXPECT_EQ(resources.Get(handle)->rgba[1], 0); // magenta
    EXPECT_TRUE(LoggedWarningContaining("crate.png"));

    std::filesystem::rename(root_ / "crate_renamed.png", root_ / "crate.png");
    resources.CheckFiles();

    EXPECT_FALSE(resources.ListAssets()[0].isPlaceholder);
    EXPECT_EQ(resources.ListAssets()[0].diskReads, 2u);
    EXPECT_EQ(resources.ListAssets()[0].refCount, 1u);
}

// Update() only checks the disk once a second has passed, not every frame.
TEST_F(ResourceManagerTest, UpdateChecksFilesAboutOncePerSecond)
{
    WriteFile("crate.png", kPng);
    ResourceManager resources(root_);
    resources.LoadTexture("crate.png");
    std::filesystem::remove(root_ / "crate.png");

    resources.Update(0.5);
    EXPECT_FALSE(resources.ListAssets()[0].isPlaceholder);
    resources.Update(0.6);
    EXPECT_TRUE(resources.ListAssets()[0].isPlaceholder);
}

// --- Loaders ---

TEST_F(ResourceManagerTest, LoadsObjMesh)
{
    WriteFile("crate.obj", kCrateObj);
    ResourceManager resources(root_);
    const auto* mesh = resources.Get(resources.LoadMesh("crate.obj"));

    ASSERT_NE(mesh, nullptr);
    EXPECT_FALSE(resources.ListAssets()[0].isPlaceholder);
    EXPECT_EQ(mesh->indices.size(), 36u);   // 6 quads -> 12 triangles
    EXPECT_EQ(mesh->positions.size(), 72u); // 24 corners
    EXPECT_EQ(mesh->uvs.size(), 48u);
    EXPECT_FLOAT_EQ(mesh->positions[0], -0.35f);
}

// A whole model goes through the same pool as meshes and textures: one disk read shared by reference count,
// and freed when the last reference is released.
TEST_F(ResourceManagerTest, LoadedModelIsSharedAndFreedWhenReleased)
{
    WriteFile("assets/models/crate.obj", kCrateObj);
    ResourceManager resources(root_);

    const auto handle = resources.LoadModel("assets/models/crate.obj");

    const auto* model = resources.Get(handle);
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->name, "crate");
    ASSERT_EQ(model->meshes.size(), 1u);
    EXPECT_EQ(model->meshes[0].indices.size(), 36u); // 6 quads -> 12 triangles
    ASSERT_EQ(model->materials.size(), 1u);

    ASSERT_EQ(resources.ListAssets().size(), 1u);
    EXPECT_EQ(resources.ListAssets()[0].path, "assets/models/crate.obj");
    EXPECT_EQ(resources.ListAssets()[0].type, AssetType::Model);
    EXPECT_EQ(resources.ListAssets()[0].refCount, 1u);
    EXPECT_EQ(resources.ListAssets()[0].diskReads, 1u);
    EXPECT_FALSE(resources.ListAssets()[0].isPlaceholder);

    // A second load of the same path shares the model instead of reading the file again
    const auto second = resources.LoadModel("assets/models/crate.obj");
    EXPECT_EQ(resources.Get(second), resources.Get(handle));
    EXPECT_EQ(resources.ListAssets()[0].refCount, 2u);
    EXPECT_EQ(resources.ListAssets()[0].diskReads, 1u);

    resources.Release(second);
    EXPECT_NE(resources.Get(handle), nullptr);
    EXPECT_EQ(resources.ListAssets()[0].refCount, 1u);

    resources.Release(handle);
    EXPECT_TRUE(resources.ListAssets().empty());
    EXPECT_EQ(resources.Get(handle), nullptr);
}

// A small .fbx quad loads as 2 triangles with positions and UVs.
TEST_F(ResourceManagerTest, LoadsFbxMesh)
{
    WriteFile("quad.fbx", kQuadFbx);
    ResourceManager resources(root_);
    const auto* mesh = resources.Get(resources.LoadMesh("quad.fbx"));

    ASSERT_NE(mesh, nullptr);
    EXPECT_FALSE(resources.ListAssets()[0].isPlaceholder);
    EXPECT_EQ(mesh->indices.size(), 6u); // quad -> 2 triangles
    EXPECT_EQ(mesh->positions.size(), 18u);
    EXPECT_EQ(mesh->uvs.size(), 12u);
    for (const float coordinate : mesh->positions)
    {
        EXPECT_TRUE(std::abs(coordinate) == 1.0f || coordinate == 0.0f) << coordinate;
    }
}

// A broken .fbx becomes the placeholder cube.
TEST_F(ResourceManagerTest, CorruptFbxGivesPlaceholderCube)
{
    WriteFile("broken.fbx", std::string("Objects: { Geometry: 1, \"Mesh\" {"));
    ResourceManager resources(root_);
    const auto* mesh = resources.Get(resources.LoadMesh("broken.fbx"));

    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->indices.size(), 36u);
    EXPECT_TRUE(resources.ListAssets()[0].isPlaceholder);
}

// A .jpg loads too, as an 8x8 RGBA image of the right colour.
TEST_F(ResourceManagerTest, LoadsJpgTexture)
{
    WriteFile("grey.jpg", MakeGreyJpeg());
    ResourceManager resources(root_);
    const auto* texture = resources.Get(resources.LoadTexture("grey.jpg"));

    ASSERT_NE(texture, nullptr);
    EXPECT_FALSE(resources.ListAssets()[0].isPlaceholder);
    EXPECT_EQ(texture->width, 8);
    EXPECT_EQ(texture->height, 8);
    ASSERT_EQ(texture->rgba.size(), 8u * 8u * 4u);
    // Greyscale expands to RGB; allow 1 step of rounding from the inverse DCT
    EXPECT_NEAR(texture->rgba[0], 200, 1);
    EXPECT_NEAR(texture->rgba[1], 200, 1);
    EXPECT_NEAR(texture->rgba[2], 200, 1);
    EXPECT_EQ(texture->rgba[3], 255);
}

// A face that points at a vertex that does not exist is rejected.
TEST(ObjParser, RejectsOutOfRangeIndex)
{
    EXPECT_FALSE(Engine::Assets::ParseObj("v 0 0 0\nf 1 2 3\n").has_value());
}

// Negative (count-from-the-end) indices and the v//vn face format both work.
TEST(ObjParser, SupportsNegativeIndicesAndNormals)
{
    const auto mesh = Engine::Assets::ParseObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf -3//1 -2//1 -1//1\n");
    ASSERT_TRUE(mesh.has_value());
    EXPECT_EQ(mesh->indices.size(), 3u);
    EXPECT_FLOAT_EQ(mesh->positions[3], 1.0f);
}

// A stereo 16-bit .wav loads with its channel count, sample rate and samples.
TEST_F(ResourceManagerTest, LoadsWavAudio)
{
    WriteFile("beep.wav", MakeWav(2, 22050, {100, -100, 200, -200}));
    ResourceManager resources(root_);
    const auto* clip = resources.Get(resources.LoadAudio("beep.wav"));

    ASSERT_NE(clip, nullptr);
    EXPECT_FALSE(resources.ListAssets()[0].isPlaceholder);
    EXPECT_EQ(clip->channels, 2);
    EXPECT_EQ(clip->sampleRate, 22050);
    EXPECT_EQ(clip->samples, (std::vector<std::int16_t>{100, -100, 200, -200}));
}

// data/click.ogg: "click_002" from Kenney's Interface Sounds (CC0), mono 44.1 kHz Vorbis, 442 frames
TEST(OggLoader, DecodesVorbisFile)
{
    ResourceManager resources(ATELIER_TEST_DATA_DIR);
    const auto* clip = resources.Get(resources.LoadAudio("click.ogg"));

    ASSERT_NE(clip, nullptr);
    ASSERT_FALSE(resources.ListAssets()[0].isPlaceholder);
    EXPECT_EQ(clip->channels, 1);
    EXPECT_EQ(clip->sampleRate, 44100);
    EXPECT_EQ(clip->samples.size(), 442u);
    EXPECT_TRUE(std::any_of(clip->samples.begin(), clip->samples.end(), [](std::int16_t s) { return s != 0; }));
}

// Only 16-bit PCM is supported; an 8-bit wav is rejected.
TEST(WavParser, RejectsNonPcm16)
{
    auto bytes = MakeWav(1, 8000, {1, 2});
    bytes[34] = 8; // bits per sample
    EXPECT_FALSE(Engine::Assets::ParseWav(bytes).has_value());
}

// LoadShader("x.vert") also reads x.frag next to it.
TEST_F(ResourceManagerTest, LoadsShaderPair)
{
    WriteFile("unlit.vert", std::string("void main() {}"));
    WriteFile("unlit.frag", std::string("void main() { gl_FragColor = vec4(1.0); }"));
    ResourceManager resources(root_);
    const auto* shader = resources.Get(resources.LoadShader("unlit.vert"));

    ASSERT_NE(shader, nullptr);
    EXPECT_EQ(shader->vertexSource, "void main() {}");
    EXPECT_NE(shader->fragmentSource.find("gl_FragColor"), std::string::npos);
}

// A .vert with no .frag is not usable, so the placeholder shader is used instead.
TEST_F(ResourceManagerTest, ShaderWithoutFragmentIsPlaceholder)
{
    WriteFile("unlit.vert", std::string("void main() {}"));
    ResourceManager resources(root_);
    const auto* shader = resources.Get(resources.LoadShader("unlit.vert"));

    ASSERT_NE(shader, nullptr);
    // The stand-in unlit shader, not half of the file's (a .vert with no .frag is not a usable shader)
    EXPECT_EQ(shader->vertexSource, Engine::Assets::PlaceholderShader().vertexSource);
    EXPECT_NE(shader->vertexSource, "void main() {}");
    EXPECT_TRUE(resources.ListAssets()[0].isPlaceholder);
}
