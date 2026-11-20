/* !
@file    	    BlockPreview.cpp
@author  	    j.cai@digipen.edu [100%]
@date    		02/11/2026
@brief
    Draws the block previews in the blocks' tooltips: the object (or Atti, a
    yellow cube) acting the block out in 3D on a little floor, seen from high
    up (moving and turning) or from low down (jumping, looks, health, loops,
    events, sensing); the game's screen with what the block puts on it, over
    the object standing in the game; or a sum or condition worked out. A
    script (ScriptPreview.hpp) plays the same way, block after block, or as
    its variables changing when it does nothing to the object. The 3D is
    rendered into a framebuffer of its own, then the words, keys and bars are
    drawn over it. It holds still when Windows' "Show animations" is off.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Shader.hpp brings in GLEW, which must come before anything that includes GL/gl.h
#include "engine/graphics/Shader/Shader.hpp"

#include "editor/panels/blocks/BlockPreview.hpp"

#include "editor/panels/blocks/ScriptPreview.hpp"

#include "editor/SavedGlState.hpp"
#include "editor/Theme.hpp"

#include "engine/assets/Animation.hpp"
#include "engine/assets/loaders/MeshLoader.hpp"
#include "engine/assets/loaders/ModelLoader.hpp"
#include "engine/assets/loaders/TextureLoader.hpp"
#include "engine/core/Log.hpp"
#include "engine/graphics/Camera/Camera3D.hpp"
#include "engine/graphics/Material/Material.hpp"
#include "engine/graphics/Mesh/Mesh.hpp"
#include "engine/graphics/Model/Model.hpp"
#include "engine/graphics/OpenGL/FramebufferGL.hpp"
#include "engine/graphics/OpenGL/TextureGL.hpp"
#include "engine/graphics/Renderer/MeshRenderer.hpp"
#include "engine/graphics/Renderer/RenderAPI.hpp"
#include "engine/scene/SceneDescription.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui/imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <string>
#include <unordered_map>

namespace Editor::Panels
{
    namespace
    {
        using Engine::Graphics::Camera3D;
        using Engine::Graphics::Mesh;
        using Engine::Logic::Op;

        constexpr float kPreviewFont = 15.0f; // its words: bold, and as big whatever the canvas is zoomed to
        constexpr int kSupersample = 2;       // rendered at twice the size, so its edges are smooth when drawn at 1x

        // ---- The 3D stage, in the game's units
        constexpr float kActor = 1.4f;     // the object's size across at size 1, whatever its own size
        constexpr float kRoomX = 2.4f;     // how far from the middle it can be seen, across...
        constexpr float kRoomZ = 1.8f;     // ...and, from high up, back and forth
        constexpr float kCeiling = 1.5f;   // and how high, from low down
        constexpr float kFromLeft = -1.6f; // where it starts when it goes up to something on the right
        constexpr float kFieldOfView = 35.0f;
        // The cameras: from high up and a little to the right (forward, -Z, is up the screen), and from low down
        struct CameraSpot
        {
            glm::vec3 target;
            float elevation; // degrees above the floor
            float yaw;       // degrees round to the right of straight in front (+Z)
            float distance;
        };
        constexpr CameraSpot kHighUp{glm::vec3(0.0f, 0.0f, 0.0f), 50.0f, 10.0f, 7.0f};
        constexpr CameraSpot kLowDown{glm::vec3(0.0f, 1.3f, 0.0f), 10.0f, 18.0f, 5.0f};

        // The sky's colours before a block changes them: the "set sky ... color" blocks' defaults
        constexpr std::array<ImVec4, 3> kSky{ImVec4(0.2f, 0.38f, 0.68f, 1.0f), ImVec4(0.64f, 0.74f, 0.86f, 1.0f),
                                             ImVec4(0.22f, 0.23f, 0.25f, 1.0f)};
        constexpr ImVec4 kGameScreen = Theme::Hex(0x2A3150); // the game's screen, behind what is put on it
        constexpr ImVec4 kWhite = Theme::Hex(0xFFFFFF);

        // Lit by a key light and a fill light, with the colour blocks' tint, see-through for "hide", the floor's
        // checks fading out at its edge, and the haze over what is far away. Skinned as the engine's own shader.
        constexpr const char* kVertexShader = R"(#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 4) in vec4 a_Joints;
layout(location = 5) in vec4 a_Weights;
uniform mat4 u_Model;
uniform mat4 u_ViewProjection;
uniform mat3 u_NormalMatrix;
const int kMaxBones = 128; // Engine::Assets::kMaxBones
uniform int u_Skinned;
uniform mat4 u_Bones[kMaxBones];
out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoord;
void main()
{
    vec4 position = vec4(a_Position, 1.0);
    vec3 normal = a_Normal;
    float total = a_Weights.x + a_Weights.y + a_Weights.z + a_Weights.w;
    if (u_Skinned == 1 && total > 0.0)
    {
        mat4 skin = a_Weights.x * u_Bones[min(int(a_Joints.x), kMaxBones - 1)] +
                    a_Weights.y * u_Bones[min(int(a_Joints.y), kMaxBones - 1)] +
                    a_Weights.z * u_Bones[min(int(a_Joints.z), kMaxBones - 1)] +
                    a_Weights.w * u_Bones[min(int(a_Joints.w), kMaxBones - 1)];
        position = skin * position;
        normal = mat3(skin) * normal;
    }
    vec4 world = u_Model * position;
    v_WorldPos = world.xyz;
    v_Normal = normalize(u_NormalMatrix * normal);
    v_TexCoord = a_TexCoord;
    gl_Position = u_ViewProjection * world;
}
)";

        constexpr const char* kFragmentShader = R"(#version 330 core
in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;
uniform vec4 u_AlbedoTint;
uniform int u_Textured;
uniform sampler2D u_Texture;
uniform vec4 u_Blend;  // rgb: the colour it is tinted towards, a: how far
uniform float u_Alpha; // see-through
uniform int u_Floor;   // the floor: checks a unit wide, fading out at its edge
uniform vec3 u_Eye;
uniform vec4 u_Fog;    // rgb: the haze's colour, a: how thick
out vec4 o_Color;
void main()
{
    vec4 albedo = u_AlbedoTint;
    if (u_Textured == 1)
    {
        albedo *= texture(u_Texture, v_TexCoord);
        if (albedo.a < 0.1)
            discard; // leaves and fences
    }
    if (u_Floor == 1)
    {
        vec2 cell = floor(v_WorldPos.xz);
        albedo.rgb *= mix(0.9, 1.0, mod(cell.x + cell.y, 2.0));
        albedo.a *= 1.0 - smoothstep(4.0, 6.0, length(v_WorldPos.xz));
    }
    vec3 n = normalize(gl_FrontFacing ? v_Normal : -v_Normal);
    float key = max(dot(n, normalize(vec3(0.5, 0.8, 0.6))), 0.0);
    float fill = max(dot(n, normalize(vec3(-0.6, 0.2, -0.4))), 0.0);
    vec3 colour = mix(albedo.rgb, u_Blend.rgb, u_Blend.a) * (0.35 + 0.65 * key + 0.2 * fill);
    colour = mix(colour, u_Fog.rgb, u_Fog.a * smoothstep(10.0, 16.0, distance(v_WorldPos, u_Eye)));
    o_Color = vec4(colour, albedo.a * u_Alpha);
}
)";

        ImU32 Colour(ImVec4 colour, float alpha = 1.0f)
        {
            return ImGui::GetColorU32(ImVec4(colour.x, colour.y, colour.z, colour.w * alpha));
        }

        glm::vec4 Vec(ImVec4 colour)
        {
            return glm::vec4(colour.x, colour.y, colour.z, colour.w);
        }

        ImVec4 Mix(ImVec4 from, const std::array<float, 3>& to, float amount)
        {
            return ImVec4(from.x + (to[0] - from.x) * amount, from.y + (to[1] - from.y) * amount,
                          from.z + (to[2] - from.z) * amount, from.w);
        }

        std::string NameOr(const std::string& name, const char* otherwise)
        {
            return name.empty() ? otherwise : name;
        }

        float Radians(float degrees)
        {
            return degrees * std::numbers::pi_v<float> / 180.0f;
        }

        // Which way it faces on the floor: direction 0 is -Z and 90 is -X, as in Play
        glm::vec3 Front(float facing)
        {
            const float r = Radians(facing);
            return glm::vec3(-std::sin(r), 0.0f, -std::cos(r));
        }

        // An arrow from tail to tip, filled in fill and outlined in ink
        void DrawArrow(ImDrawList* drawList, ImVec2 tail, ImVec2 tip, ImVec4 fill, float scale, float alpha)
        {
            const float dx = tip.x - tail.x;
            const float dy = tip.y - tail.y;
            const float length = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
            const ImVec2 along(dx / length, dy / length);
            const ImVec2 side(-along.y * 5.0f * scale, along.x * 5.0f * scale);
            const ImVec2 base(tip.x - along.x * 8.0f * scale, tip.y - along.y * 8.0f * scale);
            drawList->AddLine(tail, base, Colour(Theme::kInk, alpha), 2.5f * scale);
            const ImVec2 a(base.x + side.x, base.y + side.y);
            const ImVec2 b(base.x - side.x, base.y - side.y);
            drawList->AddTriangleFilled(tip, a, b, Colour(fill, alpha));
            drawList->AddTriangle(tip, a, b, Colour(Theme::kInk, alpha), 1.5f * scale);
        }

        // A dashed line from a to b
        void DrawDashes(ImDrawList* drawList, ImVec2 a, ImVec2 b, ImVec4 colour, float scale)
        {
            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            const float length = std::sqrt(dx * dx + dy * dy);
            const float step = 9.0f * scale;
            const int dashes = static_cast<int>(std::ceil(length / step));
            for (int dash = 0; dash < dashes; ++dash)
            {
                const float d = static_cast<float>(dash) * step;
                const float to = std::min(length, d + step * 0.55f);
                drawList->AddLine(ImVec2(a.x + dx * d / length, a.y + dy * d / length),
                                  ImVec2(a.x + dx * to / length, a.y + dy * to / length), Colour(colour), 2.0f * scale);
            }
        }

        // Text centred on a point
        void TextAt(ImDrawList* drawList, ImVec2 centre, const std::string& text, ImVec4 colour, float alpha = 1.0f)
        {
            const ImVec2 size = ImGui::CalcTextSize(text.c_str());
            drawList->AddText(ImVec2(centre.x - size.x * 0.5f, centre.y - size.y * 0.5f), Colour(colour, alpha),
                              text.c_str());
        }

        // A rounded label on a fill, outlined in ink; returns its right edge
        float Pill(ImDrawList* drawList, ImVec2 at, const std::string& text, ImVec4 fill, float scale)
        {
            const ImVec2 size = ImGui::CalcTextSize(text.c_str());
            const float pad = 5.0f * scale;
            const ImVec2 max(at.x + size.x + pad * 2.0f, at.y + size.y + pad);
            drawList->AddRectFilled(at, max, Colour(fill), 5.0f * scale);
            drawList->AddRect(at, max, Colour(Theme::kInk), 5.0f * scale, ImDrawFlags_None, 1.5f * scale);
            drawList->AddText(ImVec2(at.x + pad, at.y + pad * 0.5f), Colour(Theme::kText), text.c_str());
            return max.x;
        }

        // A name over the 3D, on a soft label so it reads on the floor and the sky alike
        void Label(ImDrawList* drawList, ImVec2 centre, const std::string& text, float scale)
        {
            const ImVec2 size = ImGui::CalcTextSize(text.c_str());
            const ImVec2 pad(4.0f * scale, 1.0f * scale);
            drawList->AddRectFilled(ImVec2(centre.x - size.x * 0.5f - pad.x, centre.y - size.y * 0.5f - pad.y),
                                    ImVec2(centre.x + size.x * 0.5f + pad.x, centre.y + size.y * 0.5f + pad.y),
                                    Colour(Theme::kHeader), 4.0f * scale);
            TextAt(drawList, centre, text, Theme::kTextSoft);
        }

        // A condition's answer as a chip that says it (not only by colour): mint true, red false
        float AnswerChip(ImDrawList* drawList, ImVec2 at, bool answer, float scale)
        {
            return Pill(drawList, at, answer ? "true" : "false", answer ? Theme::kRun : Theme::kError, scale);
        }

        // The readout and answer in the top-left corner, lit up just after it changes
        void DrawReadout(ImDrawList* drawList, ImVec2 at, const PreviewPose& pose, float scale)
        {
            float x = at.x;
            if (!pose.readout.empty())
            {
                x = Pill(drawList, at, pose.readout, pose.changed ? Theme::kRaisedHigh : Theme::kPanel, scale) +
                    4.0f * scale;
            }
            if (pose.answer)
            {
                AnswerChip(drawList, ImVec2(x, at.y), *pose.answer, scale);
            }
        }

        // The bar filling up under the readout, for what is waiting
        void DrawProgress(ImDrawList* drawList, ImVec2 min, ImVec2 max, const PreviewPose& pose, float scale)
        {
            if (pose.progress < 0.0f)
            {
                return;
            }
            const ImVec2 barMin(min.x + 8.0f * scale, min.y + 32.0f * scale);
            const ImVec2 barMax(max.x - 8.0f * scale, barMin.y + 6.0f * scale);
            drawList->AddRectFilled(barMin, barMax, Colour(Theme::kPanel), 3.0f * scale);
            drawList->AddRectFilled(barMin, ImVec2(barMin.x + (barMax.x - barMin.x) * pose.progress, barMax.y),
                                    Colour(Theme::kLoops), 3.0f * scale);
            drawList->AddRect(barMin, barMax, Colour(Theme::kInk), 3.0f * scale, ImDrawFlags_None, 1.5f * scale);
        }

        // A key on the keyboard, sunk in while pressed
        void DrawKey(ImDrawList* drawList, ImVec2 at, const std::string& key, bool down, float scale)
        {
            const ImVec2 size = ImGui::CalcTextSize(key.c_str());
            const ImVec2 max(at.x + std::max(size.x + 14.0f * scale, 28.0f * scale), at.y + size.y + 10.0f * scale);
            const ImVec2 offset(0.0f, down ? 3.0f * scale : 0.0f);
            drawList->AddRectFilled(ImVec2(at.x, at.y + 3.0f * scale), ImVec2(max.x, max.y + 3.0f * scale),
                                    Colour(Theme::kInk), 5.0f * scale);
            drawList->AddRectFilled(ImVec2(at.x + offset.x, at.y + offset.y), ImVec2(max.x, max.y + offset.y),
                                    Colour(down ? Theme::kRaisedHigh : Theme::kPanel), 5.0f * scale);
            drawList->AddRect(ImVec2(at.x, at.y + offset.y), ImVec2(max.x, max.y + offset.y), Colour(Theme::kInk),
                              5.0f * scale, ImDrawFlags_None, 1.5f * scale);
            TextAt(drawList, ImVec2((at.x + max.x) * 0.5f, (at.y + max.y) * 0.5f + offset.y), key, Theme::kText);
        }

        // The editor's Run button, pressed in for "when Play starts"
        void DrawRun(ImDrawList* drawList, ImVec2 at, bool pressed, float scale)
        {
            const ImVec2 max(at.x + 52.0f * scale, at.y + 22.0f * scale);
            const float sink = pressed ? 2.0f * scale : 0.0f;
            drawList->AddRectFilled(ImVec2(at.x + 2.0f * scale, at.y + 2.0f * scale),
                                    ImVec2(max.x + 2.0f * scale, max.y + 2.0f * scale), Colour(Theme::kInk),
                                    6.0f * scale);
            drawList->AddRectFilled(ImVec2(at.x + sink, at.y + sink), ImVec2(max.x + sink, max.y + sink),
                                    Colour(Theme::kRun), 6.0f * scale);
            drawList->AddRect(ImVec2(at.x + sink, at.y + sink), ImVec2(max.x + sink, max.y + sink), Colour(Theme::kInk),
                              6.0f * scale, ImDrawFlags_None, 1.5f * scale);
            Theme::DrawIcon(drawList, Theme::Icon::Play,
                            ImVec2(at.x + 12.0f * scale + sink, at.y + 11.0f * scale + sink), 12.0f * scale,
                            Colour(Theme::kInk));
            TextAt(drawList, ImVec2(at.x + 34.0f * scale + sink, at.y + 11.0f * scale + sink), "Run", Theme::kText);
        }

        // A message: a little envelope with its name over it
        void DrawEnvelope(ImDrawList* drawList, ImVec2 centre, const std::string& message, float scale)
        {
            const ImVec2 half(10.0f * scale, 7.0f * scale);
            const ImVec2 min(centre.x - half.x, centre.y - half.y);
            const ImVec2 max(centre.x + half.x, centre.y + half.y);
            drawList->AddRectFilled(min, max, Colour(Theme::kPanel), 2.0f * scale);
            drawList->AddRect(min, max, Colour(Theme::kInk), 2.0f * scale, ImDrawFlags_None, 1.5f * scale);
            drawList->AddLine(min, centre, Colour(Theme::kInk), 1.5f * scale);
            drawList->AddLine(ImVec2(max.x, min.y), centre, Colour(Theme::kInk), 1.5f * scale);
            Label(drawList, ImVec2(centre.x, min.y - 9.0f * scale), "\"" + message + "\"", scale);
        }

        void DrawHealthBar(ImDrawList* drawList, ImVec2 centre, float health, float scale, float alpha)
        {
            const ImVec2 half(22.0f * scale, 3.5f * scale);
            const ImVec2 min(centre.x - half.x, centre.y - half.y);
            const ImVec2 max(centre.x + half.x, centre.y + half.y);
            const ImVec4 fill = health > 0.5f ? Theme::kRun : health > 0.25f ? Theme::kEvents : Theme::kError;
            drawList->AddRectFilled(min, max, Colour(Theme::kInk, alpha), 2.0f * scale);
            drawList->AddRectFilled(ImVec2(min.x + scale, min.y + scale),
                                    ImVec2(min.x + scale + (max.x - min.x - 2.0f * scale) * health, max.y - scale),
                                    Colour(fill, alpha), 2.0f * scale);
        }

        void DrawSpeech(ImDrawList* drawList, ImVec2 from, const std::string& said, float scale)
        {
            const std::string text = said.size() > 28 ? said.substr(0, 27) + "..." : said;
            const ImVec2 size = ImGui::CalcTextSize(text.c_str());
            const float pad = 5.0f * scale;
            const ImVec2 min(from.x + 6.0f * scale, from.y - size.y - pad * 2.0f - 6.0f * scale);
            const ImVec2 max(min.x + size.x + pad * 2.0f, min.y + size.y + pad * 2.0f);
            drawList->AddTriangleFilled(from, ImVec2(min.x + 4.0f * scale, max.y), ImVec2(min.x + 14.0f * scale, max.y),
                                        Colour(Theme::kInk));
            drawList->AddRectFilled(min, max, Colour(Theme::kPanel), 6.0f * scale);
            drawList->AddRect(min, max, Colour(Theme::kInk), 6.0f * scale, ImDrawFlags_None, 1.5f * scale);
            drawList->AddText(ImVec2(min.x + pad, min.y + pad), Colour(Theme::kText), text.c_str());
        }

        // Sound waves: three arcs out of a point, to the right
        void DrawWaves(ImDrawList* drawList, ImVec2 from, float scale)
        {
            const float pulse = static_cast<float>(std::fmod(ImGui::GetTime() * 2.0, 1.0));
            for (int i = 0; i < 3; ++i)
            {
                const float radius = (6.0f + 6.0f * (static_cast<float>(i) + pulse)) * scale;
                drawList->PathArcTo(from, radius, -0.6f, 0.6f, 10);
                drawList->PathStroke(Colour(Theme::kInk, 1.0f - (static_cast<float>(i) + pulse) / 3.0f),
                                     ImDrawFlags_None, 2.0f * scale);
            }
        }

        // The framebuffer holds colours already multiplied by their alpha (blended onto a see-through clear), so
        // ImGui draws it with the blend for that, then puts its own back (the editor's ImGui renderer does not know
        // ImDrawCallback_ResetRenderState)
        void PremultipliedBlend(const ImDrawList*, const ImDrawCmd*)
        {
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }

        void StraightBlend(const ImDrawList*, const ImDrawCmd*)
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        // ---- Where things are on the 3D stage

        // The camera at spot, for a picture of this shape
        Camera3D CameraAt(const CameraSpot& spot, float aspect)
        {
            const float elevation = Radians(spot.elevation);
            const float yaw = Radians(spot.yaw);
            const glm::vec3 back(std::sin(yaw) * std::cos(elevation), std::sin(elevation),
                                 std::cos(yaw) * std::cos(elevation));
            Camera3D camera;
            camera.SetPerspective(kFieldOfView, aspect, 0.5f, 40.0f);
            camera.SetPosition(spot.target + back * spot.distance);
            camera.SetLookAt(spot.target);
            return camera;
        }

        // Where the object stands, how it is turned and how big it is
        struct Placement
        {
            glm::vec3 at{0.0f};
            float facing = 0.0f; // degrees round Y, as "point in direction"
            float tilt = 0.0f;   // its front tipped up, degrees
            float size = 1.0f;

            glm::mat4 Matrix() const
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), at);
                m = glm::rotate(m, Radians(facing), glm::vec3(0.0f, 1.0f, 0.0f));
                m = glm::rotate(m, Radians(tilt), glm::vec3(1.0f, 0.0f, 0.0f));
                return glm::scale(m, glm::vec3(size));
            }
        };

        // Seen from low down: things that go up to something start on the left; those that go across, or tip up,
        // face right (forward); the rest face the camera
        bool StartsLeft(Op op)
        {
            return op == Op::PushForward || op == Op::PushBy || op == Op::Touching || op == Op::DistanceTo;
        }

        Placement PlaceLowDown(const Engine::Logic::Block& block, const PreviewPose& pose)
        {
            const bool facesRight =
                StartsLeft(block.op) || pose.tilt || block.op == Op::IfElse || block.op == Op::PlayFootstep;
            Placement place;
            place.at = glm::vec3(std::clamp((StartsLeft(block.op) ? kFromLeft : 0.0f) + pose.x, -kRoomX, kRoomX),
                                 std::clamp(pose.y, 0.0f, kCeiling), 0.0f);
            place.facing = facesRight ? -90.0f : kLowDown.yaw + 180.0f;
            place.tilt = pose.tilt.value_or(0.0f);
            place.size = std::clamp(pose.size, 0.2f, 2.2f);
            return place;
        }

        // From high up, x and y are on the floor; offset centres the way from start to end (a whole number of
        // squares, so the floor's checks stay under the same places), and what is out of reach waits at the edge
        glm::vec3 OnFloor(float x, float y, glm::vec2 offset)
        {
            return glm::vec3(std::clamp(x + offset.x, -kRoomX, kRoomX), 0.0f,
                             std::clamp(y + offset.y, -kRoomZ, kRoomZ));
        }

        Placement PlaceHighUp(const PreviewPose& pose, glm::vec2 offset)
        {
            Placement place;
            place.at = OnFloor(pose.x, pose.y, offset);
            place.facing = pose.facing;
            place.size = std::clamp(pose.size, 0.2f, 2.2f);
            return place;
        }
    }

    // ---- The GPU side: the framebuffer, the shader, the stage's meshes and the object last shown
    struct BlockPreviewer::Gpu
    {
        std::unique_ptr<Engine::Graphics::Shader> shader; // nullptr: it did not compile, and the 3D is left out
        Engine::Graphics::OpenGL::FramebufferGL target;
        Mesh floor = Mesh::CreatePlane(12.0f);
        Mesh cube = Mesh::CreateCube(1.0f);
        Mesh disc = Mesh::CreateCircle(1.0f, 32);
        Mesh hill = Mesh::CreatePyramid(0.5f, 0.0f, 1.0f);

        // The look last asked for, and what it draws: its whole model, or one mesh (its file's, or its built-in
        // shape's) with its texture and tint. fit stands it on the floor at the origin, kActor across; height is
        // how tall it then is.
        std::optional<Engine::Scene::RenderableDesc> look;
        std::filesystem::path lookDir;
        std::unique_ptr<Engine::Graphics::Model> model;
        std::optional<Mesh> mesh;
        Engine::Graphics::PrimitiveType primitive = Engine::Graphics::PrimitiveType::Triangles;
        std::shared_ptr<Engine::Graphics::Texture> texture;
        glm::vec4 tint{1.0f};
        glm::mat4 fit{1.0f};
        float height = kActor;
        // Its model's clips, then those of its animator's files, made for its nodes, as the game does
        std::vector<Engine::Assets::AnimationClip> clips;
        std::vector<std::string> sources; // the files they came from
    };

    namespace
    {
        // What acts the block out this time: the object's look, or Atti
        struct Actor
        {
            const Engine::Graphics::Model* model = nullptr;
            const Mesh* mesh = nullptr;
            Engine::Graphics::PrimitiveType primitive = Engine::Graphics::PrimitiveType::Triangles;
            std::shared_ptr<Engine::Graphics::Texture> texture;
            glm::vec4 tint{1.0f};
            glm::mat4 fit{1.0f};
            float height = kActor;
            const std::vector<Engine::Assets::AnimationClip>* clips = nullptr;
            std::string idleClip; // what its animator plays until a block says otherwise
        };

        // The clip the pose has the object playing; nullptr: none, or its model has no such clip, so it stands still
        const Engine::Assets::AnimationClip* ClipOf(const Actor& actor, const PreviewPose& pose)
        {
            const std::string& name = pose.clip.empty() ? actor.idleClip : pose.clip;
            if (!actor.model || !actor.clips || name.empty())
            {
                return nullptr;
            }
            const int found = Engine::Assets::FindClip(*actor.clips, name);
            return found >= 0 ? &(*actor.clips)[static_cast<std::size_t>(found)] : nullptr;
        }

        std::shared_ptr<Engine::Graphics::Texture> LoadTexture(const std::filesystem::path& file)
        {
            const auto image = Engine::Assets::LoadTextureFromFile(file);
            if (!image || image->width <= 0 || image->height <= 0)
            {
                return nullptr;
            }
            Engine::Graphics::TextureSpec spec{};
            spec.width = static_cast<std::uint32_t>(image->width);
            spec.height = static_cast<std::uint32_t>(image->height);
            spec.wrapS = Engine::Graphics::TextureWrap::Repeat; // models tile their textures
            spec.wrapT = Engine::Graphics::TextureWrap::Repeat;
            auto texture = std::make_shared<Engine::Graphics::OpenGL::TextureGL>("BlockPreview");
            return texture->Create(spec.width, spec.height, spec, image->rgba.data()) ? texture : nullptr;
        }

        // A built-in shape as the engine draws it (a textured cube uses the cube with UVs)
        Mesh ShapeMesh(Engine::Game::Shape shape, bool textured)
        {
            using Engine::Game::Shape;
            switch (shape)
            {
            case Shape::Quad: return Mesh::CreateQuad();
            case Shape::Circle: return Mesh::CreateCircle();
            case Shape::Pyramid: return Mesh::CreatePyramid();
            case Shape::Plane: return Mesh::CreatePlane(1.0f);
            case Shape::Wirebox: return Mesh::CreateWirebox();
            case Shape::Sphere: return Mesh::CreateSphere(0.5f);
            case Shape::Line: return Mesh::CreateLine(glm::vec3(-0.5f, 0.0f, 0.0f), glm::vec3(0.5f, 0.0f, 0.0f));
            case Shape::Cube: break;
            }
            return textured ? Mesh::FromAssetData(Engine::Assets::PlaceholderMesh(), "BlockPreview")
                            : Mesh::CreateCube();
        }

        // Reads the object's model, mesh or shape (in that order, as the engine draws them) and sizes it to the stage.
        // ponytail: a .amat material's colour and texture are not read; the object's tint and texture are used
        void LoadLook(BlockPreviewer::Gpu& gpu, const std::filesystem::path& projectDir,
                      const Engine::Scene::RenderableDesc& look, const std::vector<std::string>& sources)
        {
            gpu.look = look;
            gpu.lookDir = projectDir;
            gpu.sources = sources;
            gpu.clips.clear();
            gpu.model.reset();
            gpu.mesh.reset();
            gpu.texture.reset();
            gpu.primitive = Engine::Graphics::PrimitiveType::Triangles;
            gpu.tint = glm::vec4(look.albedoR, look.albedoG, look.albedoB, look.albedoA);

            std::optional<Engine::Assets::BoundingBox> box;
            float turn = 0.0f; // a model or mesh file is turned inside its object; a built-in shape is not
            if (!look.model.empty())
            {
                const std::filesystem::path file = projectDir / look.model;
                if (const auto data = Engine::Assets::LoadModelFromFile(file))
                {
                    // Its textures sit next to it; each is read once even when several materials use it
                    std::unordered_map<std::string, std::shared_ptr<Engine::Graphics::Texture>> textures;
                    const auto resolve = [&](const std::string& path)
                    {
                        auto& texture = textures[path];
                        if (!texture)
                        {
                            const std::filesystem::path raw(path);
                            texture =
                                LoadTexture(raw.is_absolute() ? raw : (file.parent_path() / raw).lexically_normal());
                        }
                        return texture;
                    };
                    gpu.model = std::make_unique<Engine::Graphics::Model>(
                        Engine::Graphics::Model::FromModelData(*data, *gpu.shader, resolve));
                    gpu.tint = glm::vec4(1.0f); // a model has its own colours
                    gpu.clips = data->animations;
                    for (const std::string& source : sources)
                    {
                        if (const auto extra =
                                source.empty() ? std::nullopt : Engine::Assets::LoadModelFromFile(projectDir / source))
                        {
                            for (const Engine::Assets::AnimationClip& clip : extra->animations)
                            {
                                gpu.clips.push_back(Engine::Assets::Retarget(clip, extra->nodes, data->nodes));
                            }
                        }
                    }
                    box = Engine::Assets::RestBounds(*data);
                    turn = look.modelTurn;
                }
                else
                {
                    LOG_WARN(Editor, "Blocks: could not read the model \"{}\" for the block previews", look.model);
                }
            }
            if (!gpu.model && !look.mesh.empty())
            {
                if (const auto data = Engine::Assets::LoadMeshFromFile(projectDir / look.mesh))
                {
                    gpu.mesh = Mesh::FromAssetData(*data, "BlockPreview");
                    turn = look.modelTurn;
                }
            }
            if (!gpu.model)
            {
                if (!gpu.mesh)
                {
                    gpu.mesh = ShapeMesh(look.shape, !look.texture.empty());
                    const bool lines =
                        look.shape == Engine::Game::Shape::Wirebox || look.shape == Engine::Game::Shape::Line;
                    gpu.primitive =
                        lines ? Engine::Graphics::PrimitiveType::Lines : Engine::Graphics::PrimitiveType::Triangles;
                }
                if (!look.texture.empty())
                {
                    gpu.texture = LoadTexture(projectDir / look.texture);
                }
                if (!gpu.mesh->GetVertices().empty())
                {
                    Engine::Assets::BoundingBox vertices{gpu.mesh->GetVertices()[0].position,
                                                         gpu.mesh->GetVertices()[0].position};
                    for (const Engine::Graphics::Vertex& vertex : gpu.mesh->GetVertices())
                    {
                        vertices.min = glm::min(vertices.min, vertex.position);
                        vertices.max = glm::max(vertices.max, vertex.position);
                    }
                    box = vertices;
                }
            }

            const Engine::Assets::BoundingBox turned = Engine::Assets::Turned(
                box.value_or(Engine::Assets::BoundingBox{glm::vec3(-0.5f), glm::vec3(0.5f)}), turn);
            const glm::vec3 extent = turned.max - turned.min;
            const float scale = kActor / std::max({extent.x, extent.y, extent.z, 0.001f});
            const glm::vec3 centre = turned.Center();
            gpu.fit = glm::scale(glm::mat4(1.0f), glm::vec3(scale)) *
                      glm::translate(glm::mat4(1.0f), glm::vec3(-centre.x, -turned.min.y, -centre.z)) *
                      glm::rotate(glm::mat4(1.0f), Radians(turn), glm::vec3(0.0f, 1.0f, 0.0f));
            gpu.height = extent.y * scale;
        }

        // One mesh in one colour (and texture), at a place
        void DrawMesh(const BlockPreviewer::Gpu& gpu, const Mesh& mesh, const glm::vec4& colour, const glm::mat4& at,
                      const Camera3D& camera,
                      Engine::Graphics::PrimitiveType primitive = Engine::Graphics::PrimitiveType::Triangles,
                      const std::shared_ptr<Engine::Graphics::Texture>& texture = nullptr)
        {
            Engine::Graphics::Material material;
            material.SetShader(*gpu.shader);
            material.SetAlbedoTint(colour);
            if (texture)
            {
                material.SetAlbedoTexture(texture);
            }
            Engine::Renderer::MeshRenderer::Draw(mesh, material, at, camera, primitive);
        }

        // The object at a place, with the colour blocks' tint, faint by alpha
        void DrawActor(const BlockPreviewer::Gpu& gpu, const Actor& actor, const Placement& place,
                       const PreviewPose& pose, const Camera3D& camera, float alpha)
        {
            const Engine::Graphics::Shader& shader = *gpu.shader;
            shader.SetFloat4("u_Blend", glm::vec4(pose.tint[0], pose.tint[1], pose.tint[2], pose.tintAmount));
            shader.SetFloat("u_Alpha", alpha);
            glDepthMask(alpha < 1.0f ? GL_FALSE : GL_TRUE); // see-through: what is behind still shows
            const glm::mat4 at = place.Matrix() * actor.fit;
            if (actor.model)
            {
                if (const Engine::Assets::AnimationClip* clip = ClipOf(actor, pose))
                {
                    const float length = std::max(clip->duration, 0.001f);
                    const float going = pose.clip.empty() ? static_cast<float>(ImGui::GetTime()) : pose.clipTime;
                    const float time =
                        pose.clipFraction >= 0.0f ? pose.clipFraction * length : std::fmod(std::abs(going), length);
                    actor.model->Draw(at, camera, clip, time);
                }
                else
                {
                    actor.model->Draw(at, camera);
                }
            }
            else
            {
                DrawMesh(gpu, *actor.mesh, actor.tint, at, camera, actor.primitive, actor.texture);
            }
            glDepthMask(GL_TRUE);
            shader.SetFloat4("u_Blend", glm::vec4(0.0f));
            shader.SetFloat("u_Alpha", 1.0f);
        }

        // Its shadow on the floor under it, smaller and fainter the higher it is
        void DrawShadow(const BlockPreviewer::Gpu& gpu, const Placement& place, const Camera3D& camera)
        {
            const float fade = 1.0f - std::min(0.6f, place.at.y / 4.0f);
            glm::mat4 at = glm::translate(glm::mat4(1.0f), glm::vec3(place.at.x, 0.005f, place.at.z));
            at = glm::rotate(at, Radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            at = glm::scale(at, glm::vec3(kActor * 0.9f * place.size * fade));
            glDepthMask(GL_FALSE);
            DrawMesh(gpu, gpu.disc, glm::vec4(Theme::kInk.x, Theme::kInk.y, Theme::kInk.z, 0.22f * fade), at, camera);
            glDepthMask(GL_TRUE);
        }

        // Starts a render into the framebuffer, sized for a picture of size (in ImGui's units), on a see-through clear
        bool BeginRender(BlockPreviewer::Gpu& gpu, ImVec2 size, const Camera3D& camera, float haze)
        {
            const int width = std::max(1, static_cast<int>(size.x) * kSupersample);
            const int height = std::max(1, static_cast<int>(size.y) * kSupersample);
            if ((gpu.target.Width() != width || gpu.target.Height() != height) && !gpu.target.Resize(width, height))
            {
                return false;
            }
            gpu.target.Bind();
            glViewport(0, 0, width, height);
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_CULL_FACE); // models are often open, and seen from behind too
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glEnable(GL_BLEND);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const Engine::Graphics::Shader& shader = *gpu.shader;
            shader.SetFloat4("u_Blend", glm::vec4(0.0f));
            shader.SetFloat("u_Alpha", 1.0f);
            shader.SetFloat3("u_Eye", camera.GetPosition());
            shader.SetFloat4(
                "u_Fog", glm::vec4(Theme::kHeader.x, Theme::kHeader.y, Theme::kHeader.z, std::max(0.0f, haze) * 0.9f));
            return true;
        }

        void DrawFloor(const BlockPreviewer::Gpu& gpu, const Camera3D& camera)
        {
            gpu.shader->SetInt("u_Floor", 1);
            DrawMesh(gpu, gpu.floor, Vec(Theme::kPanel), glm::mat4(1.0f), camera);
            gpu.shader->SetInt("u_Floor", 0);
        }

        // A grey box standing on the floor: the other object a block names
        void DrawOther(const BlockPreviewer::Gpu& gpu, glm::vec3 at, const Camera3D& camera)
        {
            const glm::mat4 box =
                glm::scale(glm::translate(glm::mat4(1.0f), at + glm::vec3(0.0f, 0.45f, 0.0f)), glm::vec3(0.9f));
            DrawMesh(gpu, gpu.cube, Vec(Theme::kTextMuted), box, camera);
        }

        // Where a point of the 3D stage is drawn in the picture at min, size
        struct Projector
        {
            glm::mat4 viewProjection;
            ImVec2 min;
            ImVec2 size;

            ImVec2 operator()(glm::vec3 point) const
            {
                const glm::vec4 clip = viewProjection * glm::vec4(point, 1.0f);
                const float w = std::max(clip.w, 0.001f);
                return ImVec2(min.x + (clip.x / w * 0.5f + 0.5f) * size.x, min.y + (0.5f - clip.y / w * 0.5f) * size.y);
            }
        };

        // Draws the framebuffer's picture at min, size
        void DrawRendered(ImDrawList* drawList, const BlockPreviewer::Gpu& gpu, ImVec2 min, ImVec2 max)
        {
            drawList->AddCallback(PremultipliedBlend, nullptr);
            drawList->AddImage(static_cast<ImTextureID>(gpu.target.GetColorTextureID()), min, max, ImVec2(0.0f, 1.0f),
                               ImVec2(1.0f, 0.0f));
            drawList->AddCallback(StraightBlend, nullptr);
        }

        // The sky behind the stage from low down: its top down to the horizon, then its ground colour under it,
        // one band changing; a sky picture is a starry night coming in over it
        void DrawSky(ImDrawList* drawList, ImVec2 min, ImVec2 max, float horizon, const PreviewPose& pose, float scale)
        {
            const auto band = [&](int i)
            { return pose.skyBand == i ? Mix(kSky[i], pose.sky, pose.skyAmount) : kSky[i]; };
            horizon = std::clamp(horizon, min.y, max.y);
            drawList->AddRectFilledMultiColor(min, ImVec2(max.x, horizon), Colour(band(0)), Colour(band(0)),
                                              Colour(band(1)), Colour(band(1)));
            drawList->AddRectFilledMultiColor(ImVec2(min.x, horizon), max, Colour(band(1)), Colour(band(1)),
                                              Colour(band(2)), Colour(band(2)));
            if (pose.skyBand == 3)
            {
                drawList->AddRectFilled(min, ImVec2(max.x, horizon), Colour(kGameScreen, pose.skyAmount));
                for (int i = 0; i < 14; ++i)
                {
                    const float x = min.x + std::fmod(static_cast<float>(i) * 37.0f, max.x - min.x);
                    const float y = min.y + std::fmod(static_cast<float>(i) * 23.0f,
                                                      std::max(1.0f, horizon - min.y - 8.0f * scale));
                    drawList->AddCircleFilled(ImVec2(x, y), 1.5f * scale, Colour(kWhite, pose.skyAmount));
                }
            }
        }

        // From high up: the object on the floor, where it started faint, and an arrow out of it the way it faces
        void DrawHighUp(ImDrawList* drawList, BlockPreviewer::Gpu& gpu, const Actor& actor, ImVec2 min, ImVec2 max,
                        const Engine::Logic::Block& block, const PreviewPose& start, const PreviewPose& pose,
                        float scale)
        {
            const ImVec2 size(max.x - min.x, max.y - min.y);
            const Camera3D camera = CameraAt(kHighUp, size.x / size.y);
            const PreviewPose end = PoseAt(block, 1.0f);
            const glm::vec2 offset(-std::round((end.x + start.x) * 0.5f), -std::round((end.y + start.y) * 0.5f));
            const Placement now = PlaceHighUp(pose, offset);
            const Placement before = PlaceHighUp(start, offset);
            const bool moved = before.at != now.at || start.facing != pose.facing;
            const bool named = block.op == Op::GoToObject || block.op == Op::PointTowards;
            {
                const SavedGlState saved;
                if (!BeginRender(gpu, size, camera, -1.0f))
                {
                    return;
                }
                DrawFloor(gpu, camera);
                if (pose.target && named)
                {
                    DrawOther(gpu, OnFloor((*pose.target)[0], (*pose.target)[1], offset), camera);
                }
                DrawShadow(gpu, now, camera);
                DrawActor(gpu, actor, now, pose, camera, pose.alpha);
                if (moved)
                {
                    DrawActor(gpu, actor, before, start, camera, 0.25f);
                }
            }
            DrawRendered(drawList, gpu, min, max);

            const Projector project{camera.GetViewProjectionMatrix(), min, size};
            if (pose.target)
            {
                const glm::vec3 at = OnFloor((*pose.target)[0], (*pose.target)[1], offset);
                if (named)
                {
                    Label(drawList, project(at + glm::vec3(0.0f, 1.3f, 0.0f)), NameOr(block.text, "Player"), scale);
                }
                else
                {
                    // The place: a cross on the floor
                    constexpr float r = 0.3f;
                    drawList->AddLine(project(at + glm::vec3(-r, 0.0f, -r)), project(at + glm::vec3(r, 0.0f, r)),
                                      Colour(Theme::kSelect), 2.5f * scale);
                    drawList->AddLine(project(at + glm::vec3(-r, 0.0f, r)), project(at + glm::vec3(r, 0.0f, -r)),
                                      Colour(Theme::kSelect), 2.5f * scale);
                }
            }
            // The arrows, over its top and out past its front, the way it faces
            const auto arrow = [&](const Placement& place, float alpha)
            {
                const glm::vec3 top = place.at + glm::vec3(0.0f, actor.height * place.size + 0.05f, 0.0f);
                DrawArrow(drawList, project(top),
                          project(top + Front(place.facing) * (kActor * 0.5f * place.size + 0.5f)), Theme::kMovement,
                          scale, alpha);
            };
            if (moved)
            {
                arrow(before, 0.3f);
            }
            arrow(now, pose.alpha);
            if (!pose.mark.empty())
            {
                Label(drawList, project(now.at + glm::vec3(0.0f, 0.0f, kActor * 0.5f * now.size + 0.5f)), pose.mark,
                      scale);
            }
            DrawReadout(drawList, ImVec2(min.x + 6.0f * scale, min.y + 6.0f * scale), pose, scale);
        }

        // From low down: the object jumping, changing, talking and so on, in the sky and haze the blocks set
        void DrawLowDown(ImDrawList* drawList, BlockPreviewer::Gpu& gpu, const Actor& actor, ImVec2 min, ImVec2 max,
                         const Engine::Logic::Block& block, const PreviewPose& start, const PreviewPose& pose,
                         float scale)
        {
            const ImVec2 size(max.x - min.x, max.y - min.y);
            const Camera3D camera = CameraAt(kLowDown, size.x / size.y);
            const Projector project{camera.GetViewProjectionMatrix(), min, size};
            Placement now = PlaceLowDown(block, pose);
            if (!pose.clip.empty() && ClipOf(actor, pose))
            {
                now.at.y = 0.0f; // its real animation, so it need not bob to show it
            }
            const Placement before = PlaceLowDown(block, start);
            const float home = StartsLeft(block.op) ? kFromLeft : 0.0f;

            if (pose.skyBand >= 0)
            {
                DrawSky(drawList, min, max, project(camera.GetPosition() + glm::vec3(0.0f, 0.0f, -1000.0f)).y, pose,
                        scale);
            }
            if (pose.haze >= 0.0f)
            {
                drawList->AddRectFilled(min, max, Colour(Theme::kHeader, pose.haze * 0.9f)); // the haze over the sky
            }
            {
                const SavedGlState saved;
                if (!BeginRender(gpu, size, camera, pose.haze))
                {
                    return;
                }
                DrawFloor(gpu, camera);
                if (pose.haze >= 0.0f)
                {
                    // Far hills, for the haze to come over
                    DrawMesh(gpu, gpu.hill, Vec(Theme::kAxisY),
                             glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-3.5f, 0.0f, -9.0f)),
                                        glm::vec3(7.0f, 3.5f, 4.0f)),
                             camera);
                    DrawMesh(gpu, gpu.hill, Vec(Theme::kAxisY),
                             glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, -11.0f)),
                                        glm::vec3(9.0f, 5.0f, 4.0f)),
                             camera);
                }
                if (pose.other)
                {
                    DrawOther(gpu, glm::vec3(std::min(home + *pose.other, kRoomX), 0.0f, 0.0f), camera);
                }
                DrawShadow(gpu, now, camera);
                DrawActor(gpu, actor, now, pose, camera, pose.alpha);
                if (before.at.x != now.at.x)
                {
                    DrawActor(gpu, actor, before, start, camera, 0.25f);
                }
            }
            DrawRendered(drawList, gpu, min, max);

            const float tall = actor.height * now.size;
            const ImVec2 head = project(now.at + glm::vec3(0.0f, tall, 0.0f));
            // A jump's height as a faint line with its number, so "jump 3" reads as higher than "jump 1"
            if (block.op == Op::Jump)
            {
                const float height = PreviewNumber(block, 0);
                const float y = std::max(min.y + 30.0f * scale,
                                         project(glm::vec3(home, std::min(height, kCeiling) + actor.height, 0.0f)).y);
                char label[32];
                std::snprintf(label, sizeof label, "%g high", height);
                const float labelX = max.x - ImGui::CalcTextSize(label).x - 8.0f * scale;
                DrawDashes(drawList, ImVec2(min.x + 8.0f * scale, y), ImVec2(labelX - 8.0f * scale, y),
                           Theme::kTextMuted, scale);
                Label(drawList, ImVec2(labelX + ImGui::CalcTextSize(label).x * 0.5f, y), label, scale);
            }
            if (pose.other)
            {
                const glm::vec3 other(std::min(home + *pose.other, kRoomX), 0.0f, 0.0f);
                Label(drawList, project(other + glm::vec3(0.0f, 1.25f, 0.0f)), NameOr(block.text, "Player"), scale);
                if (block.op == Op::DistanceTo)
                {
                    DrawDashes(drawList, project(now.at + glm::vec3(kActor * 0.5f * now.size, 0.45f, 0.0f)),
                               project(other + glm::vec3(-0.45f, 0.45f, 0.0f)), Theme::kSelect, scale);
                }
            }
            if (pose.tilt)
            {
                // Its front (the right) tipped up by tilt degrees
                const float r = Radians(*pose.tilt);
                const glm::vec3 from = now.at + glm::vec3(kActor * 0.55f * now.size, tall * 0.5f, 0.0f);
                DrawArrow(drawList, project(from), project(from + glm::vec3(std::cos(r), std::sin(r), 0.0f) * 1.2f),
                          Theme::kMovement, scale, 1.0f);
            }
            if (pose.health >= 0.0f)
            {
                DrawHealthBar(drawList, ImVec2(head.x, head.y - 12.0f * scale), pose.health, scale, pose.healthAlpha);
            }
            if (pose.speaking)
            {
                DrawSpeech(drawList, ImVec2(head.x + 6.0f * scale, head.y + 4.0f * scale), block.text, scale);
            }
            if (pose.sound)
            {
                DrawWaves(drawList, project(now.at + glm::vec3(kActor * 0.5f * now.size, tall * 0.5f, 0.0f)), scale);
            }
            if (!pose.mark.empty())
            {
                Label(drawList, ImVec2(head.x, head.y - (pose.health >= 0.0f ? 26.0f : 12.0f) * scale), pose.mark,
                      scale);
            }
            if (pose.envelope)
            {
                DrawEnvelope(drawList, project(glm::vec3(home + *pose.envelope, actor.height + 0.3f, 0.0f)),
                             NameOr(block.text, "go"), scale);
            }
            const ImVec2 corner(min.x + 8.0f * scale, max.y - 34.0f * scale);
            if (!pose.key.empty())
            {
                DrawKey(drawList, corner, pose.key, pose.keyDown, scale);
            }
            if (block.op == Op::WhenPlayStarts)
            {
                DrawRun(drawList, corner, pose.runPressed, scale);
            }
            DrawReadout(drawList, ImVec2(min.x + 6.0f * scale, min.y + 6.0f * scale), pose, scale);
            DrawProgress(drawList, min, max, pose, scale);
        }

        // The game's screen, the object standing in the game behind it, with what the block puts on it or takes off
        void DrawScreen(ImDrawList* drawList, BlockPreviewer::Gpu& gpu, const Actor& actor, ImVec2 min, ImVec2 max,
                        const Engine::Logic::Block& block, const PreviewPose& pose, float scale)
        {
            const ImVec2 screenMin(min.x + 8.0f * scale, min.y + 8.0f * scale);
            const ImVec2 screenMax(max.x - 8.0f * scale, max.y - 30.0f * scale);
            const ImVec2 size(screenMax.x - screenMin.x, screenMax.y - screenMin.y);
            drawList->AddRectFilled(screenMin, screenMax, Colour(kGameScreen), 4.0f * scale);
            {
                const Camera3D camera = CameraAt(kLowDown, size.x / size.y);
                bool rendered = false;
                {
                    const SavedGlState saved;
                    if (BeginRender(gpu, size, camera, -1.0f))
                    {
                        DrawFloor(gpu, camera);
                        const Placement place{glm::vec3(0.0f), kLowDown.yaw + 180.0f, 0.0f, 1.0f};
                        DrawShadow(gpu, place, camera);
                        DrawActor(gpu, actor, place, PreviewPose{}, camera, 1.0f);
                        rendered = true;
                    }
                }
                if (rendered)
                {
                    DrawRendered(drawList, gpu, screenMin, screenMax);
                }
            }
            const auto at = [&](float x, float y)
            { return ImVec2(screenMin.x + size.x * x / 100.0f, screenMin.y + size.y * y / 100.0f); };
            const float a = pose.screenAlpha;

            switch (block.op)
            {
            case Op::ShowScreenPicture:
            {
                // A picture: a frame with hills and a sun, and its name under it
                const ImVec2 c = at(50.0f, 42.0f);
                const ImVec2 half(26.0f * scale, 18.0f * scale);
                drawList->AddRectFilled(ImVec2(c.x - half.x, c.y - half.y), ImVec2(c.x + half.x, c.y + half.y),
                                        Colour(Theme::kVisuals, a));
                drawList->AddTriangleFilled(ImVec2(c.x - half.x, c.y + half.y),
                                            ImVec2(c.x - 6.0f * scale, c.y - 2.0f * scale),
                                            ImVec2(c.x + 10.0f * scale, c.y + half.y), Colour(Theme::kAxisY, a));
                drawList->AddCircleFilled(ImVec2(c.x + 14.0f * scale, c.y - 8.0f * scale), 5.0f * scale,
                                          Colour(Theme::kEvents, a));
                drawList->AddRect(ImVec2(c.x - half.x, c.y - half.y), ImVec2(c.x + half.x, c.y + half.y),
                                  Colour(kWhite, a), 0.0f, ImDrawFlags_None, 2.0f * scale);
                TextAt(drawList, ImVec2(c.x, c.y + half.y + 9.0f * scale), NameOr(block.text, "picture"), kWhite, a);
                break;
            }
            case Op::LockMouse:
            case Op::FreeMouse:
            {
                // The pointer: an arrow shape
                const ImVec2 c = at(55.0f, 40.0f);
                const float s = 1.2f * scale;
                const ImVec2 points[] = {c,
                                         ImVec2(c.x, c.y + 16.0f * s),
                                         ImVec2(c.x + 4.0f * s, c.y + 12.0f * s),
                                         ImVec2(c.x + 7.0f * s, c.y + 18.0f * s),
                                         ImVec2(c.x + 9.0f * s, c.y + 17.0f * s),
                                         ImVec2(c.x + 6.5f * s, c.y + 11.0f * s),
                                         ImVec2(c.x + 12.0f * s, c.y + 11.0f * s)};
                drawList->AddConcavePolyFilled(points, 7, Colour(kWhite, a));
                drawList->AddPolyline(points, 7, Colour(Theme::kInk, a), ImDrawFlags_Closed, 1.5f * scale);
                break;
            }
            case Op::GoToScene:
            {
                // A curtain across the screen, with the scene's name on it
                const float across = size.x * pose.wipe;
                drawList->AddRectFilled(screenMin, ImVec2(screenMin.x + across, screenMax.y), Colour(Theme::kInk),
                                        4.0f * scale);
                if (pose.wipe > 0.6f)
                {
                    TextAt(drawList, at(50.0f, 50.0f), "scene \"" + NameOr(block.text, "main") + "\"", kWhite);
                }
                break;
            }
            default:
            {
                // A text (or a button, for "when clicked on screen"), where the block puts it, as big and in the
                // colour it says
                const std::string text = block.op == Op::ShowScreenText      ? NameOr(block.text, "Hello")
                                         : block.op == Op::WhenScreenClicked ? "Play"
                                                                             : "Text";
                const float height = std::clamp(size.y * pose.screenSize / 100.0f, 9.0f * scale, size.y * 0.6f);
                const ImVec2 c = at(pose.screenX, pose.screenY);
                ImFont* font = Theme::BoldFont() ? Theme::BoldFont() : ImGui::GetFont();
                const ImVec2 textSize = font->CalcTextSizeA(height, FLT_MAX, 0.0f, text.c_str());
                if (block.op == Op::WhenScreenClicked)
                {
                    const ImVec2 pad(10.0f * scale, 4.0f * scale);
                    const ImVec2 bmin(c.x - textSize.x * 0.5f - pad.x, c.y - textSize.y * 0.5f - pad.y);
                    const ImVec2 bmax(c.x + textSize.x * 0.5f + pad.x, c.y + textSize.y * 0.5f + pad.y);
                    drawList->AddRectFilled(bmin, bmax, Colour(pose.clicked ? Theme::kRaisedHigh : Theme::kRun),
                                            6.0f * scale);
                    drawList->AddRect(bmin, bmax, Colour(Theme::kInk), 6.0f * scale, ImDrawFlags_None, 1.5f * scale);
                }
                const ImVec4 colour =
                    block.op == Op::WhenScreenClicked ? Theme::kInk : Mix(kWhite, pose.tint, pose.tintAmount);
                // An outline, so a white text still reads over the light floor
                if (block.op != Op::WhenScreenClicked)
                {
                    for (const ImVec2 nudge :
                         {ImVec2(-1.0f, 0.0f), ImVec2(1.0f, 0.0f), ImVec2(0.0f, -1.0f), ImVec2(0.0f, 1.0f)})
                    {
                        drawList->AddText(font, height,
                                          ImVec2(c.x - textSize.x * 0.5f + nudge.x * scale,
                                                 c.y - textSize.y * 0.5f + nudge.y * scale),
                                          Colour(Theme::kInk, a), text.c_str());
                    }
                }
                drawList->AddText(font, height, ImVec2(c.x - textSize.x * 0.5f, c.y - textSize.y * 0.5f),
                                  Colour(colour, a), text.c_str());
                if (block.op == Op::WhenScreenClicked)
                {
                    // The mouse clicking it
                    const ImVec2 tip(c.x + textSize.x * 0.3f, c.y + 2.0f * scale);
                    const float s = pose.clicked ? 0.9f * scale : 1.1f * scale;
                    const ImVec2 points[] = {tip,
                                             ImVec2(tip.x, tip.y + 16.0f * s),
                                             ImVec2(tip.x + 4.0f * s, tip.y + 12.0f * s),
                                             ImVec2(tip.x + 7.0f * s, tip.y + 18.0f * s),
                                             ImVec2(tip.x + 9.0f * s, tip.y + 17.0f * s),
                                             ImVec2(tip.x + 6.5f * s, tip.y + 11.0f * s),
                                             ImVec2(tip.x + 12.0f * s, tip.y + 11.0f * s)};
                    drawList->AddConcavePolyFilled(points, 7, Colour(kWhite));
                    drawList->AddPolyline(points, 7, Colour(Theme::kInk), ImDrawFlags_Closed, 1.5f * scale);
                }
                break;
            }
            }
            drawList->AddRect(screenMin, screenMax, Colour(Theme::kInk), 4.0f * scale, ImDrawFlags_None, 1.5f * scale);
            DrawReadout(drawList, ImVec2(min.x + 8.0f * scale, screenMax.y + 4.0f * scale), pose, scale);
        }

        // A sum or condition worked out: its words big in the middle, its answer under them
        void DrawValue(ImDrawList* drawList, ImVec2 min, ImVec2 max, const PreviewPose& pose, float scale)
        {
            ImFont* font = Theme::BoldFont() ? Theme::BoldFont() : ImGui::GetFont();
            const float room = max.x - min.x - 24.0f * scale;
            float height = ImGui::GetFontSize() * 1.5f;
            ImVec2 size = font->CalcTextSizeA(height, FLT_MAX, 0.0f, pose.readout.c_str());
            if (size.x > room)
            {
                height *= room / size.x;
                size = font->CalcTextSizeA(height, FLT_MAX, 0.0f, pose.readout.c_str());
            }
            const ImVec2 centre((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f - (pose.answer ? 12.0f * scale : 0.0f));
            const ImVec2 textMin(centre.x - size.x * 0.5f, centre.y - size.y * 0.5f);
            if (pose.changed)
            {
                const float pad = 6.0f * scale;
                drawList->AddRectFilled(ImVec2(textMin.x - pad, textMin.y - pad * 0.5f),
                                        ImVec2(textMin.x + size.x + pad, textMin.y + size.y + pad * 0.5f),
                                        Colour(Theme::kRaisedHigh), 6.0f * scale);
            }
            drawList->AddText(font, height, textMin, Colour(Theme::kText), pose.readout.c_str());
            if (pose.answer)
            {
                const float width = ImGui::CalcTextSize(*pose.answer ? "true" : "false").x + 10.0f * scale;
                AnswerChip(drawList, ImVec2(centre.x - width * 0.5f, textMin.y + size.y + 10.0f * scale), *pose.answer,
                           scale);
            }
        }

        // ---- A script playing

        // How much of a script's way fits on the floor
        struct ScriptView
        {
            float centreX = 0.0f; // the middle of its way, in the middle of the stage
            float centreZ = 0.0f;
            float fit = 1.0f;   // a long way is drawn smaller, so that the object stays in sight
            bool walks = false; // it goes somewhere or turns: seen from high up, else from low down
        };

        ScriptView ViewOf(const ScriptPreview& script)
        {
            ScriptView view;
            glm::vec2 low(0.0f);
            glm::vec2 high(0.0f);
            const auto reach = [&](float x, float z)
            {
                low = glm::min(low, glm::vec2(x, z));
                high = glm::max(high, glm::vec2(x, z));
            };
            for (const ScriptStep& step : script.steps)
            {
                reach(step.to.x, step.to.z);
                if (step.other)
                {
                    reach((*step.other)[0], (*step.other)[1]);
                }
                view.walks = view.walks || step.to.x != 0.0f || step.to.z != 0.0f || step.to.facing != 0.0f;
            }
            view.centreX = (low.x + high.x) * 0.5f;
            view.centreZ = (low.y + high.y) * 0.5f;
            view.fit = std::min({1.0f, kRoomX * 2.0f / std::max(high.x - low.x, 0.001f),
                                 kRoomZ * 2.0f / std::max(high.y - low.y, 0.001f)});
            return view;
        }

        // A solid bar of words across the preview, so that they never have to be read over the picture
        void DrawBar(ImDrawList* drawList, ImVec2 min, ImVec2 max, std::string text, ImDrawFlags corners, float scale)
        {
            const float pad = 8.0f * scale;
            const float room = max.x - min.x - pad * 2.0f;
            if (ImGui::CalcTextSize(text.c_str()).x > room)
            {
                while (!text.empty() && ImGui::CalcTextSize((text + "...").c_str()).x > room)
                {
                    text.pop_back();
                }
                text += "...";
            }
            drawList->AddRectFilled(min, max, Colour(Theme::kPanel), 6.0f * scale, corners);
            drawList->AddText(ImVec2(min.x + pad, (min.y + max.y - ImGui::GetTextLineHeight()) * 0.5f),
                              Colour(Theme::kText), text.c_str());
        }

        // What starts the script, over the caption
        void DrawTrigger(ImDrawList* drawList, ImVec2 min, ImVec2 max, const ScriptFrame& frame, float scale)
        {
            const ImVec2 corner(min.x + 8.0f * scale, max.y - 38.0f * scale);
            if (!frame.pose.key.empty())
            {
                DrawKey(drawList, corner, frame.pose.key, frame.pose.keyDown, scale);
            }
            if (frame.block->op == Op::WhenPlayStarts)
            {
                DrawRun(drawList, corner, frame.pose.runPressed, scale);
            }
        }

        // The variables as labels in the corner, next to the object acting: the one just changed lit up
        void DrawVariableLabels(ImDrawList* drawList, ImVec2 max, const ScriptFrame& frame, float scale)
        {
            constexpr std::size_t kListed = 3;
            float y = max.y - ImGui::GetTextLineHeight() - 12.0f * scale;
            const std::size_t count = std::min(frame.variables.size(), kListed);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::string& text = frame.variables[i];
                const float width = ImGui::CalcTextSize(text.c_str()).x + 10.0f * scale;
                Pill(drawList, ImVec2(max.x - width - 6.0f * scale, y), text,
                     static_cast<int>(i) == frame.lit ? Theme::kRaisedHigh : Theme::kPanel, scale);
                y -= ImGui::GetTextLineHeight() + 8.0f * scale;
            }
        }

        // A script that does nothing to the object: no object, only what it does change, big in the middle
        void DrawScriptValues(ImDrawList* drawList, ImVec2 min, ImVec2 max, const ScriptFrame& frame, float scale)
        {
            if (frame.variables.empty())
            {
                // Nor any variables (a wait, a message): its words are the picture
                PreviewPose words = frame.pose;
                words.readout = words.readout.empty() ? Engine::Logic::Describe(*frame.block) : words.readout;
                DrawValue(drawList, min, max, words, scale);
            }
            else
            {
                constexpr std::size_t kListed = 3;
                ImFont* font = Theme::BoldFont() ? Theme::BoldFont() : ImGui::GetFont();
                const float height = ImGui::GetFontSize() * 1.5f;
                const std::size_t count = std::min(frame.variables.size(), kListed);
                const float line = height + 8.0f * scale;
                float y = (min.y + max.y) * 0.5f - line * static_cast<float>(count) * 0.5f;
                for (std::size_t i = 0; i < count; ++i)
                {
                    const std::string& text = frame.variables[i];
                    const ImVec2 size = font->CalcTextSizeA(height, FLT_MAX, 0.0f, text.c_str());
                    const ImVec2 at((min.x + max.x - size.x) * 0.5f, y + (line - size.y) * 0.5f);
                    if (static_cast<int>(i) == frame.lit)
                    {
                        const float pad = 6.0f * scale;
                        drawList->AddRectFilled(ImVec2(at.x - pad, at.y - pad * 0.5f),
                                                ImVec2(at.x + size.x + pad, at.y + size.y + pad * 0.5f),
                                                Colour(Theme::kRaisedHigh), 6.0f * scale);
                    }
                    drawList->AddText(font, height, at, Colour(Theme::kText), text.c_str());
                    y += line;
                }
                DrawReadout(drawList, ImVec2(min.x + 6.0f * scale, min.y + 6.0f * scale), frame.pose, scale);
            }
            DrawProgress(drawList, min, max, frame.pose, scale);
            DrawTrigger(drawList, min, max, frame, scale);
        }

        // A script the object acts out: from high up if it goes anywhere, as moving is seen, else from low down
        void DrawScriptActing(ImDrawList* drawList, BlockPreviewer::Gpu& gpu, const Actor& actor, ImVec2 min,
                              ImVec2 max, const ScriptPreview& script, const ScriptFrame& frame, float scale)
        {
            const ScriptView view = ViewOf(script);
            const ImVec2 size(max.x - min.x, max.y - min.y);
            const Camera3D camera = CameraAt(view.walks ? kHighUp : kLowDown, size.x / size.y);
            const auto onStage = [&](float x, float z)
            { return glm::vec3((x - view.centreX) * view.fit, 0.0f, (z - view.centreZ) * view.fit); };

            Placement place;
            place.at = onStage(frame.actor.x, frame.actor.z);
            place.facing = view.walks ? frame.actor.facing : kLowDown.yaw + 180.0f;
            place.tilt = frame.actor.tilt;
            place.size = std::clamp(frame.actor.size, 0.2f, 2.2f);
            PreviewPose pose = frame.pose;
            pose.tint = frame.actor.tint;
            pose.tintAmount = frame.actor.tintAmount;
            // A model playing the clip really plays it: only a shape with none bobs to show it
            const bool animated = !pose.clip.empty() && ClipOf(actor, pose) && frame.block->op != Op::Jump;
            place.at.y = animated ? 0.0f : std::clamp(frame.lift, 0.0f, kCeiling);
            const glm::vec3 other = frame.other ? onStage((*frame.other)[0], (*frame.other)[1]) : glm::vec3(0.0f);
            {
                const SavedGlState saved;
                if (!BeginRender(gpu, size, camera, -1.0f))
                {
                    return;
                }
                DrawFloor(gpu, camera);
                if (frame.other)
                {
                    DrawOther(gpu, other, camera);
                }
                DrawShadow(gpu, place, camera);
                DrawActor(gpu, actor, place, pose, camera, frame.actor.alpha);
            }
            DrawRendered(drawList, gpu, min, max);

            const Projector project{camera.GetViewProjectionMatrix(), min, size};
            const float tall = actor.height * place.size;
            const ImVec2 head = project(place.at + glm::vec3(0.0f, tall, 0.0f));
            if (view.walks)
            {
                const glm::vec3 top = place.at + glm::vec3(0.0f, tall + 0.05f, 0.0f);
                DrawArrow(drawList, project(top),
                          project(top + Front(place.facing) * (kActor * 0.5f * place.size + 0.5f)), Theme::kMovement,
                          scale, frame.actor.alpha);
            }
            if (frame.other)
            {
                Label(drawList, project(other + glm::vec3(0.0f, 1.3f, 0.0f)), NameOr(frame.block->text, "Player"),
                      scale);
            }
            if (pose.speaking)
            {
                DrawSpeech(drawList, ImVec2(head.x + 6.0f * scale, head.y + 4.0f * scale), frame.block->text, scale);
            }
            if (pose.sound)
            {
                DrawWaves(drawList, project(place.at + glm::vec3(kActor * 0.5f * place.size, tall * 0.5f, 0.0f)),
                          scale);
            }
            if (!pose.mark.empty())
            {
                Label(drawList, ImVec2(head.x, head.y - 12.0f * scale), pose.mark, scale);
            }
            DrawReadout(drawList, ImVec2(min.x + 6.0f * scale, min.y + 6.0f * scale), pose, scale);
            DrawProgress(drawList, min, max, pose, scale);
            DrawVariableLabels(drawList, max, frame, scale);
            DrawTrigger(drawList, min, max, frame, scale);
        }

        // The 3D needs OpenGL's functions (loaded by the Viewport's first render) and the shader. Returns whether it
        // can be drawn.
        bool Prepare3D(std::unique_ptr<BlockPreviewer::Gpu>& gpu)
        {
            Engine::Graphics::RenderAPI::Init(); // does nothing once GLEW is loaded
            if (!gpu && Engine::Graphics::RenderAPI::IsReady())
            {
                gpu = std::make_unique<BlockPreviewer::Gpu>();
                auto shader = std::make_unique<Engine::Graphics::Shader>("BlockPreview");
                if (shader->Compile(kVertexShader, kFragmentShader))
                {
                    gpu->shader = std::move(shader);
                }
                else
                {
                    LOG_WARN(Editor, "Blocks: the block preview shader did not compile; previews show without the 3D");
                }
            }
            return gpu && gpu->shader;
        }

        // What acts it out: the object's look, read if it is not the one last shown, or Atti, a yellow cube
        Actor ActorOf(BlockPreviewer::Gpu& gpu, const std::filesystem::path& projectDir,
                      const Engine::Scene::RenderableDesc* look, const Engine::Scene::AnimatorComponent* animator)
        {
            Actor actor;
            if (look)
            {
                const std::vector<std::string> sources = animator ? animator->sources : std::vector<std::string>{};
                if (!gpu.look || *gpu.look != *look || gpu.lookDir != projectDir || gpu.sources != sources)
                {
                    LoadLook(gpu, projectDir, *look, sources);
                }
                actor = Actor{
                    gpu.model.get(), gpu.mesh ? &*gpu.mesh : nullptr, gpu.primitive, gpu.texture, gpu.tint, gpu.fit,
                    gpu.height};
                actor.clips = &gpu.clips;
                actor.idleClip = animator && animator->playing ? animator->clip : std::string();
            }
            else
            {
                actor.mesh = &gpu.cube;
                actor.tint = Vec(Theme::kEvents);
                actor.fit = glm::scale(glm::mat4(1.0f), glm::vec3(kActor)) *
                            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f));
            }
            return actor;
        }
    }

    BlockPreviewer::BlockPreviewer() = default;
    BlockPreviewer::~BlockPreviewer() = default;

    void BlockPreviewer::Clear()
    {
        gpu_.reset();
    }

    void BlockPreviewer::Draw(const Engine::Logic::Block& block, const std::filesystem::path& projectDir,
                              const Engine::Scene::RenderableDesc* look,
                              const Engine::Scene::AnimatorComponent* animator)
    {
        const PreviewStage stage = StageOf(block.op);
        if (stage == PreviewStage::None)
        {
            return;
        }
        const float scale = ImGui::GetStyle().FontScaleDpi; // the display's scale, as the Theme's controls use
        ImGui::PushFont(Theme::BoldFont(), kPreviewFont);
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max(min.x + kPreviewWidth * scale, min.y + kPreviewHeight * scale);
        const float t = Theme::AnimationsOn()
                            ? static_cast<float>(std::fmod(ImGui::GetTime(), kPreviewSeconds) / kPreviewSeconds)
                            : StillPhase(block.op);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, Colour(Theme::kHeader), 6.0f * scale);
        drawList->PushClipRect(min, max, true);
        const PreviewPose start = PoseAt(block, 0.0f);
        const PreviewPose pose = PoseAt(block, t);

        if (stage == PreviewStage::Value || !Prepare3D(gpu_))
        {
            if (stage == PreviewStage::Value)
            {
                DrawValue(drawList, min, max, pose, scale);
            }
            else
            {
                DrawReadout(drawList, ImVec2(min.x + 6.0f * scale, min.y + 6.0f * scale), pose, scale);
            }
        }
        else
        {
            const Actor actor = ActorOf(*gpu_, projectDir, look, animator);
            switch (stage)
            {
            case PreviewStage::Top: DrawHighUp(drawList, *gpu_, actor, min, max, block, start, pose, scale); break;
            case PreviewStage::Side: DrawLowDown(drawList, *gpu_, actor, min, max, block, start, pose, scale); break;
            case PreviewStage::Screen: DrawScreen(drawList, *gpu_, actor, min, max, block, pose, scale); break;
            case PreviewStage::Value:
            case PreviewStage::None: break;
            }
        }
        drawList->PopClipRect();
        drawList->AddRect(min, max, Colour(Theme::kInk), 6.0f * scale, ImDrawFlags_None, 1.5f * scale);
        ImGui::Dummy(ImVec2(max.x - min.x, max.y - min.y));
        ImGui::PopFont();
    }

    void BlockPreviewer::DrawScript(const ScriptPreview& script, std::uint32_t blockId,
                                    const std::filesystem::path& projectDir, const Engine::Scene::RenderableDesc* look,
                                    const Engine::Scene::AnimatorComponent* animator)
    {
        // It tells a story, so it starts at its first step when the mouse comes to the block, and goes on one step at
        // a time only when asked: E for the next step, Q for the one before (not while a field is being typed in)
        constexpr double kGone = 0.25;
        const double now = ImGui::GetTime();
        if (blockId != scriptBlock_ || now - scriptSeen_ > kGone)
        {
            scriptBlock_ = blockId;
            scriptStep_ = 0;
            scriptStarted_ = now;
        }
        scriptSeen_ = now;
        const std::size_t stepCount = script.steps.size();
        scriptStep_ = stepCount == 0 ? 0 : std::min(scriptStep_, stepCount - 1);
        if (stepCount > 0 && !ImGui::GetIO().WantTextInput)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_E, false) && scriptStep_ + 1 < stepCount)
            {
                ++scriptStep_;
                scriptStarted_ = now;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Q, false) && scriptStep_ > 0)
            {
                --scriptStep_;
                scriptStarted_ = now;
            }
        }

        // The step's own stretch of the script, played over and over (still: its end)
        double seconds = 0.0;
        if (stepCount > 0)
        {
            for (std::size_t i = 0; i < scriptStep_; ++i)
            {
                seconds += script.steps[i].seconds;
            }
            const double length = std::max(static_cast<double>(script.steps[scriptStep_].seconds), 0.01);
            const double into = Theme::AnimationsOn() ? std::fmod(now - scriptStarted_, length) : length * 0.999;
            seconds += std::min(into, length * 0.999);
        }
        const ScriptFrame frame = FrameAt(script, seconds);
        shown_ = frame.block;

        const float scale = ImGui::GetStyle().FontScaleDpi; // the display's scale, as the Theme's controls use
        ImGui::PushFont(Theme::BoldFont(), kPreviewFont);
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max(min.x + kPreviewWidth * scale, min.y + kPreviewHeight * scale);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, Colour(Theme::kHeader), 6.0f * scale);
        drawList->PushClipRect(min, max, true);
        if (frame.block)
        {
            // The picture, between a bar with which outcome this is and a bar with the block running
            const float bar = ImGui::GetTextLineHeight() + 10.0f * scale;
            const ImVec2 top(min.x, frame.outcome.empty() ? min.y : min.y + bar);
            const ImVec2 bottom(max.x, max.y - bar);
            const bool in3D = (script.actsOnObject || frame.screen) && Prepare3D(gpu_);
            if (!in3D)
            {
                DrawScriptValues(drawList, top, bottom, frame, scale);
            }
            else if (frame.screen)
            {
                DrawScreen(drawList, *gpu_, ActorOf(*gpu_, projectDir, look, animator), top, bottom, *frame.block,
                           frame.pose, scale);
            }
            else
            {
                DrawScriptActing(drawList, *gpu_, ActorOf(*gpu_, projectDir, look, animator), top, bottom, script,
                                 frame, scale);
            }
            if (!frame.outcome.empty())
            {
                DrawBar(drawList, min, ImVec2(max.x, top.y), frame.outcome, ImDrawFlags_RoundCornersTop, scale);
            }
            DrawBar(drawList, ImVec2(min.x, bottom.y), max, "Running: " + Engine::Logic::Describe(*frame.block),
                    ImDrawFlags_RoundCornersBottom, scale);
        }
        drawList->PopClipRect();
        drawList->AddRect(min, max, Colour(Theme::kInk), 6.0f * scale, ImDrawFlags_None, 1.5f * scale);
        ImGui::Dummy(ImVec2(max.x - min.x, max.y - min.y));
        ImGui::PopFont();
        if (stepCount > 1)
        {
            ImGui::TextColored(Theme::kTextMuted, "Step %zu of %zu   E: next   Q: back", scriptStep_ + 1, stepCount);
        }
    }

    const Engine::Logic::Block* BlockPreviewer::ShownBlock() const
    {
        // Only while its tooltip is up: a script last shown a moment ago is not being shown any more
        constexpr double kGone = 0.25;
        return ImGui::GetTime() - scriptSeen_ > kGone ? nullptr : shown_;
    }
}
