/* !
@file    	    SceneEditorViewport.cpp
@author  	    sanni.go@digipen.edu [1%] 
         	    c.kojat@digipen.edu [92%]
         	    w.kyele@digipen.edu [7%]
@date    		30/09/2026
@brief
    Implements the Viewport window of SceneEditor: the 3D view (game
    framebuffer, selection outline, ImGuizmo transform gizmo, view cube, click
    picking), the 2D front view and the viewport toolbar.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// The editor's Viewport: toolbar, the 3D view (transform gizmos, view cube, click selection, selection outline) and
// the 2D front view (the rest of SceneEditor is in SceneEditor.cpp)
#include "editor/SceneEditor.hpp"

#include "editor/Theme.hpp"
#include "editor/panels/assets/AssetDragDrop.hpp"
#include "editor/runtime/GameHud.hpp"

#include "engine/core/Log.hpp"
#include "engine/graphics/Renderer/Renderer.hpp"
#include "engine/scene/Hierarchy.hpp"

#include <ImGuizmo/ImGuizmo.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <GL/glew.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <fstream>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace Editor
{
    namespace
    {
        using Engine::Editor::GameObject;
        using Engine::Game::Shape;
        using Engine::Scene::RenderableDesc;
        using Engine::Scene::TransformComponent;

        void ImDrawCallback_DisableBlend(const ImDrawList*, const ImDrawCmd*)
        {
            glDisable(GL_BLEND);
        }
        void ImDrawCallback_RestoreBlend(const ImDrawList*, const ImDrawCmd*)
        {
            glEnable(GL_BLEND);
        }

        constexpr ImU32 kSelectionColor = IM_COL32(255, 170, 40, 255);

        // 2D view: zoom per wheel step, the closest grid lines may be (before the grid switches to a coarser step), and
        // the smallest an object is drawn (half its width / height; flat shapes stay visible and clickable edge-on)
        constexpr float kZoomPerWheelStep2D = 1.15f;
        constexpr float kMinGridSpacing2D = 16.0f;
        constexpr float kMinObjectHalfSize2D = 3.0f;

        // An object is in the scene's picture when it has a place (Transform) and something to draw (Renderable)
        bool IsDrawn(const GameObject& object)
        {
            return object.transform && object.renderable;
        }

        glm::mat4 ModelOf(const TransformComponent& t)
        {
            return Engine::Scene::ToGraphicsTransform(t).ComposeModelMatrix();
        }

        // Half the size of each built-in shape at scale 1 (a model file is treated like a unit cube)
        // What a click on part of something selects first: the whole of it. That is its prefab copy, or else the
        // object at the top of its group (a player whose body parts are its children)
        std::uint32_t WholeOf(const Engine::Scene::SceneDescription& scene, std::uint32_t id)
        {
            if (const std::uint32_t root = Engine::Scene::InstanceRootOf(scene, id); root != 0)
            {
                return root;
            }
            std::uint32_t top = id;
            for (std::size_t guard = 0; guard < scene.objects.size(); ++guard) // a loop of parents ends here
            {
                const auto found =
                    std::find_if(scene.objects.begin(), scene.objects.end(),
                                 [top](const Engine::Scene::GameObject& each) { return each.id == top; });
                if (found == scene.objects.end() || found->parent == 0)
                {
                    break;
                }
                top = found->parent;
            }
            return top;
        }

        glm::vec3 HalfExtentsOf(const RenderableDesc& renderable)
        {
            if (!renderable.mesh.empty())
            {
                return glm::vec3(0.5f);
            }
            switch (renderable.shape)
            {
            case Shape::Sphere: return glm::vec3(1.0f);
            case Shape::Pyramid: return {0.7f, 0.6f, 0.7f};
            case Shape::Plane: return {0.5f, 0.01f, 0.5f};
            case Shape::Quad:
            case Shape::Circle: return {0.5f, 0.5f, 0.01f};
            case Shape::Line: return {0.5f, 0.02f, 0.02f};
            default: return glm::vec3(0.5f);
            }
        }

        // Where an object is in the 2D view, in screen units: its position, and its shape's X / Y size times its scale
        // (rotation is not shown), from where it is in the world (world). Call only for objects that IsDrawn.
        struct Rect2D
        {
            ImVec2 center;
            ImVec2 half;
        };
        Rect2D Rect2DOf(const GameObject& object, const TransformComponent& world, const Camera::View2D& view,
                        float minHalfSize)
        {
            if (!object.renderable)
            {
                return {}; // callers only pass objects that IsDrawn
            }
            const TransformComponent& t = world;
            const glm::vec2 center = view.WorldToScreen({t.x, t.y});
            const glm::vec3 extents =
                HalfExtentsOf(*object.renderable) * t.scale * glm::abs(glm::vec3(t.scaleX, t.scaleY, t.scaleZ));
            const glm::vec2 half = glm::max(glm::vec2(extents) * view.Zoom(), glm::vec2(minHalfSize));
            return {ImVec2(center.x, center.y), ImVec2(half.x, half.y)};
        }

        // A line of help along the bottom of the view, on a dark band so it reads over any background
        // "60.0 FPS | 1107 entities" in a dark box at the view's top-left corner (the 3D and 2D views share it)
        // 48210 as "48,210"
        std::string Thousands(std::size_t number)
        {
            std::string digits = std::to_string(number);
            for (int at = static_cast<int>(digits.size()) - 3; at > 0; at -= 3)
            {
                digits.insert(static_cast<std::size_t>(at), ",");
            }
            return digits;
        }

        // The level of detail part of the stats line: how many models are drawn at each level, and the triangles drawn
        // against what the full models have ("LOD 0/1/2: 3/1/0 | 61,204 of 98,330 tris")
        std::string LodText(const Engine::Game::GameRuntime::LodStats& stats)
        {
            if (stats.fullTriangles == 0)
            {
                return {};
            }
            return std::format("LOD 0/1/2: {}/{}/{} | {} of {} tris", stats.objects[0], stats.objects[1],
                               stats.objects[2], Thousands(stats.triangles), Thousands(stats.fullTriangles));
        }

        void DrawStats(ImDrawList* drawList, const ImVec2& corner, double deltaSeconds, std::size_t entities,
                       float uiScale, const std::string& extra = {})
        {
            const double fps = deltaSeconds > 0.0 ? 1.0 / deltaSeconds : 0.0;
            const std::string text =
                std::format("{:.1f} FPS | {} entities", fps, entities) + (extra.empty() ? "" : " | " + extra);
            const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
            const float margin = 8.0f * uiScale;
            const float padX = 8.0f * uiScale;
            const float padY = 4.0f * uiScale;
            const ImVec2 boxMin(corner.x + margin, corner.y + margin);
            const ImVec2 boxMax(boxMin.x + textSize.x + padX * 2.0f, boxMin.y + textSize.y + padY * 2.0f);
            drawList->AddRectFilled(boxMin, boxMax, IM_COL32(15, 18, 24, 220), 4.0f * uiScale);
            drawList->AddText(ImVec2(boxMin.x + padX, boxMin.y + padY), IM_COL32(210, 235, 255, 255), text.c_str());
        }

        void DrawHint(ImDrawList* drawList, const ImVec2& position, const ImVec2& size, const char* hint, float uiScale)
        {
            const float margin = 8.0f * uiScale;
            const ImVec2 hintSize = ImGui::CalcTextSize(hint);
            const ImVec2 hintAt(position.x + margin, position.y + size.y - hintSize.y - margin);
            drawList->AddRectFilled(
                ImVec2(hintAt.x - 4.0f * uiScale, hintAt.y - 2.0f * uiScale),
                ImVec2(hintAt.x + hintSize.x + 4.0f * uiScale, hintAt.y + hintSize.y + 2.0f * uiScale),
                IM_COL32(12, 14, 18, 150), 3.0f * uiScale);
            drawList->AddText(hintAt, IM_COL32(220, 224, 232, 230), hint);
        }

        // World point to viewport pixels; false if it is behind the camera
        bool ToScreen(const glm::mat4& viewProjection, const glm::vec3& point, const ImVec2& position,
                      const ImVec2& size, ImVec2& out)
        {
            const glm::vec4 clip = viewProjection * glm::vec4(point, 1.0f);
            if (clip.w <= 1e-4f)
            {
                return false;
            }
            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            out = ImVec2(position.x + (ndc.x * 0.5f + 0.5f) * size.x, position.y + (0.5f - ndc.y * 0.5f) * size.y);
            return true;
        }

        // The ray from the camera through a point of the viewport: origin on the near plane, unit direction
        struct Ray
        {
            glm::vec3 origin;
            glm::vec3 direction;
        };
        // How far along a ray (in the units of its direction) it first meets one of a model's triangles, both sides
        // counting (a model's back faces are drawn too); nullopt when it misses them all. The model is turned by turn
        // degrees round Y inside its object, so the ray is turned the other way to meet it.
        std::optional<float> NearestTriangle(const std::vector<glm::vec3>& triangles, glm::vec3 origin,
                                             glm::vec3 direction, float turn)
        {
            if (turn != 0.0f)
            {
                const glm::mat4 back = glm::rotate(glm::mat4(1.0f), glm::radians(-turn), glm::vec3(0.0f, 1.0f, 0.0f));
                origin = glm::vec3(back * glm::vec4(origin, 1.0f));
                direction = glm::vec3(back * glm::vec4(direction, 0.0f));
            }
            std::optional<float> nearest;
            for (std::size_t i = 0; i + 2 < triangles.size(); i += 3)
            {
                // Moller-Trumbore
                const glm::vec3 edge1 = triangles[i + 1] - triangles[i];
                const glm::vec3 edge2 = triangles[i + 2] - triangles[i];
                const glm::vec3 p = glm::cross(direction, edge2);
                const float determinant = glm::dot(edge1, p);
                if (std::abs(determinant) < 1e-12f)
                {
                    continue; // the ray runs along the triangle
                }
                const float inverse = 1.0f / determinant;
                const glm::vec3 fromCorner = origin - triangles[i];
                const float u = glm::dot(fromCorner, p) * inverse;
                if (u < 0.0f || u > 1.0f)
                {
                    continue;
                }
                const glm::vec3 q = glm::cross(fromCorner, edge1);
                const float v = glm::dot(direction, q) * inverse;
                if (v < 0.0f || u + v > 1.0f)
                {
                    continue;
                }
                const float t = glm::dot(edge2, q) * inverse;
                if (t > 0.0f && (!nearest || t < *nearest))
                {
                    nearest = t;
                }
            }
            return nearest;
        }

        Ray RayThrough(const glm::mat4& viewProjection, const ImVec2& position, const ImVec2& size, const ImVec2& mouse)
        {
            const float ndcX = (mouse.x - position.x) / size.x * 2.0f - 1.0f;
            const float ndcY = 1.0f - (mouse.y - position.y) / size.y * 2.0f;
            const glm::mat4 inverse = glm::inverse(viewProjection);
            glm::vec4 nearPoint = inverse * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
            glm::vec4 farPoint = inverse * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
            nearPoint /= nearPoint.w;
            farPoint /= farPoint.w;
            const glm::vec3 origin(nearPoint);
            return {origin, glm::normalize(glm::vec3(farPoint) - origin)};
        }

        // Where the ray meets the ground (y = 0); a point 10 units along it when it never does (looking up or level)
        glm::vec3 GroundPointOf(const Ray& ray)
        {
            constexpr float kMissDistance = 10.0f;
            if (ray.direction.y < -1e-4f)
            {
                const float t = -ray.origin.y / ray.direction.y;
                if (t > 0.0f)
                {
                    return ray.origin + ray.direction * t;
                }
            }
            return ray.origin + ray.direction * kMissDistance;
        }

        bool PointInQuad(const std::array<ImVec2, 4>& quad, const ImVec2& point)
        {
            float sign = 0.0f;
            for (std::size_t i = 0; i < quad.size(); ++i)
            {
                const ImVec2& a = quad[i];
                const ImVec2& b = quad[(i + 1) % quad.size()];
                const float cross = (b.x - a.x) * (point.y - a.y) - (b.y - a.y) * (point.x - a.x);
                if (cross != 0.0f)
                {
                    if (sign != 0.0f && (cross > 0.0f) != (sign > 0.0f))
                    {
                        return false;
                    }
                    sign = cross;
                }
            }
            return true;
        }

        // The Advanced toolbar's row, tighter than a Simple pill: it has many more buttons to fit in
        constexpr float kToolHeight = 30.0f;
        constexpr float kToolPaddingSaved = 18.0f;

        float ToolWidth(const char* label)
        {
            const float scale = ImGui::GetStyle().FontScaleDpi;
            return Theme::PillWidth(label, kToolHeight * scale) - kToolPaddingSaved * scale;
        }

        // A tool button that stays highlighted while its tool is on
        bool ToolButton(const char* label, bool active, const char* tooltip)
        {
            const bool clicked =
                Theme::PillButton(label, active ? Theme::kEvents : Theme::kRaised, active ? Theme::kInk : Theme::kText,
                                  kToolHeight * ImGui::GetStyle().FontScaleDpi, ToolWidth(label));
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("%s", tooltip);
            }
            return clicked;
        }

        // The gizmo tools: both toolbars show them as these icon buttons
        constexpr std::array<std::tuple<GizmoTool, Theme::Icon, const char*>, 3> kGizmoTools{
            std::tuple{GizmoTool::Move, Theme::Icon::Move, "Move (W)"},
            std::tuple{GizmoTool::Rotate, Theme::Icon::Rotate, "Rotate (E)"},
            std::tuple{GizmoTool::Scale, Theme::Icon::Scale, "Resize (R)"}};
    }

    namespace
    {
        constexpr const char* kThumbnailFile = "data/thumbnail.editor.bmp";
        constexpr int kThumbnailWidth = 320;
        constexpr int kBmpHeaderSize = 54; // BITMAPFILEHEADER + BITMAPINFOHEADER

        void PutLittleEndian(std::vector<unsigned char>& bytes, std::size_t at, std::uint32_t value, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                bytes[at + static_cast<std::size_t>(i)] = static_cast<unsigned char>((value >> (8 * i)) & 0xFFu);
            }
        }

        std::uint32_t GetLittleEndian(const std::vector<unsigned char>& bytes, std::size_t at, int count)
        {
            std::uint32_t value = 0;
            for (int i = 0; i < count; ++i)
            {
                value |= static_cast<std::uint32_t>(bytes[at + static_cast<std::size_t>(i)]) << (8 * i);
            }
            return value;
        }
    }

    // The view, made small (each pixel the average of the ones it covers) and written as a 24-bit BMP. Rows go
    // bottom to top in both OpenGL and BMP, so they are written in the order they are read.
    void SceneEditor::SaveThumbnail()
    {
        if (projectDir_.empty() || view_ != EditorView::Workspace || InPrefabMode() || !viewportFbo_.IsValid())
        {
            return;
        }
        const int width = viewportFbo_.Width();
        const int height = viewportFbo_.Height();
        if (width < 16 || height < 16)
        {
            return;
        }
        std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
        GLint previous = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(viewportFbo_.GetColorTextureID()));
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));

        const int outWidth = std::min(width, kThumbnailWidth);
        const int outHeight = std::max(1, height * outWidth / width);
        const std::size_t stride = (static_cast<std::size_t>(outWidth) * 3 + 3) & ~static_cast<std::size_t>(3);
        std::vector<unsigned char> bmp(kBmpHeaderSize + stride * static_cast<std::size_t>(outHeight), 0);
        bmp[0] = 'B';
        bmp[1] = 'M';
        PutLittleEndian(bmp, 2, static_cast<std::uint32_t>(bmp.size()), 4);
        PutLittleEndian(bmp, 10, kBmpHeaderSize, 4);
        PutLittleEndian(bmp, 14, 40, 4);
        PutLittleEndian(bmp, 18, static_cast<std::uint32_t>(outWidth), 4);
        PutLittleEndian(bmp, 22, static_cast<std::uint32_t>(outHeight), 4);
        PutLittleEndian(bmp, 26, 1, 2);  // planes
        PutLittleEndian(bmp, 28, 24, 2); // bits per pixel
        for (int y = 0; y < outHeight; ++y)
        {
            const int y0 = y * height / outHeight;
            const int y1 = std::max(y0 + 1, (y + 1) * height / outHeight);
            for (int x = 0; x < outWidth; ++x)
            {
                const int x0 = x * width / outWidth;
                const int x1 = std::max(x0 + 1, (x + 1) * width / outWidth);
                std::array<std::uint32_t, 3> sum{};
                for (int sy = y0; sy < y1; ++sy)
                {
                    for (int sx = x0; sx < x1; ++sx)
                    {
                        const std::size_t at = (static_cast<std::size_t>(sy) * static_cast<std::size_t>(width) +
                                                static_cast<std::size_t>(sx)) *
                                               4;
                        sum[0] += pixels[at];
                        sum[1] += pixels[at + 1];
                        sum[2] += pixels[at + 2];
                    }
                }
                const auto count = static_cast<std::uint32_t>((y1 - y0) * (x1 - x0));
                const std::size_t out =
                    kBmpHeaderSize + static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) * 3;
                bmp[out] = static_cast<unsigned char>(sum[2] / count); // BMP is blue, green, red
                bmp[out + 1] = static_cast<unsigned char>(sum[1] / count);
                bmp[out + 2] = static_cast<unsigned char>(sum[0] / count);
            }
        }

        std::error_code ec;
        const std::filesystem::path path = projectDir_ / kThumbnailFile;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(bmp.data()), static_cast<std::streamsize>(bmp.size()));
        if (!file)
        {
            LOG_WARN(Editor, "Could not save the game's picture to {}", path.string());
            return;
        }
        // The Home page loads the new one
        if (const auto found = thumbnails_.find(projectDir_); found != thumbnails_.end())
        {
            const GLuint texture = found->second.texture;
            if (texture != 0)
            {
                glDeleteTextures(1, &texture);
            }
            thumbnails_.erase(found);
        }
    }

    // The game's picture as a texture, read once (only BMPs written by SaveThumbnail: 24-bit, uncompressed)
    const SceneEditor::Thumbnail& SceneEditor::ThumbnailOf(const std::filesystem::path& project)
    {
        if (const auto found = thumbnails_.find(project); found != thumbnails_.end())
        {
            return found->second;
        }
        Thumbnail& thumbnail = thumbnails_[project];
        std::ifstream file(project / kThumbnailFile, std::ios::binary);
        if (!file)
        {
            return thumbnail;
        }
        const std::vector<unsigned char> bmp((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (bmp.size() < kBmpHeaderSize || bmp[0] != 'B' || bmp[1] != 'M' || GetLittleEndian(bmp, 28, 2) != 24 ||
            GetLittleEndian(bmp, 30, 4) != 0)
        {
            return thumbnail;
        }
        const auto width = static_cast<int>(GetLittleEndian(bmp, 18, 4));
        const auto height = static_cast<int>(GetLittleEndian(bmp, 22, 4));
        const std::size_t offset = GetLittleEndian(bmp, 10, 4);
        if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
        {
            return thumbnail;
        }
        const std::size_t stride = (static_cast<std::size_t>(width) * 3 + 3) & ~static_cast<std::size_t>(3);
        if (bmp.size() < offset + stride * static_cast<std::size_t>(height))
        {
            return thumbnail;
        }
        std::vector<unsigned char> rgba(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const std::size_t in = offset + static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) * 3;
                const std::size_t out =
                    (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4;
                rgba[out] = bmp[in + 2];
                rgba[out + 1] = bmp[in + 1];
                rgba[out + 2] = bmp[in];
                rgba[out + 3] = 255;
            }
        }
        GLint previous = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));
        thumbnail = Thumbnail{texture, width, height};
        return thumbnail;
    }

    void SceneEditor::FreeThumbnails()
    {
        for (const auto& [project, thumbnail] : thumbnails_)
        {
            if (thumbnail.texture != 0)
            {
                glDeleteTextures(1, &thumbnail.texture);
            }
        }
        thumbnails_.clear();
    }

    bool SceneEditor::EditScreenElements(const ImVec2& position, const ImVec2& size)
    {
        const Runtime::HudView view{.min = position, .size = size, .scale = uiScale_};
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImGuiIO& io = ImGui::GetIO();

        // Screen editing follows the selection: an element chosen here or in the Objects list turns it on, any other
        // object turns it off (letting go of the selection changes nothing)
        if (scene_.Selected() != screenEditingFor_)
        {
            screenEditingFor_ = scene_.Selected();
            if (const GameObject* chosen = scene_.Find(screenEditingFor_))
            {
                screenEditing_ = chosen->screen.has_value();
            }
        }

        // As the game shows them, lowest layer first; a bar as full as its variable starts. Faint while the 3D
        // objects have the clicks, and fainter still if they start hidden.
        std::vector<GameObject*> elements;
        for (GameObject& object : scene_.Objects())
        {
            // One over an object in the world has no place on the screen to drag it to
            if (object.screen && !object.screen->aboveObject)
            {
                elements.push_back(&object);
            }
        }
        std::stable_sort(elements.begin(), elements.end(),
                         [](const GameObject* a, const GameObject* b) { return a->screen->layer < b->screen->layer; });
        struct Drawn
        {
            std::uint32_t id;
            ImVec2 min;
            ImVec2 max;
        };
        std::vector<Drawn> drawn;
        for (GameObject* object : elements)
        {
            if (!object->screen)
            {
                continue;
            }
            const auto& element = *object->screen;
            double value = 0.0;
            for (const GameObject& owner : scene_.Objects())
            {
                if ((element.barObject.empty() ? owner.id == object->id : owner.name == element.barObject) &&
                    owner.logic)
                {
                    if (const auto* variable = Engine::Logic::FindVariable(*owner.logic, element.barVariable))
                    {
                        value = variable->value;
                    }
                }
            }
            const float alpha = (element.visible ? 1.0f : 0.35f) * (screenEditing_ ? 1.0f : 0.45f);
            const auto [min, max] = Runtime::DrawScreenElement(drawList, view, element, value, runtime_.get(), alpha);
            drawn.push_back({object->id, min, max});
        }
        if (!screenEditing_)
        {
            screenDragId_ = 0;
            return false; // the 3D objects have the view's clicks
        }

        // The selected one: outlined, with a handle at its bottom-right corner to resize it
        const float handle = 9.0f * uiScale_;
        const auto selected = std::find_if(drawn.begin(), drawn.end(),
                                           [this](const Drawn& each) { return each.id == scene_.Selected(); });
        bool overHandle = false;
        if (selected != drawn.end())
        {
            drawList->AddRect(ImVec2(selected->min.x - 4.0f * uiScale_, selected->min.y - 4.0f * uiScale_),
                              ImVec2(selected->max.x + 4.0f * uiScale_, selected->max.y + 4.0f * uiScale_),
                              ImGui::GetColorU32(Theme::kSelect), 6.0f * uiScale_, ImDrawFlags_None, 2.0f * uiScale_);
            const ImVec2 corner(selected->max.x + 4.0f * uiScale_, selected->max.y + 4.0f * uiScale_);
            drawList->AddRectFilled(ImVec2(corner.x - handle, corner.y - handle),
                                    ImVec2(corner.x + handle, corner.y + handle), ImGui::GetColorU32(Theme::kSelect),
                                    3.0f * uiScale_);
            overHandle = std::abs(io.MousePos.x - corner.x) <= handle && std::abs(io.MousePos.y - corner.y) <= handle;
        }
        // The one on top under the pointer
        const Drawn* hovered = nullptr;
        for (const Drawn& each : drawn)
        {
            if (io.MousePos.x >= each.min.x && io.MousePos.x <= each.max.x && io.MousePos.y >= each.min.y &&
                io.MousePos.y <= each.max.y)
            {
                hovered = &each;
            }
        }

        const bool inside = ImGui::IsWindowHovered() &&
                            ImGui::IsMouseHoveringRect(position, ImVec2(position.x + size.x, position.y + size.y));
        if (inside && io.MouseClicked[ImGuiMouseButton_Left] && !io.KeyAlt && screenDragId_ == 0)
        {
            const std::uint32_t id = overHandle ? selected->id : hovered ? hovered->id : 0;
            if (id == 0)
            {
                scene_.Select(0); // empty screen: lets go of the selection (screen editing stays on)
                screenEditingFor_ = 0;
            }
            const GameObject* object = id != 0 ? scene_.Find(id) : nullptr;
            if (object && object->screen)
            {
                scene_.Select(id);
                screenDragId_ = id;
                screenDragResize_ = overHandle;
                screenDragStart_ = io.MousePos;
                screenDragFrom_ = *object->screen;
            }
        }
        if (screenDragId_ != 0)
        {
            GameObject* object = scene_.Find(screenDragId_);
            if (!io.MouseDown[ImGuiMouseButton_Left] || !object || !object->screen)
            {
                screenDragId_ = 0;
                return true;
            }
            // Snap (or Ctrl): moves in steps of 1 percent
            const bool snap = gizmoSnap_ || io.KeyCtrl;
            const auto snapped = [snap](float value) { return snap ? std::round(value) : value; };
            const float dx = (io.MousePos.x - screenDragStart_.x) / size.x * 100.0f;
            const float dy = (io.MousePos.y - screenDragStart_.y) / size.y * 100.0f;
            auto& element = *object->screen;
            if (screenDragResize_)
            {
                // Its corner follows the pointer: taller by twice the move (it grows both ways from its middle)
                element.height = std::max(0.5f, snapped(screenDragFrom_.height + dy * 2.0f));
                if (screenDragFrom_.width > 0.0f && screenDragFrom_.height > 0.0f)
                {
                    element.width = screenDragFrom_.width * element.height / screenDragFrom_.height;
                }
            }
            else
            {
                element.x = snapped(screenDragFrom_.x + dx);
                element.y = snapped(screenDragFrom_.y + dy);
            }
            return true;
        }
        return inside; // while editing the screen, the view's clicks are the screen's
    }

    // The Viewport window: toolbar on top, then either the 3D game view or the 2D front view
    void SceneEditor::DrawViewport()
    {
        DrawViewportToolbar();
        const ImVec2 viewPosition = ImGui::GetCursorScreenPos();
        const ImVec2 viewSize = ImGui::GetContentRegionAvail();
        if (viewport2D_ && !playing_)
        {
            viewportHovered_ = false;
            DrawViewport2D();
        }
        else
        {
            DrawViewport3D();
        }
        // The Simple interface's way to change the selected object (it has no Inspector)
        if (interface_ == InterfaceMode::Simple && !playing_)
        {
            DrawSimpleObjectBar(viewPosition, viewSize);
        }
    }

    // The row of buttons above the view (3D/2D, gizmo tools, snap, grid, sky, game camera, maximize)
    void SceneEditor::DrawViewportToolbar()
    {
        if (interface_ == InterfaceMode::Simple)
        {
            // The learner's tools: move, rotate and scale (clicking the one that is on turns it off; Q / W / E / R
            // still work), and how to look around
            const float size = 36.0f * uiScale_;
            const float top = ImGui::GetCursorPosY() + 8.0f * uiScale_;
            ImGui::SetCursorPos(ImVec2(8.0f * uiScale_, top));
            ImGui::BeginDisabled(playing_);
            for (const auto& [tool, icon, tooltip] : kGizmoTools)
            {
                if (Theme::IconButton(tooltip, icon, size, tooltip, gizmoTool_ == tool))
                {
                    gizmoTool_ = gizmoTool_ == tool ? GizmoTool::None : tool;
                }
                ImGui::SameLine(0.0f, 4.0f * uiScale_);
            }
            // Snap: the gizmo moves in steps of half a unit, turns by 15 degrees and resizes by 0.1
            ImGui::SameLine(0.0f, 8.0f * uiScale_);
            if (Theme::PillButton("Snap", gizmoSnap_ ? Theme::kSelect : Theme::kRaised,
                                  gizmoSnap_ ? Theme::kInk : Theme::kText, size))
            {
                gizmoSnap_ = !gizmoSnap_;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("Snap: move in half steps, turn by 15 degrees, resize by 0.1 (or hold Ctrl)");
            }
            // Screen: the game's 2D screen takes the clicks, to arrange its texts, pictures, bars and buttons
            ImGui::SameLine(0.0f, 4.0f * uiScale_);
            if (Theme::PillButton("Screen", screenEditing_ ? Theme::kSelect : Theme::kRaised,
                                  screenEditing_ ? Theme::kInk : Theme::kText, size))
            {
                screenEditing_ = !screenEditing_;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip(screenEditing_ ? "Editing the game's screen: click, drag and resize its texts, "
                                                   "pictures, bars and buttons. Click again to pick 3D objects."
                                                 : "Edit the game's screen: its texts, pictures, bars and buttons");
            }
            ImGui::SameLine(0.0f, 4.0f * uiScale_);
            ImGui::EndDisabled();
            if (Theme::PillButton("Logic", showLogicBadges_ ? Theme::kSelect : Theme::kRaised,
                                  showLogicBadges_ ? Theme::kInk : Theme::kText, size))
            {
                showLogicBadges_ = !showLogicBadges_;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("Show what each object's blocks do, over it. Click a badge to see its blocks.");
            }
            ImGui::SameLine(0.0f, 4.0f * uiScale_);

            // On the right: the view on its own, like Advanced's Maximize (Shift+Space)
            const char* fullLabel = viewportMaximized_ ? "Back to editor" : "Full view";
            const float fullWidth = Theme::PillWidth(fullLabel, size);
            const float fullX = ImGui::GetWindowContentRegionMax().x - fullWidth - 8.0f * uiScale_;
            const float toolsEnd = ImGui::GetCursorPosX();

            const char* hint = !playing_ ? "Right-drag to look around \xC2\xB7 scroll to zoom \xC2\xB7 F to focus"
                               : pointerLocked_ ? "Playing: the mouse turns you \xC2\xB7 Esc frees the mouse"
                               : runtime_ && runtime_->HasPlayer()
                                   ? "Click the game to play with the mouse \xC2\xB7 W A S D move"
                                   : "Running: seen through the Main camera";
            const float hintWidth = ImGui::CalcTextSize(hint).x;
            if (fullX - hintWidth - 12.0f * uiScale_ > toolsEnd + 8.0f * uiScale_)
            {
                ImGui::SetCursorPos(
                    ImVec2(fullX - hintWidth - 12.0f * uiScale_, top + (size - ImGui::GetTextLineHeight()) * 0.5f));
                ImGui::TextColored(Theme::kTextSoft, "%s", hint);
            }
            ImGui::SetCursorPos(ImVec2(std::max(fullX, toolsEnd + 8.0f * uiScale_), top));
            if (Theme::PillButton(fullLabel, viewportMaximized_ ? Theme::kSelect : Theme::kRaised,
                                  viewportMaximized_ ? Theme::kInk : Theme::kText, size))
            {
                viewportMaximized_ = !viewportMaximized_;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip(viewportMaximized_ ? "Show the other panels again (Shift+Space)"
                                                     : "Show only the view (Shift+Space)");
            }
            ImGui::SetCursorPos(ImVec2(0.0f, top + size + 8.0f * uiScale_));
            return;
        }

        const float toolTop = ImGui::GetCursorPosY() + 8.0f * uiScale_;
        ImGui::SetCursorPos(ImVec2(8.0f * uiScale_, toolTop));

        // 3D game view or the 2D front view (not while playing)
        ImGui::BeginDisabled(playing_);
        if (ToolButton("3D", !viewport2D_ || playing_, "The game in 3D"))
        {
            viewport2D_ = false;
        }
        ImGui::SameLine(0.0f, 2.0f * uiScale_);
        if (ToolButton("2D", viewport2D_ && !playing_,
                       "Front view: click to select, drag to move, middle / right drag to pan, wheel to zoom"))
        {
            viewport2D_ = true;
        }
        ImGui::EndDisabled();

        // Transform tools (edit mode, 3D)
        ImGui::SameLine(0.0f, 12.0f * uiScale_);
        ImGui::BeginDisabled(playing_ || viewport2D_);
        if (ToolButton("Select", gizmoTool_ == GizmoTool::None, "Select only, no gizmo (Q)"))
        {
            gizmoTool_ = GizmoTool::None;
        }
        for (const auto& [tool, icon, tooltip] : kGizmoTools)
        {
            ImGui::SameLine(0.0f, 4.0f * uiScale_);
            if (Theme::IconButton(tooltip, icon, kToolHeight * uiScale_, tooltip, gizmoTool_ == tool))
            {
                gizmoTool_ = tool;
            }
        }
        ImGui::SameLine(0.0f, 8.0f * uiScale_);
        if (ToolButton(gizmoLocal_ ? "Local" : "World", false,
                       "Gizmo axes: the object's own (Local) or the world's (World). Scale is always local."))
        {
            gizmoLocal_ = !gizmoLocal_;
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0.0f, 2.0f * uiScale_);
        ImGui::BeginDisabled(playing_);
        if (ToolButton("Snap", gizmoSnap_,
                       "Snap: 0.5 units, 15 degrees, 0.1 scale; in 2D, to the grid (or hold Ctrl while dragging)"))
        {
            gizmoSnap_ = !gizmoSnap_;
        }
        ImGui::SameLine(0.0f, 2.0f * uiScale_);
        if (ToolButton("Screen", screenEditing_,
                       "Edit the game's screen (its Screen Elements) instead of picking 3D objects in the view"))
        {
            screenEditing_ = !screenEditing_;
        }
        ImGui::EndDisabled();

        ImGui::SameLine(0.0f, 8.0f * uiScale_);
        if (ToolButton("Grid", showGrid_, "Show the ground grid"))
        {
            showGrid_ = !showGrid_;
        }
        ImGui::SameLine(0.0f, 2.0f * uiScale_);
        if (ToolButton("Wire", showWireframe_,
                       "Wireframe: draw objects as the edges of their triangles, to see how detailed a model is and "
                       "its level of detail switch"))
        {
            showWireframe_ = !showWireframe_;
        }
        ImGui::SameLine(0.0f, 2.0f * uiScale_);
        if (ToolButton("Sky", showSky_, "Sky, sunlight and distance haze (off: flat colours)"))
        {
            showSky_ = !showSky_;
        }
        ImGui::SameLine(0.0f, 2.0f * uiScale_);
        if (ToolButton("Stats", showStats_, "Frame rate and entity count in the corner of the view"))
        {
            showStats_ = !showStats_;
        }
        ImGui::SameLine(0.0f, 2.0f * uiScale_);
        if (ToolButton("Debug", debugLines_.Any() || traceBlocks_,
                       "Debug: tick to show bounding boxes, collision shapes and raycasts in the view, or log blocks "
                       "to the Console"))
        {
            ImGui::OpenPopup("##debugLines");
        }
        if (ImGui::BeginPopup("##debugLines"))
        {
            const auto hint = [](const char* text)
            {
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    ImGui::SetTooltip("%s", text);
                }
            };
            // Headings flush with the left edge, the line running on to the right
            ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextPadding,
                                ImVec2(0.0f, ImGui::GetStyle().SeparatorTextPadding.y));
            ImGui::SeparatorText("Show in viewport");
            const Engine::Systems::DebugLineSettings shownBefore = debugLines_;
            ImGui::Checkbox("Bounding boxes", &debugLines_.bounds);
            hint("A box around each object, showing the space it takes up; red while it touches something");
            ImGui::Checkbox("Collision shapes", &debugLines_.colliders);
            hint("Every rigid body's collider: green static, yellow dynamic, blue kinematic; faded when asleep, "
                 "brighter "
                 "while touching something");
            ImGui::Checkbox("Raycasts", &debugLines_.rays);
            hint("Each ray the game casts: green with a cross where it hit something, red when it hits "
                 "nothing. The Physics window's tests draw their own lines");
            if (runtime_)
            {
                // What the latest rays hit, newest first
                const auto& hitRays = runtime_->LastCastRays();
                int shown = 0;
                for (auto ray = hitRays.rbegin(); ray != hitRays.rend() && shown < 5; ++ray)
                {
                    const auto& cast = *ray;
                    if (cast.hit && !cast.hitName.empty())
                    {
                        const auto& hit = *cast.hit;
                        ImGui::TextDisabled("Hit %s at %.2f, %.2f, %.2f", cast.hitName.c_str(), hit.x, hit.y, hit.z);
                        ++shown;
                    }
                }
            }
            ImGui::Checkbox("Contacts", &debugLines_.contacts);
            hint("Where bodies start touching, with an arrow the way they push apart");
            ImGui::Checkbox("Forces", &debugLines_.forces);
            hint("An arrow for each push on a body: steady forces, and impulses for a moment");
            ImGui::Checkbox("Colour colliders by layer", &debugLines_.layerColors);
            hint("Collision shapes take the colour of their first layer instead of their body type");
            if (debugLines_ != shownBefore)
            {
                ImGui::MarkIniSettingsDirty();
            }
            ImGui::SeparatorText("Console");
            ImGui::Checkbox("Log each block as it runs", &traceBlocks_);
            hint("Prints every block to the Console as it runs, in order");
            ImGui::PopStyleVar();
            ImGui::EndPopup();
        }
        ImGui::SameLine(0.0f, 2.0f * uiScale_);
        if (ToolButton("Logic", showLogicBadges_,
                       "A badge over every object with blocks: what starts its scripts; while playing, which run now, "
                       "its variables and problems. Click one to see its blocks."))
        {
            showLogicBadges_ = !showLogicBadges_;
        }

        // Editor camera or game camera (Play mode always shows the game camera)
        ImGui::SameLine(0.0f, 8.0f * uiScale_);
        ImGui::BeginDisabled(playing_ || viewport2D_);
        if (ToolButton("Game Camera", UsesGameCamera(),
                       "Look through the game camera: the Main camera, behind the player when it follows - what Run "
                       "shows. Off: the editor camera."))
        {
            gameCameraView_ = !gameCameraView_;
        }
        ImGui::EndDisabled();

        // Right side: maximize
        const char* maximizeLabel = viewportMaximized_ ? "Restore" : "Maximize";
        const float maximizeX = ImGui::GetWindowContentRegionMax().x - ToolWidth(maximizeLabel) - 8.0f * uiScale_;
        ImGui::SameLine(0.0f, 8.0f * uiScale_);
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), maximizeX));
        if (ToolButton(maximizeLabel, viewportMaximized_, "Show only the Viewport (Shift+Space)"))
        {
            viewportMaximized_ = !viewportMaximized_;
        }
        ImGui::SetCursorPos(ImVec2(0.0f, toolTop + (kToolHeight + 8.0f) * uiScale_));
    }

    // Shows the game's framebuffer as an image, then draws the editor stuff (outline, gizmo, view cube, hints) on top
    void SceneEditor::DrawViewport3D()
    {
        const ImVec2 position = ImGui::GetCursorScreenPos();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 size(std::max(available.x, 1.0f), std::max(available.y, 1.0f));
        viewportX_ = position.x;
        viewportY_ = position.y;
        viewportWidth_ = size.x;
        viewportHeight_ = size.y;
        viewportVisible_ = true;

        // No ImGui item covers the view: ImGuizmo only starts a drag while no item is hovered or active, so the view
        // tracks the mouse itself. A button pressed in the view keeps the mouse for the camera until every button is
        // released, even if the pointer leaves the window mid-drag.
        const ImGuiIO& io = ImGui::GetIO();
        const bool inside = ImGui::IsWindowHovered() &&
                            ImGui::IsMouseHoveringRect(position, ImVec2(position.x + size.x, position.y + size.y));
        const bool pressed = io.MouseClicked[0] || io.MouseClicked[1] || io.MouseClicked[2];
        if (inside && pressed)
        {
            viewportMouseDrag_ = true;
            ImGui::SetWindowFocus(); // keys go to the camera / game, not a text field elsewhere
        }
        if (!io.MouseDown[0] && !io.MouseDown[1] && !io.MouseDown[2])
        {
            viewportMouseDrag_ = false;
        }
        viewportHovered_ = inside;
        viewportOwnsMouse_ = inside || viewportMouseDrag_;

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // (Re-)allocate the FBO to match this frame's viewport panel size HERE, during ImGui layout measurement, so
        // that the color-texture GL name we capture into AddImage below is guaranteed to be the one RenderGame draws
        // into later this frame.
        const int fboW = static_cast<int>(std::lround(size.x));
        const int fboH = static_cast<int>(std::lround(size.y));

        // Also force a Resize retry if the FBO is not valid even when cached dimensions match.
        if (viewportFbo_.Width() != fboW || viewportFbo_.Height() != fboH || !viewportFbo_.IsValid())
        {
            viewportFbo_.Resize(fboW, fboH);
            LOG_INFO(Editor, "Viewport picture made at {} x {}", fboW, fboH);
        }

        // UVs are flipped vertically because OpenGL stores textures with Y=0 at the bottom, while ImGui samples with
        // Y=0 at the top.
        if (viewportFbo_.IsValid())
        {
            const ImTextureID texID = static_cast<ImTextureID>(viewportFbo_.GetColorTextureID());
            // Draw the FBO image without alpha blending over the ImGui window background: the FBO is fully opaque
            // (clear colour has alpha=1, sky shaders and meshes output alpha=1)
            drawList->AddCallback(ImDrawCallback_DisableBlend, nullptr);
            drawList->AddImage(texID, position, ImVec2(position.x + size.x, position.y + size.y), ImVec2(0.0f, 1.0f),
                               ImVec2(1.0f, 0.0f));
            drawList->AddCallback(ImDrawCallback_RestoreBlend, nullptr);
        }

        // Gizmos and picking work in the editor camera's lens; the game camera may have another
        auto* camera = runtime_ ? runtime_->Camera() : nullptr;
        if (camera && !UsesGameCamera())
        {
            // The camera moved this frame already (UpdateViewportCamera); its lens is built for this frame's size
            // here, the same way the renderer builds it, so the gizmos sit exactly on the objects at any size
            using Engine::Renderer::Renderer3D;
            const glm::mat4& view = camera->GetViewMatrix();
            const glm::mat4 projection =
                glm::perspective(glm::radians(Renderer3D::kFieldOfViewDegrees), size.x / size.y, Renderer3D::kNearPlane,
                                 Renderer3D::kFarPlane);
            const glm::mat4 viewProjection = projection * view;

            DrawSelectionOutline(viewProjection, position, size);
            DrawPhysicsTests(viewProjection, position, size);
            bool overTool = DrawTransformGizmo(view, projection, position, size);
            overTool |= DrawLogicBadges(viewProjection, position, size);
            overTool |= EditScreenElements(position, size);

            // The orientation gizmo, kept inside the view's top-right corner at any size (a view smaller than
            // ~130 px, e.g. the frame a docked Viewport first appears, still gets the 12 px minimum)
            const float half = std::clamp(22.0f * uiScale_, 12.0f, std::max(12.0f, std::min(size.x, size.y) * 0.09f));
            const float inset = half * 2.7f;
            overTool |= DrawViewCube(view, ImVec2(position.x + size.x - inset, position.y + inset), half);

            // Click an object to select it (empty space deselects); not when using a gizmo or orbiting (Alt)
            if (inside && io.MouseClicked[0] && !overTool && !io.KeyAlt)
            {
                // A click on part of something (a prefab copy, or a group such as the player and its body parts)
                // selects the whole of it first, and the part itself on a second click
                const std::uint32_t hit = PickObject(viewProjection, position, size, io.MousePos);
                const std::uint32_t root = WholeOf(scene_.Description(), hit);
                const bool inSelected = scene_.Selected() == root ||
                                        Engine::Scene::IsAncestorOf(scene_.Description(), scene_.Selected(), hit) ||
                                        scene_.Selected() == hit;
                scene_.Select(root != 0 && !inSelected ? root : hit);
            }

            // Simple: a right-click on an object (not a drag to look around) opens its menu
            const float clickDistance = 4.0f * uiScale_;
            if (interface_ == InterfaceMode::Simple && !playing_ && inside && !overTool &&
                ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
                io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < clickDistance * clickDistance)
            {
                if (const std::uint32_t hit = PickObject(viewProjection, position, size, io.MousePos); hit != 0)
                {
                    const std::uint32_t root = WholeOf(scene_.Description(), hit);
                    scene_.Select(root != 0 ? root : hit);
                    ImGui::OpenPopup("##ViewObjectMenu");
                }
            }
            std::function<void()> deferred;
            if (ImGui::BeginPopup("##ViewObjectMenu"))
            {
                if (GameObject* object = scene_.Find(scene_.Selected()))
                {
                    DrawSimpleObjectMenu(*object, deferred);
                }
                else
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            if (deferred)
            {
                deferred();
            }

            // A prefab dragged from the Hierarchy's Prefabs list and dropped here is placed where it lands on the
            // ground. The view has no ImGui item of its own (see above), so the target is its rectangle.
            if (ImGui::BeginDragDropTargetCustom(ImRect(position, ImVec2(position.x + size.x, position.y + size.y)),
                                                 ImGui::GetID("##ViewportDrop")))
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPrefabPayload))
                {
                    const std::string path(static_cast<const char*>(payload->Data));
                    PlacePrefab(path, GroundPointOf(RayThrough(viewProjection, position, size, io.MousePos)));
                }
                ImGui::EndDragDropTarget();
            }

            // An asset dragged from the Assets window and dropped on an object is used on it
            if (const auto dropped = Panels::AcceptAnyAssetDropInRect(
                    position, ImVec2(position.x + size.x, position.y + size.y), "##viewportAssetDrop"))
            {
                if (GameObject* target = scene_.Find(PickObject(viewProjection, position, size, io.MousePos)))
                {
                    UseAsset(*target, *dropped);
                }
                else
                {
                    SetStatus("Drop the asset on an object to use it");
                }
            }
        }
        else if (camera)
        {
            // Through the game camera: the lens it was drawn with this frame
            DrawPhysicsTests(camera->GetViewProjectionMatrix(), position, size);
            DrawLogicBadges(camera->GetViewProjectionMatrix(), position, size);
        }

        // While the game runs the view has a lime frame (yellow when paused): what moves now is the game, not the editor
        if (playing_)
        {
            drawList->AddRect(position, ImVec2(position.x + size.x, position.y + size.y),
                              ImGui::GetColorU32(paused_ ? Theme::kEvents : Theme::kRun), 0.0f, ImDrawFlags_None,
                              3.0f * uiScale_);
        }

        // The game's HUD over it while it plays: health, the variables the blocks show, speech bubbles, a crosshair,
        // and "click to play" until the game has the mouse
        if (playing_ && runtime_)
        {
            screenTargets_ = Runtime::DrawGameHud(drawList,
                                                  {.min = position,
                                                   .size = size,
                                                   .scale = uiScale_,
                                                   .pointerLocked = pointerLocked_ || paused_,
                                                   .mouse = io.MousePos},
                                                  *runtime_);
        }

        // Hints along the bottom. Simple shows only "no game": its toolbar has the learner's short hint.
        const char* hint = nullptr;
        if (projectDir_.empty())
        {
            hint = "Open a game, or make one on the Home page";
        }
        else if (interface_ == InterfaceMode::Simple || playing_)
        {
            hint = nullptr; // when playing, the game's own HUD says how to play
        }
        else if (gameCameraView_)
        {
            hint = "Game camera view (what Run shows) - turn off Game Camera to move the editor camera";
        }
        else
        {
            hint = "Right mouse + WASD fly   Alt + left orbit   Middle pan   Wheel zoom   F frame   W E R gizmos";
        }
        if (hint != nullptr)
        {
            DrawHint(drawList, position, size, hint, uiScale_);
        }
        const float margin = 8.0f * uiScale_;
        const float fpsLineHeight = ImGui::GetFontSize() + 8.0f * uiScale_;

        // Frame rate and entity count are for Advanced (Window > Show Stats); learners don't need them
        if (showStats_ && interface_ == InterfaceMode::Advanced)
        {
            DrawStats(drawList, position, deltaSeconds_, runtime_ ? runtime_->EntityCount() : 0, uiScale_,
                      runtime_ ? LodText(runtime_->LodStatistics()) : std::string{});
        }

        if (!runtime_ && !projectDir_.empty())
        {
            drawList->AddText(ImVec2(position.x + margin, position.y + margin + fpsLineHeight),
                              IM_COL32(230, 120, 110, 255), "The game could not be shown.");
        }
        if (paused_)
        {
            drawList->AddText(ImVec2(position.x + margin, position.y + margin + fpsLineHeight),
                              IM_COL32(240, 190, 60, 255), "PAUSED");
        }
        // Prefab Mode: which prefab the view shows, centred at the top, in the prefab blue of the Hierarchy
        if (prefabMode_)
        {
            const std::string label = "Prefab Mode: " + prefabMode_->name + (HasUnsavedChanges() ? " *" : "");
            const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
            const float pad = 6.0f * uiScale_;
            const ImVec2 at(position.x + (size.x - labelSize.x) * 0.5f, position.y + margin + pad);
            drawList->AddRectFilled(ImVec2(at.x - pad, at.y - pad * 0.5f),
                                    ImVec2(at.x + labelSize.x + pad, at.y + labelSize.y + pad * 0.5f),
                                    IM_COL32(15, 18, 24, 220), 4.0f * uiScale_);
            drawList->AddText(at, IM_COL32(115, 191, 255, 255), label.c_str());
        }

        // Claim the view's area in the layout. A Dummy has no id, so it is never "hovered" and does not stop the
        // gizmos from starting a drag
        ImGui::SetCursorScreenPos(position);
        ImGui::Dummy(size);
    }

    // The front (X / Y) view. The grid, the axis lines, objects, picking, dragging and snapping all go through
    // view2D_'s world <-> screen conversion, so they stay lined up while zooming, panning and resizing.
    void SceneEditor::DrawViewport2D()
    {
        const ImVec2 canvasPosition = ImGui::GetCursorScreenPos();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 canvasSize(std::max(available.x, 1.0f), std::max(available.y, 1.0f));
        const ImVec2 canvasEnd(canvasPosition.x + canvasSize.x, canvasPosition.y + canvasSize.y);
        view2D_.SetRect({canvasPosition.x, canvasPosition.y}, {canvasSize.x, canvasSize.y});

        // ---- Mouse first, so this frame is drawn with this frame's view: wheel zooms at the pointer, middle / right
        // drag pans, left click selects and left drag moves (Snap or Ctrl: to the grid)
        ImGui::SetCursorScreenPos(canvasPosition);
        ImGui::InvisibleButton("##Canvas", canvasSize,
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                                   ImGuiButtonFlags_MouseButtonMiddle);
        const ImGuiIO& io = ImGui::GetIO();
        const glm::vec2 mouse(io.MousePos.x, io.MousePos.y);
        if (ImGui::IsItemHovered() && io.MouseWheel != 0.0f)
        {
            view2D_.ZoomAt(mouse, std::pow(kZoomPerWheelStep2D, io.MouseWheel));
        }
        if (ImGui::IsItemActive() &&
            (ImGui::IsMouseDown(ImGuiMouseButton_Middle) || ImGui::IsMouseDown(ImGuiMouseButton_Right)))
        {
            view2D_.Pan({io.MouseDelta.x, io.MouseDelta.y});
        }
        const float gridStep = view2D_.GridStep(kMinGridSpacing2D * uiScale_);
        const float minHalfSize = kMinObjectHalfSize2D * uiScale_;

        if (ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            std::uint32_t hit = 0;
            for (const GameObject& object : scene_.Objects())
            {
                const auto world = Engine::Scene::WorldTransformOf(scene_.Description(), object.id);
                if (!IsDrawn(object) || !world)
                {
                    continue;
                }
                const Rect2D rect = Rect2DOf(object, *world, view2D_, minHalfSize);
                if (std::abs(io.MousePos.x - rect.center.x) <= rect.half.x &&
                    std::abs(io.MousePos.y - rect.center.y) <= rect.half.y)
                {
                    hit = object.id; // the last one drawn is on top
                }
            }
            scene_.Select(hit);
            dragObject_ = hit;
            if (const auto world = Engine::Scene::WorldTransformOf(scene_.Description(), hit))
            {
                dragOffset_ = glm::vec2(world->x, world->y) - view2D_.ScreenToWorld(mouse);
            }
        }
        if (ImGui::IsItemActive() && dragObject_ != 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            GameObject* dragged = scene_.Find(dragObject_);
            if (dragged && dragged->transform)
            {
                glm::vec2 position = view2D_.ScreenToWorld(mouse) + dragOffset_;
                if (gizmoSnap_ || io.KeyCtrl)
                {
                    position = Camera::View2D::Snap(position, gridStep);
                }
                // position is in the world; a child's Transform is measured from its parent
                const auto world = Engine::Scene::WorldTransformOf(scene_.Description(), dragObject_);
                const float worldZ = world ? world->z : 0.0f;
                const glm::vec3 local(glm::inverse(Engine::Scene::ParentMatrixOf(scene_.Description(), dragObject_)) *
                                      glm::vec4(position, worldZ, 1.0f));
                dragged->transform->x = local.x;
                dragged->transform->y = local.y;
                dragged->transform->z = local.z;
            }
        }
        if (!ImGui::IsItemActive())
        {
            dragObject_ = 0;
        }

        // ---- Draw
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(canvasPosition, canvasEnd, true);
        drawList->AddRectFilled(canvasPosition, canvasEnd, IM_COL32(58, 62, 68, 255));

        // Grid lines at whole multiples of the step (every tenth brighter), so one always passes through the origin
        if (showGrid_)
        {
            const glm::vec2 visibleMin = view2D_.VisibleMin();
            const glm::vec2 visibleMax = view2D_.VisibleMax();
            const auto lineColor = [](long long index)
            { return index % 10 == 0 ? IM_COL32(96, 101, 110, 255) : IM_COL32(78, 82, 90, 255); };
            for (auto i = static_cast<long long>(std::floor(visibleMin.x / gridStep));
                 i <= static_cast<long long>(std::ceil(visibleMax.x / gridStep)); ++i)
            {
                const float x = view2D_.WorldToScreen({static_cast<float>(i) * gridStep, 0.0f}).x;
                drawList->AddLine(ImVec2(x, canvasPosition.y), ImVec2(x, canvasEnd.y), lineColor(i));
            }
            for (auto i = static_cast<long long>(std::floor(visibleMin.y / gridStep));
                 i <= static_cast<long long>(std::ceil(visibleMax.y / gridStep)); ++i)
            {
                const float y = view2D_.WorldToScreen({0.0f, static_cast<float>(i) * gridStep}).y;
                drawList->AddLine(ImVec2(canvasPosition.x, y), ImVec2(canvasEnd.x, y), lineColor(i));
            }
        }

        for (const GameObject& object : scene_.Objects())
        {
            const auto world = Engine::Scene::WorldTransformOf(scene_.Description(), object.id);
            if (!object.transform || !object.renderable || !world) // not drawn (see IsDrawn)
            {
                continue;
            }
            const Rect2D rect = Rect2DOf(object, *world, view2D_, minHalfSize);
            const ImVec2& center = rect.center;
            const ImVec2& half = rect.half;
            const bool selected = scene_.Selected() == object.id;
            const ImU32 color = object.player ? IM_COL32(90, 200, 120, 255) : IM_COL32(255, 160, 40, 255);
            const ImU32 border = IM_COL32(0, 229, 255, 255);
            const Shape shape = object.renderable->shape;
            if (shape == Shape::Pyramid)
            {
                const ImVec2 top(center.x, center.y - half.y);
                const ImVec2 left(center.x - half.x, center.y + half.y);
                const ImVec2 right(center.x + half.x, center.y + half.y);
                drawList->AddTriangleFilled(top, left, right, color);
                if (selected)
                {
                    drawList->AddTriangle(top, left, right, border, 3.0f);
                }
            }
            else if (shape == Shape::Sphere || shape == Shape::Circle)
            {
                drawList->AddEllipseFilled(center, half, color);
                if (selected)
                {
                    drawList->AddEllipse(center, half, border, 0.0f, 0, 3.0f);
                }
            }
            else
            {
                const ImVec2 minPoint(center.x - half.x, center.y - half.y);
                const ImVec2 maxPoint(center.x + half.x, center.y + half.y);
                drawList->AddRectFilled(minPoint, maxPoint, color);
                if (selected)
                {
                    drawList->AddRect(minPoint, maxPoint, border, 0.0f, 0, 3.0f);
                }
            }
            drawList->AddText(ImVec2(center.x + half.x + 4.0f * uiScale_, center.y - ImGui::GetFontSize() * 0.5f),
                              IM_COL32(225, 225, 225, 255), object.name.c_str());
        }

        // The world's X (red) and Y (green) axes, through the origin
        const glm::vec2 origin = view2D_.WorldToScreen({0.0f, 0.0f});
        drawList->AddLine(ImVec2(canvasPosition.x, origin.y), ImVec2(canvasEnd.x, origin.y), IM_COL32(200, 70, 70, 200),
                          1.5f);
        drawList->AddLine(ImVec2(origin.x, canvasPosition.y), ImVec2(origin.x, canvasEnd.y), IM_COL32(90, 200, 90, 200),
                          1.5f);

        const std::string hint = std::format(
            "Left drag move   Middle / right drag pan   Wheel zoom   Ctrl snap   Grid {:g} units", gridStep);
        DrawHint(drawList, canvasPosition, canvasSize, hint.c_str(), uiScale_);

        if (showStats_)
        {
            DrawStats(drawList, canvasPosition, deltaSeconds_, runtime_ ? runtime_->EntityCount() : 0, uiScale_);
        }

        drawList->PopClipRect();
    }

    // Move/rotate/scale handles on the selected object (ImGuizmo). Returns true while the mouse is on it,
    // so clicking the gizmo doesn't also select whatever is behind it
    bool SceneEditor::DrawTransformGizmo(const glm::mat4& view, const glm::mat4& projection, const ImVec2& position,
                                         const ImVec2& size)
    {
        GameObject* object = scene_.Find(scene_.Selected());
        if (!object || !object->transform || gizmoTool_ == GizmoTool::None || playing_)
        {
            return false;
        }

        // Lines scale with the display, like the rest of the UI
        ImGuizmo::Style& style = ImGuizmo::GetStyle();
        style.TranslationLineThickness = 3.0f * uiScale_;
        style.TranslationLineArrowSize = 6.0f * uiScale_;
        style.RotationLineThickness = 2.0f * uiScale_;
        style.RotationOuterLineThickness = 3.0f * uiScale_;
        style.ScaleLineThickness = 3.0f * uiScale_;
        style.ScaleLineCircleSize = 6.0f * uiScale_;
        style.CenterCircleSize = 6.0f * uiScale_;

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(position.x, position.y, size.x, size.y);

        const ImGuizmo::OPERATION operation = gizmoTool_ == GizmoTool::Move     ? ImGuizmo::TRANSLATE
                                              : gizmoTool_ == GizmoTool::Rotate ? ImGuizmo::ROTATE
                                                                                : ImGuizmo::SCALE;
        const ImGuizmo::MODE mode = gizmoLocal_ || gizmoTool_ == GizmoTool::Scale ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        const float snapStep = gizmoTool_ == GizmoTool::Move ? 0.5f : gizmoTool_ == GizmoTool::Rotate ? 15.0f : 0.1f;
        const float snap[3] = {snapStep, snapStep, snapStep};
        const bool snapping = gizmoSnap_ || ImGui::GetIO().KeyCtrl;

        // The gizmo works in the world; a child's Transform is relative to its parent, so the result is taken back
        // into the parent's space before it is written
        TransformComponent& transform = *object->transform;
        const TransformComponent before = transform;
        const glm::mat4 parent = Engine::Scene::ParentMatrixOf(scene_.Description(), object->id);
        glm::mat4 model = parent * ModelOf(transform);
        if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), operation, mode,
                                 glm::value_ptr(model), nullptr, snapping ? snap : nullptr))
        {
            // Only the part being edited is written back, so the others do not drift
            float translation[3];
            float rotation[3];
            float scale[3];
            const glm::mat4 local = glm::inverse(parent) * model;
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(local), translation, rotation, scale);
            switch (gizmoTool_)
            {
            case GizmoTool::Move:
                transform.x = translation[0];
                transform.y = translation[1];
                transform.z = translation[2];
                break;
            case GizmoTool::Rotate:
                transform.rotationX = rotation[0];
                transform.rotation = rotation[1];
                transform.rotationZ = rotation[2];
                break;
            case GizmoTool::Scale:
            {
                const float uniformScale = std::max(transform.scale, 1e-4f);
                transform.scaleX = scale[0] / uniformScale;
                transform.scaleY = scale[1] / uniformScale;
                transform.scaleZ = scale[2] / uniformScale;
                break;
            }
            case GizmoTool::None: break;
            }
        }
        // A drag started: remember where the object was so the whole drag is one undo step
        if (ImGuizmo::IsUsing() && gizmoObject_ == 0)
        {
            gizmoObject_ = object->id;
            gizmoBefore_ = before;
        }
        return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
    }

    // The little labelled cube in the top right: click a face to look from that side, drag to orbit
    bool SceneEditor::DrawViewCube(const glm::mat4& view, const ImVec2& center, float half)
    {
        // A view: which way the camera looks from (yaw, pitch as EditorCamera::SetView takes them)
        struct Side
        {
            const char* name;
            float yaw;
            float pitch;
        };
        static constexpr Side kFront{"Front", 0.0f, 0.0f};
        static constexpr Side kBack{"Back", 180.0f, 0.0f};
        static constexpr Side kRight{"Right", -90.0f, 0.0f};
        static constexpr Side kLeft{"Left", 90.0f, 0.0f};
        static constexpr Side kTop{"Top", 0.0f, -90.0f};
        static constexpr Side kBottom{"Bottom", 0.0f, 90.0f};

        struct Face
        {
            glm::vec3 normal;
            glm::vec3 u; // the face's right, seen from outside
            glm::vec3 v; // the face's up
            const Side* side;
        };
        static const std::array<Face, 6> kFaces{{
            {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}, &kFront},
            {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, &kBack},
            {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}, &kRight},
            {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}, &kLeft},
            {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}, &kTop},
            {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}, &kBottom},
        }};

        // Axis handles: +X / +Y / +Z solid with a letter, -X / -Y / -Z as rings
        struct Handle
        {
            glm::vec3 direction;
            ImU32 color;
            const char* letter;
            bool positive;
            const Side* side;
        };
        static const std::array<Handle, 6> kHandles{{
            {{1, 0, 0}, IM_COL32(226, 76, 76, 255), "X", true, &kRight},
            {{-1, 0, 0}, IM_COL32(226, 76, 76, 255), "-X", false, &kLeft},
            {{0, 1, 0}, IM_COL32(110, 200, 80, 255), "Y", true, &kTop},
            {{0, -1, 0}, IM_COL32(110, 200, 80, 255), "-Y", false, &kBottom},
            {{0, 0, 1}, IM_COL32(72, 130, 235, 255), "Z", true, &kFront},
            {{0, 0, -1}, IM_COL32(72, 130, 235, 255), "-Z", false, &kBack},
        }};
        constexpr float kHandleDistance = 1.9f; // from the centre, in half-cube units

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImGuiIO& io = ImGui::GetIO();
        const glm::mat3 rotation(view); // world to view: the cube turns with the camera
        const auto project = [&](const glm::vec3& point)
        {
            const glm::vec3 p = rotation * point;
            return ImVec2(center.x + p.x * half, center.y - p.y * half);
        };
        const auto depthOf = [&](const glm::vec3& point) { return (rotation * point).z; }; // + towards the viewer

        std::array<std::array<ImVec2, 4>, 6> quads{};
        for (std::size_t i = 0; i < kFaces.size(); ++i)
        {
            const Face& f = kFaces[i];
            quads[i] = {project(f.normal + f.u + f.v), project(f.normal - f.u + f.v), project(f.normal - f.u - f.v),
                        project(f.normal + f.u - f.v)};
        }
        const float handleRadius = half * 0.36f;
        const float areaRadius = half * (kHandleDistance + 0.4f);
        const float dx = io.MousePos.x - center.x;
        const float dy = io.MousePos.y - center.y;
        const bool windowHovered = ImGui::IsWindowHovered();
        const bool overArea = windowHovered && dx * dx + dy * dy <= areaRadius * areaRadius;

        // What the mouse is on, front to back: front handles, then faces, then handles behind the cube
        const Side* hovered = nullptr;
        int hoveredFace = -1;
        int hoveredHandle = -1;
        if (overArea && !viewCubeDrag_)
        {
            const auto onHandle = [&](std::size_t i)
            {
                const ImVec2 at = project(kHandles[i].direction * kHandleDistance);
                const float hx = io.MousePos.x - at.x;
                const float hy = io.MousePos.y - at.y;
                return hx * hx + hy * hy <= handleRadius * handleRadius;
            };
            float best = -1e9f;
            for (std::size_t i = 0; i < kHandles.size(); ++i)
            {
                const float depth = depthOf(kHandles[i].direction);
                if (depth > 0.0f && depth > best && onHandle(i))
                {
                    best = depth;
                    hoveredHandle = static_cast<int>(i);
                }
            }
            if (hoveredHandle < 0)
            {
                for (std::size_t i = 0; i < kFaces.size(); ++i)
                {
                    const float depth = depthOf(kFaces[i].normal);
                    if (depth > 0.02f && depth > best && PointInQuad(quads[i], io.MousePos))
                    {
                        best = depth;
                        hoveredFace = static_cast<int>(i);
                    }
                }
            }
            if (hoveredHandle < 0 && hoveredFace < 0)
            {
                for (std::size_t i = 0; i < kHandles.size(); ++i)
                {
                    if (depthOf(kHandles[i].direction) <= 0.0f && onHandle(i))
                    {
                        hoveredHandle = static_cast<int>(i);
                    }
                }
            }
            hovered = hoveredHandle >= 0 ? kHandles[hoveredHandle].side
                      : hoveredFace >= 0 ? kFaces[hoveredFace].side
                                         : nullptr;
        }

        // ---- Draw: backdrop, handles behind, the cube, handles in front
        drawList->AddCircleFilled(center, areaRadius, IM_COL32(16, 18, 22, overArea || viewCubeDrag_ ? 150 : 90));

        const float fontSize = ImGui::GetFontSize() * 0.8f;
        const auto drawHandle = [&](std::size_t i)
        {
            const Handle& h = kHandles[i];
            const ImVec2 at = project(h.direction * kHandleDistance);
            const bool isHovered = static_cast<int>(i) == hoveredHandle;
            const float radius = handleRadius * (isHovered ? 1.15f : 1.0f);
            if (h.positive)
            {
                drawList->AddLine(center, at, h.color, 2.0f * uiScale_);
                drawList->AddCircleFilled(at, radius, h.color);
                if (isHovered)
                {
                    drawList->AddCircle(at, radius, IM_COL32(255, 255, 255, 255), 0, 2.0f * uiScale_);
                }
                const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, h.letter);
                drawList->AddText(ImGui::GetFont(), fontSize,
                                  ImVec2(at.x - textSize.x * 0.5f, at.y - textSize.y * 0.5f), IM_COL32(18, 18, 20, 255),
                                  h.letter);
            }
            else
            {
                const ImU32 fill = isHovered ? h.color : IM_COL32(30, 33, 38, 220);
                drawList->AddCircleFilled(at, radius * 0.85f, fill);
                drawList->AddCircle(at, radius * 0.85f, h.color, 0, 2.0f * uiScale_);
            }
        };
        for (std::size_t i = 0; i < kHandles.size(); ++i)
        {
            if (depthOf(kHandles[i].direction) <= 0.0f)
            {
                drawHandle(i);
            }
        }

        std::array<int, 6> order{};
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(),
                  [&](int a, int b) { return depthOf(kFaces[a].normal) < depthOf(kFaces[b].normal); });
        for (const int i : order)
        {
            const float facing = depthOf(kFaces[i].normal);
            if (facing <= 0.02f)
            {
                continue;
            }
            // Shaded by how squarely the face looks at the camera, so the cube reads as a solid
            const float light = 0.30f + 0.32f * facing;
            const ImU32 fill = i == hoveredFace
                                   ? IM_COL32(58, 110, 186, 245)
                                   : ImGui::GetColorU32(ImVec4(light, light + 0.02f, light + 0.05f, 0.95f));
            const auto& q = quads[i];
            drawList->AddQuadFilled(q[0], q[1], q[2], q[3], fill);
            drawList->AddQuad(q[0], q[1], q[2], q[3], IM_COL32(14, 15, 18, 255), 1.2f * uiScale_);
            if (facing > 0.3f)
            {
                const char* label = kFaces[i].side->name;
                const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
                const ImVec2 at = project(kFaces[i].normal);
                const auto alpha = static_cast<int>(std::clamp((facing - 0.3f) / 0.4f, 0.0f, 1.0f) * 255.0f);
                drawList->AddText(ImGui::GetFont(), fontSize,
                                  ImVec2(at.x - textSize.x * 0.5f, at.y - textSize.y * 0.5f),
                                  IM_COL32(245, 247, 250, alpha), label);
            }
        }

        for (std::size_t i = 0; i < kHandles.size(); ++i)
        {
            if (depthOf(kHandles[i].direction) > 0.0f)
            {
                drawHandle(i);
            }
        }

        // ---- Mouse: click a face / handle to snap there, drag anywhere on the gizmo to orbit
        if (overArea && io.MouseClicked[0])
        {
            viewCubeDrag_ = true;
            viewCubeMoved_ = false;
        }
        if (viewCubeDrag_)
        {
            if (io.MouseDown[0])
            {
                const ImVec2 dragged = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
                if (!viewCubeMoved_ && dragged.x * dragged.x + dragged.y * dragged.y > 9.0f * uiScale_ * uiScale_)
                {
                    viewCubeMoved_ = true;
                }
                if (viewCubeMoved_)
                {
                    editorCamera_.Orbit(io.MouseDelta.x, io.MouseDelta.y);
                }
            }
            else
            {
                if (!viewCubeMoved_ && hovered)
                {
                    editorCamera_.SetView(hovered->yaw, hovered->pitch);
                }
                viewCubeDrag_ = false;
            }
        }
        if (hovered && !viewCubeDrag_)
        {
            ImGui::SetTooltip("%s view (click)   drag: orbit", hovered->name);
        }
        else if (hovered && !viewCubeMoved_)
        {
            ImGui::SetTooltip("%s view", hovered->name);
        }

        // Home: back to the default three-quarter view
        const float homeWidth = ImGui::CalcTextSize("Home").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(center.x - homeWidth * 0.5f, center.y + areaRadius + 4.0f * uiScale_));
        if (ImGui::SmallButton("Home"))
        {
            editorCamera_.SetView(35.0f, -25.0f);
        }
        const bool overHome = ImGui::IsItemHovered() || ImGui::IsItemActive();
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Default three-quarter view");
        }
        ImGui::SetCursorScreenPos(cursor);

        return overArea || viewCubeDrag_ || overHome;
    }

    SceneEditor::LocalBox SceneEditor::BoxOf(const RenderableDesc& renderable) const
    {
        const std::string& file = !renderable.model.empty() ? renderable.model : renderable.mesh;
        if (!file.empty())
        {
            if (const auto& bounds = ModelFactsOf(file).bounds)
            {
                const Engine::Assets::BoundingBox turned = Engine::Assets::Turned(*bounds, renderable.modelTurn);
                return {turned.Center(), turned.Half()};
            }
        }
        // A built-in shape at its own size and centre inside the object
        if (!renderable.mesh.empty())
        {
            return {glm::vec3(0.0f), HalfExtentsOf(renderable)};
        }
        const glm::vec3 shapeSize(renderable.shapeSizeX, renderable.shapeSizeY, renderable.shapeSizeZ);
        return {glm::vec3(renderable.shapeCenterX, renderable.shapeCenterY, renderable.shapeCenterZ),
                HalfExtentsOf(renderable) * shapeSize};
    }

    // Draws the selected object's box as 12 lines in screen space (easier than a real stencil outline)
    void SceneEditor::DrawSelectionOutline(const glm::mat4& viewProjection, const ImVec2& position, const ImVec2& size)
    {
        const GameObject* object = scene_.Find(scene_.Selected());
        if (!object || !object->transform || !object->renderable) // nothing selected, or not drawn (see IsDrawn)
        {
            return;
        }

        // The object's box, drawn over the scene
        const glm::mat4 model = Engine::Scene::WorldMatrixOf(scene_.Description(), object->id);
        const LocalBox box = BoxOf(*object->renderable);
        const glm::vec3 e = box.half;
        std::array<ImVec2, 8> corners{};
        std::array<bool, 8> visible{};
        for (int i = 0; i < 8; ++i)
        {
            const glm::vec3 local =
                box.center + glm::vec3((i & 1) ? e.x : -e.x, (i & 2) ? e.y : -e.y, (i & 4) ? e.z : -e.z);
            visible[i] =
                ToScreen(viewProjection, glm::vec3(model * glm::vec4(local, 1.0f)), position, size, corners[i]);
        }
        // Clipped to the view, so a box partly off screen does not draw over the toolbar
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(position, ImVec2(position.x + size.x, position.y + size.y), true);
        for (int a = 0; a < 8; ++a)
        {
            for (const int bit : {1, 2, 4})
            {
                const int b = a | bit;
                if (b != a && visible[a] && visible[b])
                {
                    drawList->AddLine(corners[a], corners[b], kSelectionColor, 1.5f * uiScale_);
                }
            }
        }
        drawList->PopClipRect();
    }

    // Click-to-select: shoots a ray through the mouse and returns the id of the closest object it hits (0 = none)
    std::uint32_t SceneEditor::PickObject(const glm::mat4& viewProjection, const ImVec2& position, const ImVec2& size,
                                          const ImVec2& mouse) const
    {
        // A ray from the camera through the mouse
        const auto [origin, direction] = RayThrough(viewProjection, position, size, mouse);

        // The nearest object whose box the ray goes through (the box test runs in the object's own space, so rotation
        // and scale are taken into account)
        std::uint32_t best = 0;
        float bestDistance = std::numeric_limits<float>::max();
        for (const GameObject& object : scene_.Objects())
        {
            if (!object.transform || !object.renderable) // not drawn (see IsDrawn)
            {
                continue;
            }
            const glm::mat4 model = Engine::Scene::WorldMatrixOf(scene_.Description(), object.id);
            if (std::abs(glm::determinant(model)) < 1e-9f)
            {
                continue;
            }
            const glm::mat4 toLocal = glm::inverse(model);
            const glm::vec3 localOrigin(toLocal * glm::vec4(origin, 1.0f));
            const glm::vec3 localDirection(toLocal * glm::vec4(direction, 0.0f));
            // Its box, at least 5 cm thick in the world whatever its scale (a minimum in its own units would grow
            // with it: a 60 m ground would be a 6 m block that hides everything standing on it). Flat shapes are
            // flat here, so the ground is only the ground.
            const LocalBox box = BoxOf(*object.renderable);
            glm::vec3 extents = box.half;
            if (object.renderable->mesh.empty() && object.renderable->model.empty())
            {
                if (object.renderable->shape == Shape::Plane)
                {
                    extents.y = 0.0f;
                }
                else if (object.renderable->shape == Shape::Quad || object.renderable->shape == Shape::Circle)
                {
                    extents.z = 0.0f;
                }
            }
            const glm::vec3 axisScale(glm::length(glm::vec3(model[0])), glm::length(glm::vec3(model[1])),
                                      glm::length(glm::vec3(model[2])));
            extents = glm::max(extents, glm::vec3(0.05f) / glm::max(axisScale, glm::vec3(1e-6f)));
            const glm::vec3 fromBox = localOrigin - box.center; // the ray's start from the box's centre
            const auto& renderable = *object.renderable;

            float tMin = 0.0f;
            float tMax = std::numeric_limits<float>::max();
            bool hit = true;
            for (int axis = 0; axis < 3 && hit; ++axis)
            {
                if (std::abs(localDirection[axis]) < 1e-8f)
                {
                    hit = std::abs(fromBox[axis]) <= extents[axis];
                    continue;
                }
                float t0 = (-extents[axis] - fromBox[axis]) / localDirection[axis];
                float t1 = (extents[axis] - fromBox[axis]) / localDirection[axis];
                if (t0 > t1)
                {
                    std::swap(t0, t1);
                }
                tMin = std::max(tMin, t0);
                tMax = std::min(tMax, t1);
                hit = tMin <= tMax;
            }
            if (!hit)
            {
                continue;
            }
            // A model or mesh file is hit only where it has a surface, not anywhere in its box: a big ground's box
            // holds everything standing on it (and the camera), and would be clicked in place of all of them
            float along = tMin;
            const std::string& file = !renderable.model.empty() ? renderable.model : renderable.mesh;
            if (!file.empty())
            {
                const std::vector<glm::vec3>& triangles = ModelFactsOf(file).triangles;
                if (!triangles.empty())
                {
                    const std::optional<float> surface =
                        NearestTriangle(triangles, localOrigin, localDirection, renderable.modelTurn);
                    if (!surface)
                    {
                        continue;
                    }
                    along = *surface;
                }
            }
            // Compare in world units
            const glm::vec3 worldHit(model * glm::vec4(localOrigin + localDirection * along, 1.0f));
            const float distance = glm::length(worldHit - origin);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = object.id;
            }
        }
        return best;
    }

    // F key / double-click in Hierarchy: move the editor camera so the selected object fills the view
    void SceneEditor::FocusSelected()
    {
        const GameObject* object = scene_.Find(scene_.Selected());
        const auto world = object ? Engine::Scene::WorldTransformOf(scene_.Description(), object->id) : std::nullopt;
        if (!world)
        {
            return; // no Transform: it has no place in the world to frame
        }
        const TransformComponent& t = *world;
        const LocalBox box = object->renderable ? BoxOf(*object->renderable) : LocalBox{};
        const glm::vec3 extents = box.half * t.scale * glm::abs(glm::vec3(t.scaleX, t.scaleY, t.scaleZ));
        // On the middle of what it draws (a figure's chest, not its feet)
        const glm::mat4 model = Engine::Scene::WorldMatrixOf(scene_.Description(), object->id);
        const glm::vec3 middle(model * glm::vec4(box.center, 1.0f));
        view_ = EditorView::Workspace;
        editorCamera_.Focus(middle, std::max(glm::length(extents), 0.25f));
        view2D_.SetCenter({t.x, t.y});
    }
}
