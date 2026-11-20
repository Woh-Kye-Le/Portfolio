/* !
@file    	    SavedGlState.hpp
@author  	    j.cai@digipen.edu [100%]
@date    		02/11/2026
@brief
    Declares SavedGlState: the OpenGL state an editor render into a
    framebuffer of its own changes (the Assets previews, the block previews),
    saved when made and put back when it goes, so the editor's own drawing is
    unaffected.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <GL/glew.h>

namespace Editor
{
    class SavedGlState
    {
    public:
        SavedGlState()
        {
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer_);
            glGetIntegerv(GL_VIEWPORT, viewport_);
            glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor_);
            glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb_);
            glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb_);
            glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha_);
            glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha_);
            depthTest_ = glIsEnabled(GL_DEPTH_TEST);
            cullFace_ = glIsEnabled(GL_CULL_FACE);
            blend_ = glIsEnabled(GL_BLEND);
            scissor_ = glIsEnabled(GL_SCISSOR_TEST);
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask_);
        }
        ~SavedGlState()
        {
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer_));
            glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
            glClearColor(clearColor_[0], clearColor_[1], clearColor_[2], clearColor_[3]);
            glBlendFuncSeparate(static_cast<GLenum>(blendSrcRgb_), static_cast<GLenum>(blendDstRgb_),
                                static_cast<GLenum>(blendSrcAlpha_), static_cast<GLenum>(blendDstAlpha_));
            Set(GL_DEPTH_TEST, depthTest_);
            Set(GL_CULL_FACE, cullFace_);
            Set(GL_BLEND, blend_);
            Set(GL_SCISSOR_TEST, scissor_);
            glDepthMask(depthMask_);
        }
        SavedGlState(const SavedGlState&) = delete;
        SavedGlState& operator=(const SavedGlState&) = delete;

    private:
        static void Set(GLenum capability, GLboolean enabled)
        {
            enabled ? glEnable(capability) : glDisable(capability);
        }

        GLint framebuffer_ = 0;
        GLint viewport_[4]{};
        GLfloat clearColor_[4]{};
        GLint blendSrcRgb_ = GL_ONE;
        GLint blendDstRgb_ = GL_ZERO;
        GLint blendSrcAlpha_ = GL_ONE;
        GLint blendDstAlpha_ = GL_ZERO;
        GLboolean depthTest_ = GL_FALSE;
        GLboolean cullFace_ = GL_FALSE;
        GLboolean blend_ = GL_FALSE;
        GLboolean scissor_ = GL_FALSE;
        GLboolean depthMask_ = GL_TRUE;
    };
}
