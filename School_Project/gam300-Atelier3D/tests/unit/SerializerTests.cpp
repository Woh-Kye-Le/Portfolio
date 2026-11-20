#include "engine/serializer/CfgSerializer.hpp"
#include "engine/serializer/JsonSerializer.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

using Engine::Serializer::CFGSerializer;
using Engine::Serializer::JSONSerializer;

namespace
{
    // Gives each test an empty folder for its files
    class SerializerTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            dir_ = std::filesystem::temp_directory_path() / "atelier_serializer_tests" / info->name();
            std::filesystem::remove_all(dir_);
            std::filesystem::create_directories(dir_);
        }

        void TearDown() override { std::filesystem::remove_all(dir_); }

        std::string PathOf(const std::string& name) const { return (dir_ / name).string(); }

        std::filesystem::path dir_;
    };
}

// --- CFGSerializer ---

TEST_F(SerializerTest, CfgRoundTrip)
{
    {
        CFGSerializer out;
        out.NewFile(PathOf("settings.cfg"));
        out.WriteString("name", "Atelier");
        out.WriteInt("count", 3);
        out.WriteFloat("scale", 2.5f);
        out.WriteBool("enabled", true);
        out.WriteBool("render.vsync", false); // "section.key" writes into [render]
    }

    CFGSerializer in;
    in.LoadFile(PathOf("settings.cfg"));

    std::string name;
    int count = 0;
    float scale = 0.0f;
    bool enabled = false;
    bool vsync = true;
    in.ReadString("name", name);
    in.ReadInt("count", count);
    in.ReadFloat("scale", scale);
    in.ReadBool("enabled", enabled);
    in.ReadBool("render.vsync", vsync);

    EXPECT_EQ(name, "Atelier");
    EXPECT_EQ(count, 3);
    EXPECT_FLOAT_EQ(scale, 2.5f);
    EXPECT_TRUE(enabled);
    EXPECT_FALSE(vsync);
    EXPECT_TRUE(in.HasSection("render"));
}

// CFG files: ; and # comments are skipped, [sections] and keys ignore case, quotes are taken off.
TEST_F(SerializerTest, CfgSectionsCommentsAndQuotes)
{
    std::ofstream(PathOf("game.cfg")) << "; comment\n# comment\n[Window]\nTitle = \"My Game\"\nWidth=800\n";

    CFGSerializer cfg;
    cfg.LoadFile(PathOf("game.cfg"));
    cfg.EnterSection("window"); // section and key names are case-insensitive

    std::string title;
    int width = 0;
    cfg.ReadString("title", title);
    cfg.ReadInt("width", width);

    EXPECT_EQ(title, "My Game");
    EXPECT_EQ(width, 800);
    EXPECT_TRUE(cfg.HasKey("width"));
    EXPECT_FALSE(cfg.HasKey("height"));
}

// yes/on/1 read as true and no/off/0 as false.
TEST_F(SerializerTest, CfgBoolAcceptsCommonSpellings)
{
    std::ofstream(PathOf("flags.cfg")) << "a=yes\nb=On\nc=1\nd=no\ne=off\nf=0\n";

    CFGSerializer cfg;
    cfg.LoadFile(PathOf("flags.cfg"));

    for (const char* key : {"a", "b", "c"})
    {
        bool value = false;
        cfg.ReadBool(key, value);
        EXPECT_TRUE(value) << key;
    }
    for (const char* key : {"d", "e", "f"})
    {
        bool value = true;
        cfg.ReadBool(key, value);
        EXPECT_FALSE(value) << key;
    }
}

// A missing key or a value that is not a number reads as 0.
TEST_F(SerializerTest, CfgMissingOrInvalidValuesReadAsZero)
{
    std::ofstream(PathOf("bad.cfg")) << "width=wide\n";

    CFGSerializer cfg;
    cfg.LoadFile(PathOf("bad.cfg"));

    int width = 99;
    int height = 99;
    cfg.ReadInt("width", width);
    cfg.ReadInt("height", height);
    EXPECT_EQ(width, 0);
    EXPECT_EQ(height, 0);
}

// --- JSONSerializer ---

