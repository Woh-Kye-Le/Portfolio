/* !
@file    	    RenderAPI.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Implements the RenderAPI static class by translating engine-level enums to
    OpenGL constants and forwarding calls to the corresponding GLEW-wrapped GL
    functions for init, viewport, clear, fixed-function state, and draws.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Graphics Headers
#include "engine/graphics/Renderer/RenderAPI.hpp"

#include "engine/core/Log.hpp"

// Third-Party Headers
#include <GL/glew.h>

// Standard Library Headers

namespace Engine::Graphics
{
    namespace
    {
        /*!***********************************************************************
        @brief          Maps a PrimitiveType enum value to the corresponding GLenum primitive constant.
        @param          type          Engine primitive type (Triangles/Lines/Points).
        @return         OpenGL GLenum for the given primitive type; defaults to GL_TRIANGLES.
        *************************************************************************/
        GLenum ToGL(PrimitiveType type)
        {
            switch (type)
            {
            case PrimitiveType::Triangles: return GL_TRIANGLES;
            case PrimitiveType::Lines: return GL_LINES;
            case PrimitiveType::Points: return GL_POINTS;
            }
            return GL_TRIANGLES;
        }

        /*!***********************************************************************
        @brief          Maps a CullFace enum value to the corresponding GLenum cull-face constant.
        @param          face          Engine cull-face selection (Back/Front/FrontAndBack).
        @return         OpenGL GLenum for the given cull face; defaults to GL_BACK.
        *************************************************************************/
        GLenum ToGL(CullFace face)
        {
            switch (face)
            {
            case CullFace::Back: return GL_BACK;
            case CullFace::Front: return GL_FRONT;
            case CullFace::FrontAndBack: return GL_FRONT_AND_BACK;
            }
            return GL_BACK;
        }

        /*!***********************************************************************
        @brief          Maps a FrontFace enum value to the corresponding GLenum winding constant.
        @param          winding       Engine front-face winding (CCW/CW).
        @return         OpenGL GLenum for the given winding; defaults to GL_CCW.
        *************************************************************************/
        GLenum ToGL(FrontFace winding)
        {
            switch (winding)
            {
            case FrontFace::CCW: return GL_CCW;
            case FrontFace::CW: return GL_CW;
            }
            return GL_CCW;
        }

        // Per-process flag: true once GLEW has initialized successfully
        bool s_initialized = false;
    }

    /*!***********************************************************************
    @brief          Initializes GLEW with experimental mode enabled, logging failure on error.
    *************************************************************************/
    void RenderAPI::Init()
    {
        if (s_initialized)
        {
            return;
        }

        glewExperimental = GL_TRUE;
        const GLenum err = glewInit();
        if (err != GLEW_OK)
        {
            LOG_ERROR(Render, "glewInit failed: {}",
                      glewGetErrorString(err) ? reinterpret_cast<const char*>(glewGetErrorString(err))
                                              : "unknown error");
            return;
        }
        s_initialized = true;
    }

    /*!***********************************************************************
    @brief          Resets the internal initialized flag (GL context remains owned elsewhere).
    *************************************************************************/
    void RenderAPI::Shutdown()
    {
        s_initialized = false;
    }

    /*!***********************************************************************
    @brief          Returns whether GLEW initialized successfully and GL function pointers are non-null.
    @return         True if the API is ready for use; false otherwise.
    *************************************************************************/
    bool RenderAPI::IsReady()
    {
        return s_initialized && glCreateShader != nullptr;
    }

    /*!***********************************************************************
    @brief          Sets the OpenGL viewport rectangle in pixel coordinates.
    @param          x             Leftmost x coordinate of the viewport.
    @param          y             Bottommost y coordinate of the viewport.
    @param          width         Width of the viewport in pixels.
    @param          height        Height of the viewport in pixels.
    *************************************************************************/
    void RenderAPI::SetViewport(int x, int y, int width, int height)
    {
        glViewport(x, y, width, height);
    }

    /*!***********************************************************************
    @brief          Sets the RGBA clear color for glClear calls.
    @param          r             Red channel in [0,1].
    @param          g             Green channel in [0,1].
    @param          b             Blue channel in [0,1].
    @param          a             Alpha channel in [0,1].
    *************************************************************************/
    void RenderAPI::SetClearColor(float r, float g, float b, float a)
    {
        glClearColor(r, g, b, a);
    }

    /*!***********************************************************************
    @brief          Clears both the color buffer and the depth buffer simultaneously.
    *************************************************************************/
    void RenderAPI::Clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void RenderAPI::ClearDepth()
    {
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    /*!***********************************************************************
    @brief          Enables or disables the GL depth test for per-pixel depth comparisons.
    @param          enabled       True to enable depth testing; false to disable.
    *************************************************************************/
    void RenderAPI::EnableDepthTest(bool enabled)
    {
        enabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
    }

    /*!***********************************************************************
    @brief          Enables or disables alpha blending with SRC_ALPHA / ONE_MINUS_SRC_ALPHA factors.
    @param          enabled       True to enable blending; false to disable.
    *************************************************************************/
    void RenderAPI::EnableBlend(bool enabled)
    {
        if (enabled)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        else
        {
            glDisable(GL_BLEND);
        }
    }

    /*!***********************************************************************
    @brief          Enables or disables face culling and sets the culled face and winding order.
    @param          enabled       True to enable face culling; false to disable.
    @param          face          Which polygon face to discard when culling is enabled.
    @param          winding       Winding order that defines a front-facing polygon.
    *************************************************************************/
    void RenderAPI::EnableFaceCulling(bool enabled, CullFace face, FrontFace winding)
    {
        if (enabled)
        {
            glEnable(GL_CULL_FACE);
            glCullFace(ToGL(face));
            glFrontFace(ToGL(winding));
        }
        else
        {
            glDisable(GL_CULL_FACE);
        }
    }

    /*!***********************************************************************
    @brief          Sets which winding is a front face without changing whether culling is on.
    @param          winding       Winding order that defines a front-facing polygon.
    *************************************************************************/
    void RenderAPI::SetFrontFace(FrontFace winding)
    {
        glFrontFace(ToGL(winding));
    }

    /*!***********************************************************************
    @brief          Switches the polygon mode of both faces between lines and fill.
    @param          enabled       True for GL_LINE, false for GL_FILL.
    *************************************************************************/
    void RenderAPI::SetWireframe(bool enabled)
    {
        glPolygonMode(GL_FRONT_AND_BACK, enabled ? GL_LINE : GL_FILL);
    }

    /*!***********************************************************************
    @brief          Issues an indexed draw call with unsigned-int indices from the currently bound EBO.
    @param          indexCount    Number of indices to consume from the element buffer.
    @param          type          Primitive assembly type used to interpret the indices.
    *************************************************************************/
    void RenderAPI::DrawIndexed(uint32_t indexCount, PrimitiveType type)
    {
        glDrawElements(ToGL(type), static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, nullptr);
    }

    /*!***********************************************************************
    @brief          Issues an indexed draw call beginning at firstIndex.
    @param          firstIndex    Index inside the bound element buffer the draw starts at.
    @param          indexCount    Number of indices to consume from that offset.
    @param          type          Primitive assembly type used to interpret the indices.
    *************************************************************************/
    void RenderAPI::DrawIndexedRange(uint32_t firstIndex, uint32_t indexCount, PrimitiveType type)
    {
        glDrawElements(ToGL(type), static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT,
                       reinterpret_cast<const void*>(static_cast<std::uintptr_t>(firstIndex) * sizeof(uint32_t)));
    }

    /*!***********************************************************************
    @brief          Issues a non-indexed draw call using sequential vertices from the currently bound VBO.
    @param          vertexCount   Number of vertices to draw starting from index 0.
    @param          type          Primitive assembly type used to interpret the vertices.
    *************************************************************************/
    void RenderAPI::DrawArrays(uint32_t vertexCount, PrimitiveType type)
    {
        glDrawArrays(ToGL(type), 0, static_cast<GLsizei>(vertexCount));
    }
}
