/*
 * Builtin/Standard3D.frag
 * Vertex colour * u_AlbedoTint, lit by the default environment (Renderer::Environment): a sun, soft light from the
 * sky above and the ground below, a small highlight, and haze that fades distant surfaces into the horizon.
 * u_Lit = 0 (lines, the grid) skips the lighting but keeps the haze. u_Environment = 0 gives the plain flat colour.
 */

#version 330 core

#include "Common/BlinnPhong.glsl"

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;
in vec4 v_Color;

uniform sampler2D u_Texture;
uniform int       u_Textured = 0;
uniform vec4      u_AlbedoTint = vec4(1.0);
uniform vec3      u_CameraPos;
uniform vec3      u_EmissiveTint = vec3(0.0);    // glow (a material's, or a model's), added after the lighting
uniform float     u_EmissiveStrength = 0.0;

uniform int   u_Environment = 0;
uniform int   u_Lit = 1;
uniform vec3  u_SunDirection = vec3(0.45, 0.8, 0.35);
uniform vec3  u_SunColor = vec3(0.85);
uniform vec3  u_SkyColor = vec3(0.42, 0.56, 0.77);
uniform vec3  u_GroundColor = vec3(0.22, 0.23, 0.25);
uniform float u_Ambient = 0.55;
uniform vec3  u_FogColor = vec3(0.64, 0.74, 0.86);
uniform float u_FogDensity = 0.012;

uniform int  u_PointLightCount = 0;
uniform vec3 u_PointLightPos   [BP_kMaxPointLights];
uniform vec3 u_PointLightColor [BP_kMaxPointLights];
uniform float u_PointLightRange[BP_kMaxPointLights];

out vec4 o_Color;

void main()
{
    vec4 texSample = u_Textured > 0 ? texture(u_Texture, v_TexCoord) : vec4(1.0);
    // Cut-out pictures (leaves, grass, flowers on flat cards): where the picture is see-through, draw nothing, so the
    // card's shape is the picture's shape. Only the picture's own alpha cuts; a see-through tint still blends.
    if (texSample.a < 0.5)
    {
        discard;
    }
    vec4 albedo = v_Color * texSample * u_AlbedoTint;
    if (u_Environment == 0)
    {
        o_Color = albedo;
        return;
    }

    vec3 color = albedo.rgb;
    float distanceFromCamera = length(u_CameraPos - v_WorldPos);
    if (u_Lit == 1)
    {
        vec3 n = normalize(v_Normal);
        if (!gl_FrontFacing)
        {
            n = -n; // flat shapes are lit from both sides
        }
        vec3 v = normalize(u_CameraPos - v_WorldPos);

        // Hemispheric ambient + per-light additive Blinn-Phong accumulator.
        vec3 ambient = BP_HemisphericAmbient(n, u_SkyColor, u_GroundColor, u_Ambient);
        vec3 diffuseSum  = vec3(0.0);
        vec3 specularSum = vec3(0.0);

        BP_AccumDirectional(diffuseSum, specularSum, n, v, u_SunDirection, u_SunColor);

        int pointCount = clamp(u_PointLightCount, 0, BP_kMaxPointLights);
        for (int i = 0; i < BP_kMaxPointLights; ++i)
        {
            if (i >= pointCount) break;
            BP_AccumPoint(diffuseSum, specularSum, n, v, v_WorldPos,
                          u_PointLightPos[i], u_PointLightColor[i], u_PointLightRange[i]);
        }

        color = albedo.rgb * (ambient + diffuseSum) + specularSum;
    }

    color += u_EmissiveTint * u_EmissiveStrength;

    // Haze: thicker with distance, so depth reads at a glance
    float haze = BP_FogFactorExp2(distanceFromCamera, u_FogDensity);
    o_Color = vec4(mix(color, u_FogColor, clamp(haze, 0.0, 1.0)), albedo.a);
}
