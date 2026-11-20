/* !
@file    	    Material.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    This file contains the implementation of the Material class, providing the
    named constructor that initializes a Material instance with a user-specified
    identifier, and the Bind() method which uploads all PBR scalar uniforms
    (albedo tint, metallic, roughness, emissive) to the active shader and binds
    the albedo/normal/metallic-roughness textures on texture units 0/1/2
    respectively.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Graphics Headers
#include "engine/graphics/Material/Material.hpp"

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          Constructs a named Material instance with default PBR
                    parameters: albedo tint white (1,1,1,1), metallic 0,
                    roughness 0.5, emissive disabled (black tint, strength 0),
                    and no shader/textures bound.
    @param          name          Display/identifier name for this material.
    *************************************************************************/
    Material::Material(std::string name) : name_(std::move(name)) {}

    /*!***********************************************************************
    @brief          Binds every material-owned resource the currently-active
                    shader expects to consume:
                      - PBR scalar uniforms (u_AlbedoTint, u_Metallic,
                        u_Roughness, u_EmissiveStrength, u_EmissiveTint)
                        uploaded to the shader returned by GetShader().
                      - u_Texture sampler set to unit 0 and albedoTex_
                        bound on GL_TEXTURE0 (when present).
                      - u_NormalMap sampler set to unit 1 and normalTex_
                        bound on GL_TEXTURE1 (when present).
                      - u_MetallicRoughnessMap sampler set to unit 2 and
                        metallicRoughnessTex_ bound on GL_TEXTURE2 (when
                        present).
                    Each upload is skipped if the corresponding shader
                    pointer or texture pointer is null, so Bind() is safe
                    to call on partially-populated Materials.
    *************************************************************************/
    void Material::Bind() const
    {
        const Shader* const s = GetShader();
        if (s)
        {
            s->SetFloat4("u_AlbedoTint", albedoTint_);
            s->SetFloat("u_Metallic", metallic_);
            s->SetFloat("u_Roughness", roughness_);
            s->SetFloat("u_EmissiveStrength", emissiveStrength_);
            s->SetFloat3("u_EmissiveTint", emissiveTint_);
            s->SetInt("u_Textured", albedoTex_ ? 1 : 0); // lets the shader skip sampling when there's no texture
        }
        if (albedoTex_)
        {
            if (s)
            {
                s->SetInt("u_Texture", 0);
            }
            albedoTex_->Bind(0);
        }
        if (normalTex_)
        {
            if (s)
            {
                s->SetInt("u_NormalMap", 1);
            }
            normalTex_->Bind(1);
        }
        if (metallicRoughnessTex_)
        {
            if (s)
            {
                s->SetInt("u_MetallicRoughnessMap", 2);
            }
            metallicRoughnessTex_->Bind(2);
        }
    }
}
