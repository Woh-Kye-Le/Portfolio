/*
 * Builtin/FallbackUnlit.vert
 * Simple unlit textured vertex shader used as the built-in fallback when a custom
 * asset shader is missing or fails to compile. Standard attribute layout matching
 * the rest of the engine (a_Position at 0, a_TexCoord at 2).
 */

#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_Model;
uniform mat4 u_ViewProjection;

out vec2 v_TexCoord;

void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
}
