/* BlinnPhong.glsl - shared surface-lighting helpers (pure, no uniforms).
 * Intended to be #include'd from Builtin/*.frag shaders after the shader's
 * uniform + in/out declarations. Everything here is self-contained GLSL
 * functions so Standard3D.frag, Sky.frag's sun disc math, and any future
 * lit variants can share identical Blinn-Phong formulae.
 */

#ifndef BP_CONSTANTS_GLSL
#define BP_CONSTANTS_GLSL

const int   BP_kMaxPointLights = 8;
const float BP_kShininess      = 48.0;
const float BP_kSpecularStrength = 0.18;

// Attenuates a light against its world-space range: full intensity up to
// 25% of range, then smooth falloff to exactly zero at the range boundary.
float BP_SmoothAttenuation(float worldDistance, float range)
{
    float safeRange = max(range, 1e-4f);
    return 1.0 - smoothstep(0.25 * safeRange, safeRange, worldDistance);
}

// Hemispheric ambient: sky colour above the surface, ground colour below,
// blended by the Y component of the (already-normalized) surface normal.
vec3 BP_HemisphericAmbient(vec3 n, vec3 skyColor, vec3 groundColor, float ambientStrength)
{
    float t = n.y * 0.5 + 0.5;
    return mix(groundColor, skyColor, t) * ambientStrength;
}

// Exp-squared distance fog: thicker at a distance, clamped to [0,1].
// density should be a small positive like 0.012 (tuned for world units ~1m).
float BP_FogFactorExp2(float worldDistanceFromCamera, float density)
{
    float x = worldDistanceFromCamera * density;
    return 1.0 - exp(-(x * x));
}

// Standard (directional or omni) Blinn-Phong surface response. Returns the
// additive {diffuse, specular} contributions before albedo or attenuation.
// All input vectors are assumed normalized and in the same coordinate space
// (typically world space). specGate is usually step(0.0, ndotl) to kill spec
// when the light is behind the surface.
struct BP_BlinnPhongOut
{
    vec3 diffuse;
    vec3 specular;
};

BP_BlinnPhongOut BP_BlinnPhong(vec3 n, vec3 l, vec3 v, vec3 lightColor,
                               float shininess, float specularStrength)
{
    vec3  h     = normalize(l + v);
    float ndotl = max(dot(n, l), 0.0);
    float ndoth = max(dot(n, h), 0.0);
    float mask  = step(0.0, ndotl);
    float spec  = pow(ndoth, shininess) * specularStrength * mask;

    BP_BlinnPhongOut o;
    o.diffuse  = lightColor * ndotl;
    o.specular = lightColor * spec;
    return o;
}

// Convenience: accumulate one directional light (sun) into existing sums.
void BP_AccumDirectional(inout vec3 diffuseSum, inout vec3 specularSum,
                         vec3 n, vec3 v, vec3 dirTowardsLight, vec3 lightColor)
{
    BP_BlinnPhongOut bp = BP_BlinnPhong(n, normalize(dirTowardsLight), v, lightColor,
                                        BP_kShininess, BP_kSpecularStrength);
    diffuseSum  += bp.diffuse;
    specularSum += bp.specular;
}

// Convenience: accumulate one point light with range attenuation into sums.
void BP_AccumPoint(inout vec3 diffuseSum, inout vec3 specularSum,
                   vec3 n, vec3 v, vec3 worldPos,
                   vec3 lightPos, vec3 lightColor, float range)
{
    vec3  toLight = lightPos - worldPos;
    float dist    = length(toLight);
    float atten   = BP_SmoothAttenuation(dist, range);
    if (atten <= 0.0) return;

    vec3 l = toLight / max(dist, 1e-5f);
    BP_BlinnPhongOut bp = BP_BlinnPhong(n, l, v, lightColor,
                                        BP_kShininess, BP_kSpecularStrength);
    diffuseSum  += bp.diffuse  * atten;
    specularSum += bp.specular * atten;
}

#endif // BP_CONSTANTS_GLSL
