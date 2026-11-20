/* !
@file    	    Shader.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/09/2026
@brief
    This file contains the implementation of the Shader class, including
    constructor, destructor, move semantics, shader compilation and linking from
    source or files, program binding, and OpenGL uniform variable setting
    utilities.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Graphics Headers
#include "engine/graphics/Shader/Shader.hpp"

#include "engine/core/FileSystem.hpp"
#include "engine/core/Log.hpp"

#include <glm/gtc/type_ptr.inl>

// Standard Library Headers
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Engine::Graphics
{
    /*!***********************************************************************
    @brief          Constructs a Shader with the given name identifier.
    @param          name          Name string for identification and logging purposes.
    *************************************************************************/
    Shader::Shader(std::string name) : name_(std::move(name)) {}

    /*!***********************************************************************
    @brief          Destroys the Shader and releases the underlying OpenGL program.
    *************************************************************************/
    Shader::~Shader()
    {
        Destroy();
    }

    /*!***********************************************************************
    @brief          Move constructor that transfers ownership of the shader program from another instance.
    @param          other         The Shader instance to transfer resources from.
    *************************************************************************/
    Shader::Shader(Shader&& other) noexcept : rendererID_(other.rendererID_), name_(std::move(other.name_))
    {
        other.rendererID_ = 0;
    }

    /*!***********************************************************************
    @brief          Move assignment operator that transfers ownership of the shader program.
    @param          other         The Shader instance to transfer resources from.
    @return         Reference to this Shader instance after the transfer.
    *************************************************************************/
    Shader& Shader::operator=(Shader&& other) noexcept
    {
        if (this != &other)
        {
            Destroy();
            rendererID_ = other.rendererID_;
            name_ = std::move(other.name_);
            other.rendererID_ = 0;
        }
        return *this;
    }

    /*!***********************************************************************
    @brief          Deletes the OpenGL program object and resets the renderer ID.
    *************************************************************************/
    void Shader::Destroy() noexcept
    {
        if (rendererID_ != 0)
        {
            glDeleteProgram(rendererID_);
            rendererID_ = 0;
        }
    }

    /*!***********************************************************************
    @brief          Queries the location of a named uniform variable within the linked program.
    @param          name          Name of the uniform variable to look up.
    @return         GLint uniform location index, or -1 if the program is invalid or the uniform is not found.
    *************************************************************************/
    GLint Shader::UniformLocation(const std::string& name) const
    {
        if (rendererID_ == 0)
        {
            return -1;
        }
        return glGetUniformLocation(rendererID_, name.c_str());
    }

    namespace
    {
        /*!***********************************************************************
        @brief          Compiles a single shader stage (vertex or fragment) from source code.
        @param          type          OpenGL shader type enumeration (GL_VERTEX_SHADER or GL_FRAGMENT_SHADER).
        @param          source        Shader source code string.
        @param          shaderName    Name used for error logging (e.g. "MyShader.vert").
        @return         GLuint shader object ID on success, 0 on compilation failure or missing GL context.
        *************************************************************************/
        GLuint CompileShaderSingle(GLenum type, const std::string& source, const std::string& shaderName)
        {
            if (glCreateShader == nullptr)
            {
                LOG_ERROR(Shader,
                          "{}: glCreateShader is NULL - GLEW was never initialized or no GL context is current. "
                          "Call Window::MakeContextCurrent() before compiling or loading shaders for this window.",
                          shaderName);
                return 0;
            }

            const GLuint shader = glCreateShader(type);
            const char* src = source.c_str();
            const GLint length = static_cast<GLint>(source.size());
            glShaderSource(shader, 1, &src, &length);
            glCompileShader(shader);

            GLint compiled = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if (compiled == GL_FALSE)
            {
                GLint logLen = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
                std::vector<GLchar> log(static_cast<size_t>(logLen) + 1, '\0');
                glGetShaderInfoLog(shader, logLen, &logLen, log.data());
                LOG_ERROR(Shader, "{}: compile error: {}", shaderName, log.data());
                glDeleteShader(shader);
                return 0;
            }
            return shader;
        }

        /*!***********************************************************************
        @brief          RAII guard that makes a program current for the uniform calls of a setter, and puts the
                        previously bound program back when it leaves scope. Nothing is done when the program is
                        already current.
        *************************************************************************/
        class ScopedProgramBind
        {
        public:
            /*!***********************************************************************
            @brief          Makes the given program current, remembering the one that was bound before.
            @param          program       OpenGL program object to make current.
            *************************************************************************/
            explicit ScopedProgramBind(GLuint program)
            {
                glGetIntegerv(GL_CURRENT_PROGRAM, &previous_);
                rebind_ = static_cast<GLuint>(previous_) != program;
                if (rebind_)
                {
                    glUseProgram(program);
                }
            }

            /*!***********************************************************************
            @brief          Destructor that puts the previously bound program back if it was changed.
            *************************************************************************/
            ~ScopedProgramBind()
            {
                if (rebind_)
                {
                    glUseProgram(static_cast<GLuint>(previous_));
                }
            }

            /*!***********************************************************************
            @brief          Deleted copy constructor.
            *************************************************************************/
            ScopedProgramBind(const ScopedProgramBind&) = delete;
            /*!***********************************************************************
            @brief          Deleted copy assignment operator.
            @return         Reference to this ScopedProgramBind.
            *************************************************************************/
            ScopedProgramBind& operator=(const ScopedProgramBind&) = delete;

        private:
            GLint previous_ = 0;
            bool rebind_ = false;
        };

        /*!***********************************************************************
        @brief          Strips leading whitespace characters in-place.
        @param          s             Mutable view of characters; modified on return.
        *************************************************************************/
        void LStrip(std::string_view& s)
        {
            while (!s.empty() && (s[0] == ' ' || s[0] == '\t'))
            {
                s.remove_prefix(1);
            }
        }

        /*!***********************************************************************
        @brief          Returns the directory portion of a file path (everything up to the last separator).
        @details        Returns empty string if the path contains no separator.
        @param          path          File path to take the directory of.
        @return         Directory portion of the path, or an empty string if it has no separator.
        *************************************************************************/
        std::string ParentDirectory(const std::string& path)
        {
            const std::size_t slash = path.find_last_of("/\\");
            if (slash == std::string::npos)
            {
                return {};
            }
            if (slash == 0)
            {
                return path.substr(0, 1); // absolute Unix root
            }
            return path.substr(0, slash);
        }

        /*!***********************************************************************
        @brief          Joins two path components with a single forward separator.
        @details        Handles empty strings on either side gracefully; does not canonicalize "." or ".." (kept
                        intentionally simple so the resulting string can still be opened via std::ifstream on both
                        Windows and POSIX).
        @param          parentDir     Directory component placed first.
        @param          rel           Relative component appended after it.
        @return         The joined path.
        *************************************************************************/
        std::string JoinPaths(const std::string& parentDir, const std::string& rel)
        {
            if (parentDir.empty())
            {
                return rel;
            }
            if (rel.empty())
            {
                return parentDir;
            }
            std::string out = parentDir;
            out.push_back('/');
            out.append(rel);
            return out;
        }

        /*!***********************************************************************
        @brief          Very small C-style `#include "file"` preprocessor for GLSL sources.
        @details        Only the quoted form is supported (angle brackets are ignored so
                        standard-library-like paths can be detected as intentional).
                        Include paths are resolved relative to the INCLUDING source file's
                        directory. A max-depth guard prevents infinite recursion.
        @param          source        Input source (may already have content from a prior include).
        @param          originDir     Directory containing the source being processed, used to resolve
                                     relative include paths. Pass empty string for non-file sources
                                     (includes are then skipped).
        @param          depth         Current nesting depth; callers pass 0 for the top level.
        @return         New source string with includes expanded (never returns empty on
                        valid input: original is preserved if includes fail).
        *************************************************************************/
        std::string ExpandIncludes(std::string source, const std::string& originDir, int depth = 0)
        {
            constexpr int kMaxIncludeDepth = 4;
            if (depth >= kMaxIncludeDepth)
            {
                return source;
            }

            std::istringstream in(source);
            std::ostringstream out;
            std::string line;
            bool anyIncluded = false;
            while (std::getline(in, line))
            {
                std::string_view view(line);
                LStrip(view);
                if (!view.empty() && view[0] == '#')
                {
                    view.remove_prefix(1);
                    LStrip(view);
                    constexpr std::string_view kInclude = "include";
                    if (view.size() >= kInclude.size() && view.substr(0, kInclude.size()) == kInclude)
                    {
                        std::string_view rest = view.substr(kInclude.size());
                        LStrip(rest);
                        if (!rest.empty() && rest[0] == '"')
                        {
                            const auto closing = rest.find('"', 1);
                            if (closing != std::string_view::npos)
                            {
                                const std::string rel(rest.substr(1, closing - 1));
                                const std::string incPath = JoinPaths(originDir, rel);
                                std::string includeSource = Core::ReadText(incPath).value_or("");
                                if (!includeSource.empty())
                                {
                                    const std::string incDir = ParentDirectory(incPath);
                                    out << ExpandIncludes(std::move(includeSource), incDir, depth + 1);
                                    out << "\n";
                                    anyIncluded = true;
                                    continue;
                                }
                                // Fall-through: leave the original #include line in the output
                                // so the GLSL compiler produces a diagnostic for missing files.
                                LOG_ERROR(Shader, "[Shader #include] WARNING: could not open include: {}", incPath);
                            }
                        }
                    }
                }
                out << line << '\n';
            }
            if (!anyIncluded)
            {
                return source;
            }
            return out.str();
        }
    }

    /*!***********************************************************************
    @brief          Compiles vertex and fragment shader sources, links them into an OpenGL program, and stores the
                    program ID.
    @param          vertexSource    Source code string for the vertex shader stage.
    @param          fragmentSource  Source code string for the fragment shader stage.
    @return         True if compilation and linking completed without error; false otherwise with diagnostic logs.
    *************************************************************************/
    bool Shader::Compile(const std::string& vertexSource, const std::string& fragmentSource)
    {
        Destroy();

        const GLuint vs = CompileShaderSingle(GL_VERTEX_SHADER, vertexSource, name_ + ".vert");
        if (vs == 0)
        {
            return false;
        }
        const GLuint fs = CompileShaderSingle(GL_FRAGMENT_SHADER, fragmentSource, name_ + ".frag");
        if (fs == 0)
        {
            glDeleteShader(vs);
            return false;
        }

        if (glCreateProgram == nullptr)
        {
            LOG_ERROR(Shader, "{}: glCreateProgram is NULL - GL context not initialized.", name_);
            glDeleteShader(vs);
            glDeleteShader(fs);
            return false;
        }

        const GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);

        GLint linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked == GL_FALSE)
        {
            GLint logLen = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
            std::vector<GLchar> log(static_cast<size_t>(logLen) + 1, '\0');
            glGetProgramInfoLog(program, logLen, &logLen, log.data());
            LOG_ERROR(Shader, "{}: link error: {}", name_, log.data());
            glDeleteShader(vs);
            glDeleteShader(fs);
            glDeleteProgram(program);
            return false;
        }

        glDeleteShader(vs);
        glDeleteShader(fs);
        rendererID_ = program;
        return true;
    }

    /*!***********************************************************************
    @brief          Reads vertex and fragment shader files from disk, then compiles and links them into a program.
    @param          vertexPath    Filesystem path to the vertex shader source file.
    @param          fragmentPath  Filesystem path to the fragment shader source file.
    @return         True if file reading, compilation, and linking all succeeded; false otherwise.
    *************************************************************************/
    bool Shader::LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath)
    {
        const std::string rawVs = Core::ReadText(vertexPath).value_or("");
        if (rawVs.empty())
        {
            LOG_ERROR(Shader, "Could not read vertex shader: {}", vertexPath);
            return false;
        }
        const std::string rawFs = Core::ReadText(fragmentPath).value_or("");
        if (rawFs.empty())
        {
            LOG_ERROR(Shader, "Could not read fragment shader: {}", fragmentPath);
            return false;
        }
        // Expand #include "..." directives relative to each source file's directory
        // before passing the fully-expanded text to the GLSL compiler.
        const std::string vs = ExpandIncludes(rawVs, ParentDirectory(vertexPath));
        const std::string fs = ExpandIncludes(rawFs, ParentDirectory(fragmentPath));
        return Compile(vs, fs);
    }

    /*!***********************************************************************
    @brief          Compiles and links a pre-loaded CPU shader-data struct.
    @param          shaderAsset   CPU-side shader data with vertexSource + fragmentSource.
    @return         True if compilation and linking succeeded; false otherwise with diagnostic logs.
    *************************************************************************/
    bool Shader::Compile(const Assets::ShaderData& shaderAsset)
    {
        return Compile(shaderAsset.vertexSource, shaderAsset.fragmentSource);
    }

    /*!***********************************************************************
    @brief          Binds the shader program to the current OpenGL context for rendering.
    *************************************************************************/
    void Shader::Bind() const
    {
        if (rendererID_ != 0)
        {
            glUseProgram(rendererID_);
        }
    }

    /*!***********************************************************************
    @brief          Unbinds any currently bound shader program by binding program 0.
    *************************************************************************/
    void Shader::Unbind() const
    {
        glUseProgram(0);
    }

    /*!***********************************************************************
    @brief          Uploads an integer scalar uniform to the shader, temporarily rebinding the program if necessary.
    @param          name          Uniform variable name in the shader source.
    @param          value         Integer value to upload.
    *************************************************************************/
    void Shader::SetInt(const std::string& name, int value) const
    {
        const GLint loc = UniformLocation(name);
        if (loc < 0)
        {
            return;
        }
        ScopedProgramBind bind(rendererID_);
        glUniform1i(loc, value);
    }

    /*!***********************************************************************
    @brief          Uploads a float scalar uniform to the shader, temporarily rebinding the program if necessary.
    @param          name          Uniform variable name in the shader source.
    @param          value         Float value to upload.
    *************************************************************************/
    void Shader::SetFloat(const std::string& name, float value) const
    {
        const GLint loc = UniformLocation(name);
        if (loc < 0)
        {
            return;
        }
        ScopedProgramBind bind(rendererID_);
        glUniform1f(loc, value);
    }

    /*!***********************************************************************
    @brief          Uploads a 2-component float vector uniform to the shader, temporarily rebinding if necessary.
    @param          name          Uniform variable name in the shader source.
    @param          value         2D vector value to upload.
    *************************************************************************/
    void Shader::SetFloat2(const std::string& name, const glm::vec2& value) const
    {
        const GLint loc = UniformLocation(name);
        if (loc < 0)
        {
            return;
        }
        ScopedProgramBind bind(rendererID_);
        glUniform2f(loc, value.x, value.y);
    }

    /*!***********************************************************************
    @brief          Uploads a 3-component float vector uniform to the shader, temporarily rebinding if necessary.
    @param          name          Uniform variable name in the shader source.
    @param          value         3D vector value to upload.
    *************************************************************************/
    void Shader::SetFloat3(const std::string& name, const glm::vec3& value) const
    {
        const GLint loc = UniformLocation(name);
        if (loc < 0)
        {
            return;
        }
        ScopedProgramBind bind(rendererID_);
        glUniform3f(loc, value.x, value.y, value.z);
    }

    /*!***********************************************************************
    @brief          Uploads a 4-component float vector uniform to the shader, temporarily rebinding if necessary.
    @param          name          Uniform variable name in the shader source.
    @param          value         4D vector value to upload.
    *************************************************************************/
    void Shader::SetFloat4(const std::string& name, const glm::vec4& value) const
    {
        const GLint loc = UniformLocation(name);
        if (loc < 0)
        {
            return;
        }
        ScopedProgramBind bind(rendererID_);
        glUniform4f(loc, value.x, value.y, value.z, value.w);
    }

    /*!***********************************************************************
    @brief          Uploads a 3x3 matrix uniform to the shader, temporarily rebinding if necessary.
    @param          name          Uniform variable name in the shader source.
    @param          value         3x3 matrix value to upload (uploaded without transposition).
    *************************************************************************/
    void Shader::SetMat3(const std::string& name, const glm::mat3& value) const
    {
        const GLint loc = UniformLocation(name);
        if (loc < 0)
        {
            return;
        }
        ScopedProgramBind bind(rendererID_);
        glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(value));
    }

    /*!***********************************************************************
    @brief          Uploads a 4x4 matrix uniform to the shader, temporarily rebinding if necessary.
    @param          name          Uniform variable name in the shader source.
    @param          value         4x4 matrix value to upload (uploaded without transposition).
    *************************************************************************/
    void Shader::SetMat4(const std::string& name, const glm::mat4& value) const
    {
        const GLint loc = UniformLocation(name);
        if (loc < 0)
        {
            return;
        }
        ScopedProgramBind bind(rendererID_);
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
    }

    /*!***********************************************************************
    @brief          Uploads the first count matrices of a 4x4 matrix array uniform, temporarily rebinding if necessary.
    @param          name          Array uniform name in the shader source ("u_Bones").
    @param          values        Matrices to upload (uploaded without transposition).
    @param          count         How many to upload.
    *************************************************************************/
    void Shader::SetMat4Array(const std::string& name, const glm::mat4* values, std::size_t count) const
    {
        // The array's first element is where it starts; drivers report it as "u_Bones[0]" or "u_Bones"
        GLint loc = UniformLocation(name + "[0]");
        if (loc < 0)
        {
            loc = UniformLocation(name);
        }
        if (loc < 0 || count == 0 || values == nullptr)
        {
            return;
        }
        ScopedProgramBind bind(rendererID_);
        glUniformMatrix4fv(loc, static_cast<GLsizei>(count), GL_FALSE, glm::value_ptr(values[0]));
    }
}
