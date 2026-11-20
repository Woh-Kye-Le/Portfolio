/*
 * Builtin/Unlit.frag - FUTURE (Assets/Shaders/Builtin/Unlit.frag)
 * Minimal unlit fragment shader. Long term:
 *   - Sample u_Texture; default to 1.0 if not bound.
 *   - Vertex-color blending with texture.
 */

#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;

uniform sampler2D u_Texture;
uniform vec4      u_Tint = vec4(1.0);

out vec4 o_Color;

void main()
{
    o_Color = v_Color * u_Tint;
}
