/* !
@file    	    RenderAPI.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    Provides the RenderAPI static class plus supporting enumerations for
    primitive type, cull face, and front-face winding. RenderAPI abstracts
    common low-level OpenGL operations such as GLEW init, viewport, clear,
    fixed-function state, and indexed/array draw calls.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Standard Library Headers
#include <cstdint>

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          Enumerates supported OpenGL primitive assembly modes for draw calls.
    *************************************************************************/
    enum class PrimitiveType : std::uint8_t
    {
        Triangles,
        Lines,
        Points,
    };

    /*!***********************************************************************
    @brief          Enumerates which polygon faces are discarded when face culling is enabled.
    *************************************************************************/
    enum class CullFace : std::uint8_t
    {
        Back,
        Front,
        FrontAndBack,
    };

    /*!***********************************************************************
    @brief          Enumerates the screen-space winding order that defines a front-facing polygon.
    *************************************************************************/
    enum class FrontFace : std::uint8_t
    {
        CCW,
        CW,
    };

    /*!***********************************************************************
    @brief          Static-only wrapper around common OpenGL rendering operations and fixed-function state.
    *************************************************************************/
    class RenderAPI
    {
    public:
        /*!***********************************************************************
        @brief          Deleted default constructor (all static API).
        *************************************************************************/
        RenderAPI() = delete;
        /*!***********************************************************************
        @brief          Deleted copy constructor (all static API).
        *************************************************************************/
        RenderAPI(const RenderAPI&) = delete;
        /*!***********************************************************************
        @brief          Deleted copy assignment operator (all static API).
        @return         Reference to this RenderAPI.
        *************************************************************************/
        RenderAPI& operator=(const RenderAPI&) = delete;

        /*!***********************************************************************
        @brief          Initializes GLEW so that OpenGL function pointers are safe to call.
        *************************************************************************/
        static void Init();
        /*!***********************************************************************
        @brief          Resets the internal initialized flag; does not tear down GL context.
        *************************************************************************/
        static void Shutdown();

        /*!***********************************************************************
        @brief          Reports whether GLEW completed successfully and GL function pointers are valid.
        @return         True if RenderAPI::Init succeeded and a GL context is current.
        *************************************************************************/
        static bool IsReady();

        /*!***********************************************************************
        @brief          Sets the GL viewport rectangle in pixels.
        @param          x             Leftmost x coordinate of the viewport.
        @param          y             Bottommost y coordinate of the viewport.
        @param          width         Viewport width in pixels.
        @param          height        Viewport height in pixels.
        *************************************************************************/
        static void SetViewport(int x, int y, int width, int height);
        /*!***********************************************************************
        @brief          Sets the RGBA value used to clear the color buffer.
        @param          r             Red channel in [0,1].
        @param          g             Green channel in [0,1].
        @param          b             Blue channel in [0,1].
        @param          a             Alpha channel in [0,1].
        *************************************************************************/
        static void SetClearColor(float r, float g, float b, float a);
        /*!***********************************************************************
        @brief          Clears both the color and depth buffers in one call.
        *************************************************************************/
        static void Clear();
        /*!***********************************************************************
        @brief          Clears the depth buffer only: what is drawn next goes over everything drawn so far.
        *************************************************************************/
        static void ClearDepth();

        /*!***********************************************************************
        @brief          Enables or disables the OpenGL depth test.
        @param          enabled       True to enable depth testing; false to disable.
        *************************************************************************/
        static void EnableDepthTest(bool enabled);
        /*!***********************************************************************
        @brief          Enables or disables alpha blending (SRC_ALPHA, ONE_MINUS_SRC_ALPHA).
        @param          enabled       True to enable blending; false to disable.
        *************************************************************************/
        static void EnableBlend(bool enabled);
        /*!***********************************************************************
        @brief          Enables or disables face culling and configures which face is discarded.
        @param          enabled       True to enable face culling; false to disable.
        @param          face          Which face to discard when enabled (Back/Front/FrontAndBack).
        @param          winding       Vertex winding order that identifies a "front" face (CCW or CW).
        *************************************************************************/
        static void EnableFaceCulling(bool enabled, CullFace face = CullFace::Back, FrontFace winding = FrontFace::CCW);

        /*!***********************************************************************
        @brief          Sets which winding is a front face, leaving culling on or off as it is (a mirrored object,
                        with a negative scale, turns its faces round).
        @param          winding       Vertex winding order that identifies a "front" face.
        *************************************************************************/
        static void SetFrontFace(FrontFace winding);

        /*!***********************************************************************
        @brief          Draws triangles as their edges only (wireframe), or filled again.
        @param          enabled       True for edges only; false for filled triangles (the default).
        *************************************************************************/
        static void SetWireframe(bool enabled);

        /*!***********************************************************************
        @brief          Issues an indexed draw call using the currently bound VAO and element buffer.
        @param          indexCount    Number of indices to draw from the index buffer.
        @param          type          Primitive assembly type (triangles, lines, or points).
        *************************************************************************/
        static void DrawIndexed(uint32_t indexCount, PrimitiveType type = PrimitiveType::Triangles);
        /*!***********************************************************************
        @brief          Issues an indexed draw call starting partway into the index buffer,
                        so one uploaded mesh can be drawn one submesh (material run) at a time.
        @param          firstIndex    Index the draw starts at.
        @param          indexCount    Number of indices to draw from firstIndex.
        @param          type          Primitive assembly type (triangles, lines, or points).
        *************************************************************************/
        static void DrawIndexedRange(uint32_t firstIndex, uint32_t indexCount,
                                     PrimitiveType type = PrimitiveType::Triangles);
        /*!***********************************************************************
        @brief          Issues a non-indexed (arrays) draw call using the currently bound VAO.
        @param          vertexCount   Number of vertices to draw from the vertex buffer.
        @param          type          Primitive assembly type (triangles, lines, or points).
        *************************************************************************/
        static void DrawArrays(uint32_t vertexCount, PrimitiveType type = PrimitiveType::Triangles);
    };
}
