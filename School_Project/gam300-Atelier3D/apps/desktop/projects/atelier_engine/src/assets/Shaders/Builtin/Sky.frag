/*
 * Builtin/Sky.frag
 * Procedural sky: horizon-to-zenith gradient above the horizon, ground below, plus the sun's
 * disc and a soft halo. The environment's u_Zenith/u_Horizon/u_Ground blend smoothly so editors
 * can tune the mood without reauthoring a cubemap. With a sky picture (a panorama: all the way
 * round across its width, straight up to straight down over its height) that is drawn instead.
 */

#version 330 core

in vec2 v_Ndc;

uniform mat4 u_InverseViewProjection;
uniform vec3 u_CameraPos;
uniform vec3 u_Zenith;
uniform vec3 u_Horizon;
uniform vec3 u_Ground;
uniform vec3 u_SunDirection;
uniform vec3 u_SunColor;
uniform int u_HasPicture;
uniform float u_PictureTurn; // radians
uniform sampler2D u_Picture;

out vec4 o_Color;

const float kPi = 3.14159265;

void main()
{
    vec4 world = u_InverseViewProjection * vec4(v_Ndc, 1.0, 1.0);
    vec3 direction = normalize(world.xyz / world.w - u_CameraPos);
    float up = direction.y;
    if (u_HasPicture != 0)
    {
        // Round the horizon across the picture, from the top (straight up) to the bottom (straight down). The
        // largest picture level is read so the seam where the picture wraps round has no line.
        float around = atan(direction.x, -direction.z) + u_PictureTurn;
        vec2 uv = vec2(fract(around / (2.0 * kPi) + 0.5), acos(clamp(up, -1.0, 1.0)) / kPi);
        o_Color = vec4(textureLod(u_Picture, vec2(uv.x, 1.0 - uv.y), 0.0).rgb, 1.0);
        return;
    }
    vec3 color = up >= 0.0 ? mix(u_Horizon, u_Zenith, pow(up, 0.55))
                           : mix(u_Horizon, u_Ground, pow(min(-up * 3.0, 1.0), 0.6));
    float sun = max(dot(direction, normalize(u_SunDirection)), 0.0);
    color += u_SunColor * (pow(sun, 900.0) * 3.0 + pow(sun, 24.0) * 0.12) * step(0.0, up);
    o_Color = vec4(color, 1.0);
}
