/* !
@file               main.cpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Command line of the asset tool: AtelierAssetTool <input> <output.amodel>
    [--textures-dir <folder>] [--scale <factor>] [--model-folder]. Prints a one-line summary on
    success, the reason on failure, and exits with 0 or 1 so the editor and
    build scripts can run it.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "tool/AssetConverter.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off
#include <windows.h>
#include <shellapi.h> // after windows.h: it needs its types (the formatter would sort it first)
// clang-format on
#endif

// Standard Library Headers
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    // The command line as paths. Windows hands main() the ANSI code page, which cannot hold every folder name a user
    // has, so there the wide command line is read instead.
    std::vector<std::filesystem::path> Arguments(int argc, char** argv)
    {
        std::vector<std::filesystem::path> arguments;
#ifdef _WIN32
        (void)argc;
        (void)argv;
        int count = 0;
        wchar_t** wide = CommandLineToArgvW(GetCommandLineW(), &count);
        for (int i = 1; wide != nullptr && i < count; ++i)
        {
            arguments.emplace_back(wide[i]);
        }
        if (wide != nullptr)
        {
            LocalFree(wide);
        }
#else
        for (int i = 1; i < argc; ++i)
        {
            arguments.emplace_back(argv[i]);
        }
#endif
        return arguments;
    }

    std::string Utf8(const std::filesystem::path& path)
    {
        const auto text = path.u8string();
        return std::string(text.begin(), text.end());
    }

    void PrintUsage()
    {
        std::cerr
            << "usage: AtelierAssetTool <input> <output> [--textures-dir <folder>] [--scale <factor>]\n"
               "                        [--fit <size>] [--center] [--model-folder]\n"
               "  converts an OBJ, FBX, glTF, ... model into the engine's .amodel file and copies its textures\n"
               "  <input>         a model file, or a folder or .zip holding one model with its .mtl and textures\n"
               "  <output>        the .amodel file to write, or a folder to write <model name>.amodel into\n"
               "  --textures-dir  where the textures are copied (default: beside the output)\n"
               "  --scale         multiplies the whole model, e.g. 0.01 for a model made in centimetres\n"
               "  --fit           scales the model so its longest side is <size> long (then --scale applies)\n"
               "  --center        moves the model so the middle of its bounds is at the origin\n"
               "  --model-folder  <output> is the models folder: the model gets <output>/<name>/ with its\n"
               "                  .amodel and its original files; its images (and, from a folder or .zip, the\n"
               "                  pack's other images) go to <textures-dir>/<name>/\n";
    }
}

int main(int argc, char** argv)
{
    Atelier::Tool::ConvertOptions options;
    int positional = 0;
    const std::vector<std::filesystem::path> arguments = Arguments(argc, argv);
    for (std::size_t i = 0; i < arguments.size(); ++i)
    {
        const std::string argument = arguments[i].string();
        if (argument == "--help" || argument == "-h")
        {
            PrintUsage();
            return 0;
        }
        if (argument == "--center")
        {
            options.center = true;
        }
        else if (argument == "--model-folder")
        {
            options.modelFolder = true;
        }
        else if ((argument == "--textures-dir" || argument == "--scale" || argument == "--fit") &&
                 i + 1 < arguments.size())
        {
            const std::filesystem::path& valuePath = arguments[++i];
            if (argument == "--textures-dir")
            {
                options.texturesDir = valuePath;
            }
            else
            {
                const std::string value = valuePath.string();
                char* end = nullptr;
                const float number = std::strtof(value.c_str(), &end);
                if (end == value.c_str() || *end != '\0' || !(number > 0.0f))
                {
                    std::cerr << "error: " << argument << " needs a positive number, got \"" << value << "\"\n";
                    return 1;
                }
                (argument == "--fit" ? options.fitSize : options.scale) = number;
            }
        }
        else if (!argument.empty() && argument[0] != '-' && positional < 2)
        {
            (positional++ == 0 ? options.input : options.output) = arguments[i];
        }
        else
        {
            std::cerr << "error: unexpected argument \"" << argument << "\"\n";
            PrintUsage();
            return 1;
        }
    }
    if (positional != 2)
    {
        PrintUsage();
        return 1;
    }

    const Atelier::Tool::ConvertResult result = Atelier::Tool::ConvertModelFile(options);
    for (const std::string& warning : result.warnings)
    {
        std::cerr << "warning: " << warning << "\n";
    }
    if (!result.ok)
    {
        std::cerr << "error: " << result.error << "\n";
        return 1;
    }
    // The editor reads this line to know which file was written (with a folder as output, the model names it)
    std::cout << "output: " << Utf8(result.output) << "\n";
    std::cout << "converted " << result.source << ": " << result.meshes << " meshes, " << result.materials
              << " materials, " << result.nodes << " nodes, " << result.textures.size() << " textures copied\n";
    return 0;
}
