/* !
@file    	    Material.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    This file provides the Material class, which describes how a surface reacts
    to light. It stores a shader reference, PBR-style scalar parameters (albedo
    tint, metallic, roughness, emissive), and associated texture maps for
    rendering mesh surfaces.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/graphics/Shader/Shader.hpp"
#include "engine/graphics/Texture/Texture.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <string>

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          Describes surface appearance properties including shader, PBR parameters, and texture maps.
    *************************************************************************/
    class Material
    {
    public:
        /*!***********************************************************************
        @brief          Default constructor that creates an unnamed material with default parameters.
        *************************************************************************/
        Material() = default;

        /*!***********************************************************************
        @brief          Constructs a Material with the given name identifier.
        @param          name          Name string for identification of this material.
        *************************************************************************/
        explicit Material(std::string name);

        /*!***********************************************************************
        @brief          Returns the name identifier of this material.
        @return         Const reference to the material name string.
        *************************************************************************/
        const std::string& GetName() const { return name_; }

        /*!***********************************************************************
        @brief          Sets the name identifier of this material.
        @param          name          New name for the material.
        *************************************************************************/
        void SetName(std::string name) { name_ = std::move(name); }

        /*!***********************************************************************
        @brief          Returns the RGBA albedo color tint multiplier applied to the surface.
        @return         Const reference to the 4-component RGBA albedo tint vector.
        *************************************************************************/
        const glm::vec4& GetAlbedoTint() const { return albedoTint_; }

        /*!***********************************************************************
        @brief          Sets the RGBA albedo color tint multiplier applied to the surface.
        @param          tint          4-component RGBA color tint value.
        *************************************************************************/
        void SetAlbedoTint(const glm::vec4& tint) { albedoTint_ = tint; }

        /*!***********************************************************************
        @brief          Returns the metallic PBR parameter of the surface.
        @return         Metallic value (typically in range 0.0 to 1.0).
        *************************************************************************/
        float GetMetallic() const { return metallic_; }

        /*!***********************************************************************
        @brief          Sets the metallic PBR parameter of the surface.
        @param          v             Metallic value (typically in range 0.0 to 1.0).
        *************************************************************************/
        void SetMetallic(float v) { metallic_ = v; }

        /*!***********************************************************************
        @brief          Returns the roughness PBR parameter of the surface.
        @return         Roughness value (typically in range 0.0 to 1.0).
        *************************************************************************/
        float GetRoughness() const { return roughness_; }

        /*!***********************************************************************
        @brief          Sets the roughness PBR parameter of the surface.
        @param          v             Roughness value (typically in range 0.0 to 1.0).
        *************************************************************************/
        void SetRoughness(float v) { roughness_ = v; }

        /*!***********************************************************************
        @brief          Returns the emissive strength multiplier for self-illumination.
        @return         Emissive strength scalar multiplier.
        *************************************************************************/
        float GetEmissiveStrength() const { return emissiveStrength_; }

        /*!***********************************************************************
        @brief          Sets the emissive strength multiplier for self-illumination.
        @param          v             Emissive strength scalar multiplier.
        *************************************************************************/
        void SetEmissiveStrength(float v) { emissiveStrength_ = v; }

        /*!***********************************************************************
        @brief          Returns the RGB tint applied to the emissive contribution of the surface.
        @return         Const reference to the 3-component RGB emissive tint vector.
        *************************************************************************/
        const glm::vec3& GetEmissiveTint() const { return emissiveTint_; }

        /*!***********************************************************************
        @brief          Sets the RGB tint applied to the emissive contribution of the surface.
        @param          tint          3-component RGB emissive tint value.
        *************************************************************************/
        void SetEmissiveTint(const glm::vec3& tint) { emissiveTint_ = tint; }

        /*!***********************************************************************
        @brief          Sets the shader to be associated with this material for rendering.
        @param          s             Shared pointer to the Shader to use for drawing
                                       (copied for lvalues, moved for rvalues).
        *************************************************************************/
        void SetShader(std::shared_ptr<Shader> s)
        {
            shader_ = std::move(s);
            shaderRaw_ = nullptr;
        }

        /*!***********************************************************************
        @brief          Sets a non-owning shader reference (used for temporary stack-allocated Materials
                        whose shader lifetime is already guaranteed by a caller-owned value).
        @param          s             Shader reference to use; must outlive all draw calls using this material.
        *************************************************************************/
        void SetShader(const Shader& s)
        {
            shaderRaw_ = const_cast<Shader*>(&s);
            shader_ = nullptr;
        }

        /*!***********************************************************************
        @brief          Returns the shader associated with this material for rendering (non-owning first, then
                        the stored shared pointer). Returns nullptr if neither is set.
        *************************************************************************/
        Shader* GetShader() const { return shaderRaw_ ? shaderRaw_ : shader_.get(); }

        /*!***********************************************************************
        @brief          Returns the albedo (base diffuse) color texture map.
        @return         Shared pointer to the albedo Texture.
        *************************************************************************/
        std::shared_ptr<Texture> GetAlbedoTexture() const { return albedoTex_; }

        /*!***********************************************************************
        @brief          Sets the albedo (base diffuse) color texture map.
        @param          t             Shared pointer to the albedo Texture to use.
        *************************************************************************/
        void SetAlbedoTexture(std::shared_ptr<Texture> t) { albedoTex_ = std::move(t); }

        /*!***********************************************************************
        @brief          Returns the normal (bump) map texture for perturbing surface normals.
        @return         Shared pointer to the normal map Texture.
        *************************************************************************/
        std::shared_ptr<Texture> GetNormalTexture() const { return normalTex_; }

        /*!***********************************************************************
        @brief          Sets the normal (bump) map texture for perturbing surface normals.
        @param          t             Shared pointer to the normal map Texture to use.
        *************************************************************************/
        void SetNormalTexture(std::shared_ptr<Texture> t) { normalTex_ = std::move(t); }

        /*!***********************************************************************
        @brief          Returns the combined metallic-roughness texture map.
                        Convention: G channel = roughness, B channel = metallic.
        @return         Shared pointer to the metallic-roughness Texture.
        *************************************************************************/
        std::shared_ptr<Texture> GetMetallicRoughnessTexture() const { return metallicRoughnessTex_; }
        /*!***********************************************************************
        @brief          Sets the combined metallic-roughness texture map (G = roughness, B = metallic).
        @param          t             Shared pointer to the metallic-roughness Texture to use.
        *************************************************************************/
        void SetMetallicRoughnessTexture(std::shared_ptr<Texture> t) { metallicRoughnessTex_ = std::move(t); }

        /*!***********************************************************************
        @brief          Binds all material-owned resources that the shader expects: sets albedo tint,
                        PBR scalars (metallic/roughness/emissive) as uniforms on the bound shader, and
                        binds the albedo/normal/metallicRoughness textures on units 0/1/2.
        *************************************************************************/
        void Bind() const;

    private:
        std::string name_;
        glm::vec4 albedoTint_ = glm::vec4(1.0f);
        float metallic_ = 0.0f;
        float roughness_ = 0.5f;
        glm::vec3 emissiveTint_ = glm::vec3(0.0f);
        float emissiveStrength_ = 0.0f;

        std::shared_ptr<Shader> shader_ = nullptr;
        Shader* shaderRaw_ = nullptr;
        std::shared_ptr<Texture> albedoTex_ = nullptr;
        std::shared_ptr<Texture> normalTex_ = nullptr;
        std::shared_ptr<Texture> metallicRoughnessTex_ = nullptr;
    };
}
