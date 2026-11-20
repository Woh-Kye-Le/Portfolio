/*
 * Builtin/Standard3D.vert - FUTURE (Assets/Shaders/Builtin/Standard3D.vert)
 * Long-term: Blinn-Phong / simplified-PBR lit shader for the educational block
 * editor's default 3D materials. Includes positions, normals, TBN for normal mapping.
 */

#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Color;
layout(location = 4) in vec4 a_Joints;  // skinning: the four bones a vertex follows...
layout(location = 5) in vec4 a_Weights; // ...and how much each moves it

uniform mat4 u_Model;
uniform mat4 u_ViewProjection;
uniform mat3 u_NormalMatrix;

// A skinned mesh is drawn with u_Skinned 1 and its bones' matrices (the model's space, at rest, to where the bone is
// now); every other mesh with u_Skinned 0
const int kMaxBones = 128; // Engine::Assets::kMaxBones
uniform int u_Skinned;
uniform mat4 u_Bones[kMaxBones];

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoord;
out vec4 v_Color;

void main()
{
    vec4 position = vec4(a_Position, 1.0);
    vec3 normal = a_Normal;
    float total = a_Weights.x + a_Weights.y + a_Weights.z + a_Weights.w;
    if (u_Skinned == 1 && total > 0.0)
    {
        mat4 skin = a_Weights.x * u_Bones[min(int(a_Joints.x), kMaxBones - 1)] + a_Weights.y * u_Bones[min(int(a_Joints.y), kMaxBones - 1)] +
                    a_Weights.z * u_Bones[min(int(a_Joints.z), kMaxBones - 1)] + a_Weights.w * u_Bones[min(int(a_Joints.w), kMaxBones - 1)];
        position = skin * position;
        normal = mat3(skin) * normal;
    }

    vec4 world = u_Model * position;
    v_WorldPos = world.xyz;
    v_Normal   = normalize(u_NormalMatrix * normal);
    v_TexCoord = a_TexCoord;
    v_Color    = a_Color;
    gl_Position = u_ViewProjection * world;
}