TEST_F(SerializerTest, JsonRoundTripWithNestedObjectAndArray)
{
    {
        JSONSerializer out;
        out.NewFile(PathOf("data.json"));
        out.WriteString("name", "Atelier");
        out.WriteInt("version", 2);
        out.WriteFloat("scale", 1.5f);
        out.WriteBool("enabled", true);
        out.CreateObject("window");
        out.EnterObject("window");
        out.WriteInt("width", 800);
        out.LeaveObject();
        out.CreateArray("tags");
        out.EnterArray("tags");
        out.WriteString("editor");
        out.WriteString("engine");
        out.LeaveArray();
        out.SaveFile(PathOf("data.json"));
    }

    JSONSerializer in;
    in.LoadFile(PathOf("data.json"));

    std::string name;
    int version = 0;
    float scale = 0.0f;
    bool enabled = false;
    in.ReadString("name", name);
    in.ReadInt("version", version);
    in.ReadFloat("scale", scale);
    in.ReadBool("enabled", enabled);
    EXPECT_EQ(name, "Atelier");
    EXPECT_EQ(version, 2);
    EXPECT_FLOAT_EQ(scale, 1.5f);
    EXPECT_TRUE(enabled);
    EXPECT_EQ(in.GetDataType("version"), JSONSerializer::INT);
    EXPECT_EQ(in.GetDataType("name"), JSONSerializer::STRING);

    int width = 0;
    in.EnterObject("window");
    in.ReadInt("width", width);
    in.LeaveObject();
    EXPECT_EQ(width, 800);

    std::string first;
    std::string second;
    in.EnterArray("tags");
    in.ReadString(first);
    in.ReadString(second);
    in.LeaveArray();
    EXPECT_EQ(first, "editor");
    EXPECT_EQ(second, "engine");
}

// The plain Read* functions give 0 / "" for keys that are not there.
TEST_F(SerializerTest, JsonMissingKeysReadAsDefaults)
{
    JSONSerializer json;
    json.ParseJSON("{}");

    int i = 7;
    std::string s = "x";
    json.ReadInt("missing", i);
    json.ReadString("missing", s);
    EXPECT_EQ(i, 0);
    EXPECT_EQ(s, "");
    EXPECT_FALSE(json.HasKey("missing"));
}

// Loading a file that does not exist throws.
TEST_F(SerializerTest, JsonLoadingMissingFileThrows)
{
    JSONSerializer json;
    EXPECT_THROW(json.LoadFile(PathOf("missing.json")), std::runtime_error);
}

// TryRead* only succeeds (and only changes the value) when the key exists and has the right type - the scene loader
// uses this to report bad fields.
TEST_F(SerializerTest, JsonTryReadOnlyReadsValuesOfTheRightType)
{
    JSONSerializer json;
    json.ParseJSON(R"({ "speed": 2.5, "count": 3, "on": true, "name": "crate", "list": [], "child": {} })");

    float speed = 1.0f;
    int count = 0;
    bool on = false;
    std::string name;
    EXPECT_TRUE(json.TryReadFloat("speed", speed));
    EXPECT_TRUE(json.TryReadInt("count", count));
    EXPECT_TRUE(json.TryReadBool("on", on));
    EXPECT_TRUE(json.TryReadString("name", name));
    EXPECT_FLOAT_EQ(speed, 2.5f);
    EXPECT_EQ(count, 3);
    EXPECT_TRUE(on);
    EXPECT_EQ(name, "crate");

    // Missing or another type: false, and the value keeps what it had (no exception)
    float missing = 7.0f;
    int notWhole = 9;
    std::string notText = "keep";
    EXPECT_FALSE(json.TryReadFloat("nothing", missing));
    EXPECT_FALSE(json.TryReadInt("speed", notWhole));
    EXPECT_FALSE(json.TryReadString("count", notText));
    EXPECT_FLOAT_EQ(missing, 7.0f);
    EXPECT_EQ(notWhole, 9);
    EXPECT_EQ(notText, "keep");

    EXPECT_TRUE(json.HasArray("list"));
    EXPECT_FALSE(json.HasArray("child"));
    EXPECT_TRUE(json.HasObject("child"));
    EXPECT_FALSE(json.HasObject("nothing"));
}
