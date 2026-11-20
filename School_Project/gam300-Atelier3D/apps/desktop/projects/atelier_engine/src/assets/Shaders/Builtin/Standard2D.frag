/*
 * Builtin/Standard2D.frag - flat 2D colour: the mesh's vertex colour times u_AlbedoTint, so
 * a shape keeps its own tint and Renderer2D stays free of any lighting or environment maths.
 */

#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;

uniform sampler2D u_Albedo;
uniform vec4      u_AlbedoTint = vec4(1.0);

out vec4 o_Color;

void main()
{
    o_Color = v_Color * u_AlbedoTint;
}
