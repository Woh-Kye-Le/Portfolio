/* !
@file               ShaderLoader.cpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Implements the shader file loader: reads the .vert file and the matching
    .frag beside it, expands quoted #include directives, and provides the
    built-in unlit fallback shader source.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/assets/loaders/ShaderLoader.hpp"

// Engine Core Headers
#include "engine/core/FileSystem.hpp"

// Standard Library Headers
#include <sstream>
#include <string>
#include <utility>

namespace Engine::Assets
{
    namespace
    {
        // Very small GLSL `#include "..."` expander, relative to each source file's directory
        std::string ExpandShaderIncludes(std::string source, const std::filesystem::path& originDir, int depth = 0)
        {
            constexpr int kMaxIncludeDepth = 4;
            if (depth >= kMaxIncludeDepth)
            {
                return source;
            }

            bool anyIncluded = false;
            std::ostringstream out;
            std::istringstream ss(source);
            std::string line;

            while (std::getline(ss, line))
            {
                std::string trimmed = line;
                const auto begin = trimmed.find_first_not_of(" \t\r");
                if (begin != std::string::npos)
                {
                    trimmed = trimmed.substr(begin);
                }

                const bool isInclude = trimmed.rfind("#include", 0) == 0;
                if (isInclude)
                {
                    const auto quote = trimmed.find('"', 8);
                    if (quote != std::string::npos)
                    {
                        const auto endQuote = trimmed.find('"', quote + 1);
                        if (endQuote != std::string::npos)
                        {
                            const std::string rel = trimmed.substr(quote + 1, endQuote - quote - 1);
                            const std::filesystem::path incPath = originDir / rel;
                            auto incText = Core::ReadText(incPath);
                            if (incText)
                            {
                                const std::filesystem::path incOrigin =
                                    incPath.has_parent_path() ? incPath.parent_path() : originDir;
                                out << ExpandShaderIncludes(std::move(*incText), incOrigin, depth + 1);
                                out << '\n';
                                anyIncluded = true;
                                continue;
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

        /*!***********************************************************************
        @brief          Returns the parent directory of a path, or "." when it has none.
        @param          p             Path to take the parent of.
        @return         Parent directory, or "." for a bare file name.
        *************************************************************************/
        std::filesystem::path ParentDir(const std::filesystem::path& p)
        {
            return p.has_parent_path() ? p.parent_path() : std::filesystem::path{"."};
        }
    }

    /*!***********************************************************************
    @brief          Loads a shader pair: the .vert file plus the matching .frag.
    @param          path          Vertex shader file to read.
    @return         Populated ShaderData, or nullopt when either file is missing.
    *************************************************************************/
    std::optional<ShaderData> LoadShaderFromFile(const std::filesystem::path& path)
    {
        auto fragmentPath = path;
        fragmentPath.replace_extension(".frag");

        auto vertex = Core::ReadText(path);
        auto fragment = Core::ReadText(fragmentPath);
        if (!vertex || !fragment)
        {
            return std::nullopt;
        }

        ShaderData s;
        s.vertexSource = ExpandShaderIncludes(std::move(*vertex), ParentDir(path));
        s.fragmentSource = ExpandShaderIncludes(std::move(*fragment), ParentDir(fragmentPath));
        return s;
    }

    /*!***********************************************************************
    @brief          Builds the built-in unlit textured fallback shader.
    @return         ShaderData for the fallback program.
    *************************************************************************/
    ShaderData PlaceholderShader()
    {
        ShaderData s;
        s.vertexSource = "#version 330 core\n"
                         "layout(location = 0) in vec3 a_Position;\n"
                         "layout(location = 2) in vec2 a_TexCoord;\n"
                         "uniform mat4 u_Model;\n"
                         "uniform mat4 u_ViewProjection;\n"
                         "out vec2 v_TexCoord;\n"
                         "void main()\n"
                         "{\n"
                         "    v_TexCoord = a_TexCoord;\n"
                         "    gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);\n"
                         "}\n";
        s.fragmentSource = "#version 330 core\n"
                           "in vec2 v_TexCoord;\n"
                           "uniform sampler2D u_Texture;\n"
                           "out vec4 o_Color;\n"
                           "void main()\n"
                           "{\n"
                           "    o_Color = texture(u_Texture, v_TexCoord);\n"
                           "}\n";
        return s;
    }
}
