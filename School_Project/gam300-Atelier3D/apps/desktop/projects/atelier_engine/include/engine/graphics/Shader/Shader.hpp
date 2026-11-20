/* !
@file    	    Shader.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    This file provides the Shader class, which encapsulates an OpenGL shader
    program consisting of vertex and fragment shaders. It handles compilation,
    linking, binding, and uniform value setting for rendering operations.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/assets/data/ShaderData.hpp"

#include <glm/glm.hpp>

#include <GL/glew.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          Encapsulates an OpenGL shader program with compilation, linking, and uniform management.
    *************************************************************************/
    class Shader
    {
    public:
        /*!***********************************************************************
        @brief          Constructs a Shader with an optional name identifier.
        @param          name          Optional name for identification and logging.
        *************************************************************************/
        explicit Shader(std::string name = "");

        /*!***********************************************************************
        @brief          Destroys the Shader and releases underlying OpenGL resources.
        *************************************************************************/
        ~Shader();

        /*!***********************************************************************
        @brief          Deleted copy constructor (GPU resources have a single owner).
        *************************************************************************/
        Shader(const Shader&) = delete;
        /*!***********************************************************************
        @brief          Deleted copy assignment operator (GPU resources have a single owner).
        @return         Reference to this Shader.
        *************************************************************************/
        Shader& operator=(const Shader&) = delete;

        /*!***********************************************************************
        @brief          Move constructor that transfers ownership of the shader program.
        @param          other         The Shader instance to move from.
        *************************************************************************/
        Shader(Shader&& other) noexcept;

        /*!***********************************************************************
        @brief          Move assignment operator that transfers ownership of the shader program.
        @param          other         The Shader instance to move from.
        @return         Reference to this Shader instance.
        *************************************************************************/
        Shader& operator=(Shader&& other) noexcept;

        /*!***********************************************************************
        @brief          Compiles and links vertex and fragment shader sources into a program.
        @param          vertexSource    Source code of the vertex shader.
        @param          fragmentSource  Source code of the fragment shader.
        @return         True if compilation and linking succeeded, false otherwise.
        *************************************************************************/
        bool Compile(const std::string& vertexSource, const std::string& fragmentSource);

        /*!***********************************************************************
        @brief          Loads, compiles, and links shaders from the specified file paths.
        @param          vertexPath    File path to the vertex shader source.
        @param          fragmentPath  File path to the fragment shader source.
        @return         True if loading, compilation, and linking succeeded, false otherwise.
        *************************************************************************/
        bool LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);

        /*!***********************************************************************
        @brief          Compiles and links a pre-loaded CPU shader-data struct.
        @param          shaderAsset   CPU-side shader data with vertexSource + fragmentSource.
        @return         True if compilation and linking succeeded, false otherwise.
        *************************************************************************/
        bool Compile(const Assets::ShaderData& shaderAsset);

        /*!***********************************************************************
        @brief          Binds the shader program for use in subsequent rendering commands.
        *************************************************************************/
        void Bind() const;

        /*!***********************************************************************
        @brief          Unbinds the current shader program (binds program 0).
        *************************************************************************/
        void Unbind() const;

        /*!***********************************************************************
        @brief          Sets an integer uniform value in the shader program.
        @param          name          Name of the uniform variable.
        @param          value         Integer value to set.
        *************************************************************************/
        void SetInt(const std::string& name, int value) const;

        /*!***********************************************************************
        @brief          Sets a float uniform value in the shader program.
        @param          name          Name of the uniform variable.
        @param          value         Float value to set.
        *************************************************************************/
        void SetFloat(const std::string& name, float value) const;

        /*!***********************************************************************
        @brief          Sets a 2-component float vector uniform value in the shader program.
        @param          name          Name of the uniform variable.
        @param          value         2D vector value to set.
        *************************************************************************/
        void SetFloat2(const std::string& name, const glm::vec2& value) const;

        /*!***********************************************************************
        @brief          Sets a 3-component float vector uniform value in the shader program.
        @param          name          Name of the uniform variable.
        @param          value         3D vector value to set.
        *************************************************************************/
        void SetFloat3(const std::string& name, const glm::vec3& value) const;

        /*!***********************************************************************
        @brief          Sets a 4-component float vector uniform value in the shader program.
        @param          name          Name of the uniform variable.
        @param          value         4D vector value to set.
        *************************************************************************/
        void SetFloat4(const std::string& name, const glm::vec4& value) const;

        /*!***********************************************************************
        @brief          Sets a 3x3 matrix uniform value in the shader program.
        @param          name          Name of the uniform variable.
        @param          value         3x3 matrix value to set.
        *************************************************************************/
        void SetMat3(const std::string& name, const glm::mat3& value) const;

        /*!***********************************************************************
        @brief          Sets a 4x4 matrix uniform value in the shader program.
        @param          name          Name of the uniform variable.
        @param          value         4x4 matrix value to set.
        *************************************************************************/
        void SetMat4(const std::string& name, const glm::mat4& value) const;

        /*!***********************************************************************
        @brief          Sets the first `count` elements of a 4x4 matrix array uniform (e.g. "u_Bones").
        @param          name          Name of the array uniform.
        @param          values        The matrices.
        @param          count         How many of them.
        *************************************************************************/
        void SetMat4Array(const std::string& name, const glm::mat4* values, std::size_t count) const;

        /*!***********************************************************************
        @brief          Returns the underlying OpenGL program renderer ID.
        @return         OpenGL program object ID (0 if not compiled).
        *************************************************************************/
        uint32_t GetRendererID() const { return rendererID_; }

        /*!***********************************************************************
        @brief          Returns the name identifier of this shader.
        @return         Const reference to the shader name string.
        *************************************************************************/
        const std::string& GetName() const { return name_; }

    private:
        /*!***********************************************************************
        @brief          Retrieves the location of a uniform variable in the compiled program.
        @param          name          Name of the uniform variable to look up.
        @return         Uniform location index, or -1 if not found or program is invalid.
        *************************************************************************/
        GLint UniformLocation(const std::string& name) const;

        /*!***********************************************************************
        @brief          Destroys the underlying OpenGL shader program if one exists.
        *************************************************************************/
        void Destroy() noexcept;

        GLuint rendererID_ = 0;
        std::string name_;
    };
}
