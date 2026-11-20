/*
 * Builtin/Standard2D.vert - the simple 2D pipeline used by Renderer2D: the model and
 * view-projection matrices map the unit quad / circle straight into clip space, and the
 * mesh's vertex colour and UV are passed through to the fragment stage.
 */

#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;

uniform mat4 u_Model;
uniform mat4 u_ViewProjection;

out vec2 v_TexCoord;
out vec4 v_Color;

void main()
{
    vec4 world = u_Model * vec4(a_Position, 1.0);
    v_TexCoord = a_TexCoord;
    v_Color    = a_Color;
    gl_Position = u_ViewProjection * world;
}
