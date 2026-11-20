/*
 * Builtin/FallbackUnlit.frag
 * Straight 2D texture sample - the safety net shader drawn when an asset shader
 * fails to load. Guaranteed to compile and produce something visible so you can
 * still see placeholder geometry while authoring real materials.
 */

#version 330 core

in vec2 v_TexCoord;

uniform sampler2D u_Texture;

out vec4 o_Color;

void main()
{
    o_Color = texture(u_Texture, v_TexCoord);
}
