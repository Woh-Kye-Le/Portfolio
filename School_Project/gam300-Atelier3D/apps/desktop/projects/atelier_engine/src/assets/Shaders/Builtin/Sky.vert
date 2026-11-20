/*
 * Builtin/Sky.vert
 * Fullscreen triangle: passes through the clip-space positions, so each fragment can
 * turn its screen coordinate back into a world-space view direction for sky shading.
 */

#version 330 core

layout(location = 0) in vec3 a_Position;

out vec2 v_Ndc;

void main()
{
    v_Ndc = a_Position.xy;
    gl_Position = vec4(a_Position.xy, 1.0, 1.0);
}
