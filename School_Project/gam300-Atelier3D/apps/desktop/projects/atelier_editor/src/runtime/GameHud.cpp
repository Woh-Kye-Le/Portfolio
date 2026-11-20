/* !
@file    	    GameHud.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		01/11/2026
@brief
    Implements the games' on-screen display.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/runtime/GameHud.hpp"

#include "editor/Theme.hpp"
#include "engine/assets/Resources.hpp"
#include "engine/game/GameRuntime.hpp"
#include "engine/graphics/Camera/Camera3D.hpp"
#include "engine/graphics/Renderer/Renderer.hpp"
#include "engine/scene/Components.hpp"
#include "engine/scene/Scene.hpp"
#include "engine/scene/components/ScreenElementComponent.hpp"
#include "engine/scene/systems/PlayerControllerSystem.hpp"

#include <GL/glew.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Editor::Runtime
{
    namespace
    {
        constexpr double kSpeechSeconds = 4.0; // how long a speech bubble stays
        constexpr double kFadeSeconds = 0.6;   // ...fading out over its last moments
        constexpr float kBubbleHeight = 1.3f;  // how far above the object's centre its bubble points

        ImU32 Colour(const ImVec4& colour, float alpha = 1.0f)
        {
            return ImGui::GetColorU32(ImVec4(colour.x, colour.y, colour.z, colour.w * alpha));
        }

        ImFont* Display()
        {
            return Theme::DisplayFont() ? Theme::DisplayFont() : ImGui::GetFont();
        }

        ImFont* Bold()
        {
            return Theme::BoldFont() ? Theme::BoldFont() : ImGui::GetFont();
        }

        ImVec2 TextSize(ImFont* font, float size, const std::string& text, float wrap = 0.0f)
        {
            return font->CalcTextSizeA(size, FLT_MAX, wrap, text.c_str());
        }

        // A number as the HUD shows it: whole numbers without decimals
        std::string Number(double value)
        {
            return std::abs(value - std::round(value)) < 1e-6 ? std::format("{}", std::llround(value))
                                                              : std::format("{:.1f}", value);
        }

        // A screen text with its variables filled in: "{ammo}" is the value of the element's own object's variable,
        // "{Player.health}" another object's; a name with no such variable stays as it is written
        std::string WithVariables(const std::string& text, std::uint32_t objectId,
                                  const Engine::Logic::LogicRunner& blocks)
        {
            std::string out;
            std::size_t at = 0;
            while (at < text.size())
            {
                const std::size_t open = text.find('{', at);
                const std::size_t close = open == std::string::npos ? open : text.find('}', open);
                if (close == std::string::npos)
                {
                    out += text.substr(at);
                    break;
                }
                out += text.substr(at, open - at);
                const std::string name = text.substr(open + 1, close - open - 1);
                const std::size_t dot = name.find('.');
                const std::optional<double> value = dot == std::string::npos
                                                        ? blocks.VariableOfObject(objectId, name)
                                                        : blocks.VariableOf(name.substr(0, dot), name.substr(dot + 1));
                out += value ? Number(*value) : text.substr(open, close - open + 1);
                at = close + 1;
            }
            return out;
        }

        // A dark, rounded panel the HUD's parts sit on, readable over any scene
        void Panel(ImDrawList* drawList, ImVec2 min, ImVec2 max, float scale, float alpha = 0.78f)
        {
            drawList->AddRectFilled(min, max, Colour(ImVec4(0.06f, 0.07f, 0.1f, 1.0f), alpha), 12.0f * scale);
            drawList->AddRect(min, max, Colour(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 0.12f * alpha / 0.78f), 12.0f * scale,
                              ImDrawFlags_None, 1.5f * scale);
        }

        struct Player
        {
            Engine::Scene::Entity entity;
            const Engine::Scene::PlayerControllerComponent* controls = nullptr;
        };

        std::optional<Player> FindPlayer(Engine::Scene::Scene& ecs)
        {
            std::optional<Player> found;
            ecs.ForEach<Engine::Scene::PlayerControllerComponent>(
                [&](Engine::Scene::Entity entity, const Engine::Scene::PlayerControllerComponent& controls)
                {
                    if (!found)
                    {
                        found = Player{.entity = entity, .controls = &controls};
                    }
                });
            return found;
        }

        // A speech bubble pointing down at a point, with who said it over what they said
        void DrawBubble(ImDrawList* drawList, const HudView& view, ImVec2 at, const std::string& who,
                        const std::string& text, float alpha)
        {
            const float s = view.scale;
            const float name = 12.0f * s;
            const float words = 16.0f * s;
            const float wrap = 240.0f * s;
            const float pad = 10.0f * s;
            const float tail = 10.0f * s;
            const ImVec2 textSize = TextSize(Bold(), words, text, wrap);
            const float width = std::max(textSize.x, TextSize(Bold(), name, who).x) + pad * 2.0f;
            const float height = name + 2.0f * s + textSize.y + pad * 2.0f;
            // Kept inside the view
            const float left = std::clamp(at.x - width * 0.5f, view.min.x + 8.0f * s,
                                          std::max(view.min.x + 8.0f * s, view.min.x + view.size.x - width - 8.0f * s));
            const float top = std::max(at.y - tail - height, view.min.y + 8.0f * s);
            const ImVec2 min(left, top);
            const ImVec2 max(left + width, top + height);
            const ImU32 paper = Colour(ImVec4(1.0f, 1.0f, 1.0f, 0.96f), alpha);
            const ImU32 ink = Colour(Theme::kInk, alpha);
            const float tipX = std::clamp(at.x, min.x + 14.0f * s, max.x - 14.0f * s);
            drawList->AddRectFilled(ImVec2(min.x + 3.0f * s, min.y + 3.0f * s),
                                    ImVec2(max.x + 3.0f * s, max.y + 3.0f * s), Colour(ImVec4(0, 0, 0, 0.25f), alpha),
                                    12.0f * s);
            drawList->AddRectFilled(min, max, paper, 12.0f * s);
            drawList->AddTriangleFilled(ImVec2(tipX - tail * 0.8f, max.y - 1.0f),
                                        ImVec2(tipX + tail * 0.8f, max.y - 1.0f), ImVec2(tipX, max.y + tail), paper);
            drawList->AddRect(min, max, ink, 12.0f * s, ImDrawFlags_None, 2.0f * s);
            drawList->AddText(Bold(), name, ImVec2(min.x + pad, min.y + pad), Colour(Theme::kSelect, alpha),
                              who.c_str());
            drawList->AddText(Bold(), words, ImVec2(min.x + pad, min.y + pad + name + 2.0f * s), ink, text.c_str(),
                              nullptr, wrap);
        }

        void DrawSubtitle(ImDrawList* drawList, const HudView& view, float bottom, const std::string& who,
                          const std::string& text, float alpha)
        {
            const float s = view.scale;
            const float size = 18.0f * s;
            const std::string line = who + ": " + text;
            const float wrap = std::min(view.size.x - 80.0f * s, 720.0f * s);
            const ImVec2 textSize = TextSize(Bold(), size, line, wrap);
            const float pad = 10.0f * s;
            const ImVec2 min(view.min.x + (view.size.x - textSize.x) * 0.5f - pad, bottom - textSize.y - pad * 2.0f);
            const ImVec2 max(min.x + textSize.x + pad * 2.0f, bottom);
            Panel(drawList, min, max, s, 0.78f * alpha);
            drawList->AddText(Bold(), size, ImVec2(min.x + pad, min.y + pad), Colour(ImVec4(1, 1, 1, 1), alpha),
                              line.c_str(), nullptr, wrap);
        }

        void DrawClickToPlay(ImDrawList* drawList, const HudView& view)
        {
            const float s = view.scale;
            const std::string title = "Click to play";
            const std::string hint = "The mouse turns you. Esc frees it.";
            const float big = 28.0f * s;
            const float small = 15.0f * s;
            const ImVec2 titleSize = TextSize(Display(), big, title);
            const ImVec2 hintSize = TextSize(Bold(), small, hint);
            const float pad = 18.0f * s;
            const float width = std::max(titleSize.x, hintSize.x) + pad * 2.0f;
            const float height = titleSize.y + hintSize.y + 6.0f * s + pad * 2.0f;
            const ImVec2 min(view.min.x + (view.size.x - width) * 0.5f, view.min.y + (view.size.y - height) * 0.5f);
            const ImVec2 max(min.x + width, min.y + height);
            Panel(drawList, min, max, s, 0.85f);
            drawList->AddText(Display(), big, ImVec2(min.x + (width - titleSize.x) * 0.5f, min.y + pad),
                              Colour(ImVec4(1, 1, 1, 1)), title.c_str());
            drawList->AddText(Bold(), small,
                              ImVec2(min.x + (width - hintSize.x) * 0.5f, min.y + pad + titleSize.y + 6.0f * s),
                              Colour(ImVec4(1, 1, 1, 0.75f)), hint.c_str());
        }
    }

    namespace
    {
        // Where a point of the world is on the game view (nullopt: behind the camera, or off the view)
        std::optional<ImVec2> OnScreen(const std::optional<glm::mat4>& viewProjection, const HudView& view,
                                       const glm::vec3& point)
        {
            if (!viewProjection)
            {
                return std::nullopt;
            }
            const glm::vec4 clip = *viewProjection * glm::vec4(point, 1.0f);
            if (clip.w <= 0.1f)
            {
                return std::nullopt;
            }
            const ImVec2 at(view.min.x + (clip.x / clip.w * 0.5f + 0.5f) * view.size.x,
                            view.min.y + (0.5f - clip.y / clip.w * 0.5f) * view.size.y);
            if (at.x < view.min.x || at.x > view.min.x + view.size.x || at.y < view.min.y ||
                at.y > view.min.y + view.size.y)
            {
                return std::nullopt;
            }
            return at;
        }

        // The texture loader keeps OpenGL's order, bottom row first, so a picture's top is at v = 1
        constexpr ImVec2 kTopLeftUv(0.0f, 1.0f);
        constexpr ImVec2 kBottomRightUv(1.0f, 0.0f);

        // A picture's texture, made once from its pixels (by the file it came from)
        struct Uploaded
        {
            GLuint texture = 0;
            int width = 0;
            int height = 0;
        };

        const Uploaded* Upload(const std::string& path, const Engine::Assets::Texture& picture)
        {
            static std::map<std::string, Uploaded> uploaded;
            const auto found = uploaded.find(path);
            if (found != uploaded.end() && found->second.width == picture.width &&
                found->second.height == picture.height)
            {
                return &found->second;
            }
            if (picture.rgba.size() <
                static_cast<std::size_t>(picture.width) * static_cast<std::size_t>(picture.height) * 4)
            {
                return nullptr;
            }
            GLint previous = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
            Uploaded made{.width = picture.width, .height = picture.height};
            glGenTextures(1, &made.texture);
            glBindTexture(GL_TEXTURE_2D, made.texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, picture.width, picture.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         picture.rgba.data());
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));
            if (found != uploaded.end())
            {
                glDeleteTextures(1, &found->second.texture);
                found->second = made;
                return &found->second;
            }
            return &uploaded.emplace(path, made).first->second;
        }

    }

    ImVec2 ScreenElementCentre(const HudView& view, const Engine::Scene::ScreenElementComponent& element)
    {
        const auto [ax, ay] = Engine::Scene::AnchorPoint(element.anchor);
        return {view.min.x + view.size.x * (ax + element.x / 100.0f),
                view.min.y + view.size.y * (ay + element.y / 100.0f)};
    }

    std::pair<ImVec2, ImVec2> DrawScreenElement(ImDrawList* drawList, const HudView& view,
                                                const Engine::Scene::ScreenElementComponent& element, double barValue,
                                                Engine::Game::GameRuntime* pictures, float alpha)
    {
        using Kind = Engine::Scene::ScreenElementKind;
        const float s = view.scale;
        const ImVec2 centre = ScreenElementCentre(view, element);
        const float height = std::max(view.size.y * element.height / 100.0f, 4.0f);
        const float fixedWidth = element.width > 0.0f ? view.size.y * element.width / 100.0f : 0.0f;
        const ImVec4 colour(element.colorR, element.colorG, element.colorB, 1.0f);
        // Its picture: what a picture shows, and the game's own look for a button or a bar (none: the plain one)
        std::string path;
        const Engine::Assets::Texture* picture =
            pictures && element.kind != Kind::Text ? pictures->ScreenPicture(element.picture, &path) : nullptr;
        const Uploaded* uploaded = picture ? Upload(path, *picture) : nullptr;
        if (element.kind == Kind::Text)
        {
            const ImVec2 size = TextSize(Display(), height, element.text);
            const ImVec2 min(centre.x - size.x * 0.5f, centre.y - size.y * 0.5f);
            const float shadow = std::max(2.0f * s, height * 0.06f);
            drawList->AddText(Display(), height, ImVec2(min.x + shadow, min.y + shadow),
                              Colour(ImVec4(0, 0, 0, 0.55f), alpha), element.text.c_str());
            drawList->AddText(Display(), height, min, Colour(colour, alpha), element.text.c_str());
            return {min, ImVec2(min.x + size.x, min.y + size.y)};
        }
        if (element.kind == Kind::Button)
        {
            const float labelSize = height * 0.5f;
            const ImVec2 label = TextSize(Display(), labelSize, element.text);
            // A picture keeps its proportions unless a width is given or the label needs more
            const float pictureWidth =
                uploaded ? height * static_cast<float>(uploaded->width) / static_cast<float>(uploaded->height) : 0.0f;
            const float width = fixedWidth > 0.0f ? fixedWidth : std::max(label.x + height * 1.2f, pictureWidth);
            const ImVec2 min(centre.x - width * 0.5f, centre.y - height * 0.5f);
            const ImVec2 max(min.x + width, min.y + height);
            const ImVec2 labelAt(centre.x - label.x * 0.5f, centre.y - label.y * 0.5f);
            if (uploaded)
            {
                // Light on a shadow, to read on whatever the picture is
                drawList->AddImage(static_cast<ImTextureID>(uploaded->texture), min, max, kTopLeftUv, kBottomRightUv,
                                   Colour(colour, alpha));
                drawList->AddText(Display(), labelSize, ImVec2(labelAt.x + 1.0f * s, labelAt.y + 1.0f * s),
                                  Colour(ImVec4(0, 0, 0, 0.7f), alpha), element.text.c_str());
                drawList->AddText(Display(), labelSize, labelAt, Colour(ImVec4(1, 1, 1, 1), alpha),
                                  element.text.c_str());
                return {min, max};
            }
            const float rounding = height * 0.32f;
            drawList->AddRectFilled(ImVec2(min.x + 4.0f * s, min.y + 4.0f * s),
                                    ImVec2(max.x + 4.0f * s, max.y + 4.0f * s), Colour(ImVec4(0, 0, 0, 0.45f), alpha),
                                    rounding);
            drawList->AddRectFilled(min, max, Colour(colour, alpha), rounding);
            drawList->AddText(Display(), labelSize, labelAt, Colour(Theme::kInk, alpha), element.text.c_str());
            return {min, max};
        }
        if (element.kind == Kind::Bar)
        {
            const float width = fixedWidth > 0.0f ? fixedWidth : height * 6.0f;
            const ImVec2 min(centre.x - width * 0.5f, centre.y - height * 0.5f);
            const ImVec2 max(min.x + width, min.y + height);
            const float fraction = element.barMaximum > 0.0f
                                       ? static_cast<float>(std::clamp(barValue / element.barMaximum, 0.0, 1.0))
                                       : 0.0f;
            drawList->AddRectFilled(ImVec2(min.x - 3.0f * s, min.y - 3.0f * s),
                                    ImVec2(max.x + 3.0f * s, max.y + 3.0f * s),
                                    Colour(ImVec4(0.06f, 0.07f, 0.1f, 0.78f), alpha), uploaded ? 0.0f : 999.0f);
            if (fraction > 0.0f && uploaded)
            {
                // The picture shown as far as the bar is full, not squeezed into it
                drawList->AddImage(static_cast<ImTextureID>(uploaded->texture), min,
                                   ImVec2(min.x + width * fraction, max.y), kTopLeftUv,
                                   ImVec2(fraction, kBottomRightUv.y), Colour(colour, alpha));
            }
            else if (fraction > 0.0f)
            {
                drawList->AddRectFilled(min, ImVec2(min.x + std::max(width * fraction, height), max.y),
                                        Colour(colour, alpha), 999.0f);
            }
            if (element.text.empty())
            {
                return {min, max};
            }
            const std::string label =
                std::format("{}  {} / {}", element.text, Number(barValue), Number(element.barMaximum));
            const float labelSize = height * 0.62f;
            const ImVec2 extent = TextSize(Bold(), labelSize, label);
            const ImVec2 at(centre.x - extent.x * 0.5f, centre.y - extent.y * 0.5f);
            drawList->AddText(Bold(), labelSize, ImVec2(at.x + 1.0f * s, at.y + 1.0f * s),
                              Colour(ImVec4(0, 0, 0, 0.6f), alpha), label.c_str());
            drawList->AddText(Bold(), labelSize, at, Colour(ImVec4(1, 1, 1, 1), alpha), label.c_str());
            return {min, max};
        }
        // A picture, as wide as its proportions make it (no picture: as wide as it is given, else square)
        const float width = uploaded
                                ? height * static_cast<float>(uploaded->width) / static_cast<float>(uploaded->height)
                            : fixedWidth > 0.0f ? fixedWidth
                                                : height;
        const ImVec2 min(centre.x - width * 0.5f, centre.y - height * 0.5f);
        const ImVec2 max(min.x + width, min.y + height);
        if (uploaded)
        {
            drawList->AddImage(static_cast<ImTextureID>(uploaded->texture), min, max, kTopLeftUv, kBottomRightUv,
                               Colour(colour, alpha));
        }
        else
        {
            // No such picture (or none chosen yet): a plain see-through box in its colour, outlined
            drawList->AddRectFilled(min, max, Colour(ImVec4(colour.x, colour.y, colour.z, 0.35f), alpha), 6.0f * s);
            drawList->AddRect(min, max, Colour(ImVec4(colour.x, colour.y, colour.z, 0.9f), alpha), 6.0f * s,
                              ImDrawFlags_None, 2.0f * s);
        }
        return {min, max};
    }

    std::vector<ScreenTarget> DrawGameHud(ImDrawList* drawList, const HudView& view, Engine::Game::GameRuntime& runtime)
    {
        std::vector<ScreenTarget> targets;
        Engine::Scene::Scene* ecs = runtime.Ecs();
        if (drawList == nullptr || ecs == nullptr || view.size.x < 64.0f || view.size.y < 64.0f)
        {
            return targets;
        }
        drawList->PushClipRect(view.min, ImVec2(view.min.x + view.size.x, view.min.y + view.size.y), true);
        const Engine::Logic::LogicRunner& blocks = runtime.Blocks();
        const std::optional<Player> player = FindPlayer(*ecs);
        const bool firstPerson =
            player && player->controls->followCamera &&
            player->controls->cameraOffsetZ <= Engine::Systems::PlayerControllerSystem::kMinDistance + 0.5f;
        std::optional<glm::mat4> viewProjection;
        if (const Engine::Graphics::Camera3D* camera = runtime.Camera())
        {
            const glm::mat4 projection = glm::perspective(
                glm::radians(Engine::Renderer::Renderer::kFieldOfViewDegrees), view.size.x / view.size.y,
                Engine::Renderer::Renderer::kNearPlane, Engine::Renderer::Renderer::kFarPlane);
            viewProjection = projection * camera->GetViewMatrix();
        }

        // The Screen Elements, lowest layer first; what can be clicked is handed back
        struct Shown
        {
            const Engine::Scene::ScreenElementComponent* element;
            std::uint32_t objectId;
            Engine::Scene::Entity entity;
        };
        std::vector<Shown> shown;
        ecs->ForEach<Engine::Scene::ScreenElementComponent>(
            [&](Engine::Scene::Entity entity, const Engine::Scene::ScreenElementComponent& element)
            {
                if (element.visible)
                {
                    shown.push_back({&element, runtime.ObjectOf(entity.GetId()), entity});
                }
            });
        std::stable_sort(shown.begin(), shown.end(),
                         [](const Shown& a, const Shown& b) { return a.element->layer < b.element->layer; });
        for (const Shown& each : shown)
        {
            Engine::Scene::ScreenElementComponent element = *each.element;
            if (element.text.find('{') != std::string::npos)
            {
                element.text = WithVariables(element.text, each.objectId, blocks);
            }
            double value = 0.0;
            if (element.kind == Engine::Scene::ScreenElementKind::Bar)
            {
                value = element.barObject.empty()
                            ? blocks.VariableOfObject(each.objectId, element.barVariable).value_or(0.0)
                            : blocks.VariableOf(element.barObject, element.barVariable).value_or(0.0);
            }
            if (element.aboveObject)
            {
                // Over its object, for as long as that is in view; a bar that has run out is gone
                const auto where = ecs->GetTransform(each.entity);
                if (!where || (element.kind == Engine::Scene::ScreenElementKind::Bar && value <= 0.0))
                {
                    continue;
                }
                const float top = where->scale * std::max({where->scaleX, where->scaleY, where->scaleZ}) + 0.3f;
                const auto at = OnScreen(viewProjection, view, glm::vec3(where->x, where->y + top, where->z));
                if (!at)
                {
                    continue;
                }
                element.anchor = Engine::Scene::ScreenAnchor::TopLeft;
                element.x = (at->x - view.min.x) / view.size.x * 100.0f;
                element.y = (at->y - view.min.y) / view.size.y * 100.0f + element.y;
            }
            const auto [min, max] = DrawScreenElement(drawList, view, element, value, &runtime);
            if (element.kind == Engine::Scene::ScreenElementKind::Button || blocks.HasScreenClickScript(each.objectId))
            {
                const bool hovered = !view.pointerLocked && view.mouse.x >= min.x && view.mouse.x <= max.x &&
                                     view.mouse.y >= min.y && view.mouse.y <= max.y;
                if (hovered)
                {
                    drawList->AddRectFilled(min, max, Colour(ImVec4(1, 1, 1, 0.2f)), (max.y - min.y) * 0.32f);
                }
                targets.push_back({.objectId = each.objectId, .min = min, .max = max});
            }
        }
        // Speech: a bubble over whoever said it, or a subtitle when they are off screen (or are the player, seen from
        // behind their eyes)
        float subtitleBottom = view.min.y + view.size.y - 18.0f * view.scale;
        for (const auto& speech : blocks.Speeches(kSpeechSeconds))
        {
            const float alpha = static_cast<float>(std::clamp((kSpeechSeconds - speech.age) / kFadeSeconds, 0.0, 1.0));
            const Engine::Scene::Entity entity = ecs->FindEntity(speech.entity);
            const auto where = entity ? ecs->GetTransform(entity) : std::nullopt;
            const bool isPlayer = player && entity == player->entity;
            const std::optional<ImVec2> onScreen =
                where && !(isPlayer && firstPerson)
                    ? OnScreen(viewProjection, view, glm::vec3(where->x, where->y + kBubbleHeight, where->z))
                    : std::nullopt;
            if (onScreen)
            {
                DrawBubble(drawList, view, *onScreen, speech.objectName, speech.text, alpha);
            }
            else
            {
                DrawSubtitle(drawList, view, subtitleBottom, speech.objectName, speech.text, alpha);
                subtitleBottom -= 54.0f * view.scale;
            }
        }

        // Until the game has the mouse, unless it shows something to click (a menu)
        if (!view.pointerLocked && targets.empty())
        {
            DrawClickToPlay(drawList, view);
        }
        drawList->PopClipRect();
        return targets;
    }
}
