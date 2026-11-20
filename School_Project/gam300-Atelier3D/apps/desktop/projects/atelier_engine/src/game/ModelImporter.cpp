/* !
@file               ModelImporter.cpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Implements ImportModel: runs AtelierAssetTool on the model and reports what
    it did.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/game/ModelImporter.hpp"

// Engine Headers
#include "engine/assets/loaders/AtelierModel.hpp"
#include "engine/game/AssetLayout.hpp"
#include "engine/platform/Paths.hpp"
#include "engine/platform/Process.hpp"

// Standard Library Headers
#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace Engine::Game
{
    namespace
    {
        std::string ToLower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        bool StartsWith(const std::string& text, const std::string& prefix)
        {
            return text.compare(0, prefix.size(), prefix) == 0;
        }

        // The tool prints "warning: ..." lines, "output: <file written>" and, when it fails, one "error: ..." line
        void SplitOutput(const std::string& output, std::vector<std::string>& warnings, std::string& error,
                         std::string& written)
        {
            std::istringstream lines(output);
            std::string line;
            while (std::getline(lines, line))
            {
                while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
                {
                    line.pop_back();
                }
                if (StartsWith(line, "warning: "))
                {
                    warnings.push_back(line.substr(9));
                }
                else if (StartsWith(line, "error: "))
                {
                    error = line.substr(7);
                }
                else if (StartsWith(line, "output: "))
                {
                    written = line.substr(8);
                }
            }
        }
    }

    bool IsImportableModel(const std::filesystem::path& file)
    {
        const std::string extension = ToLower(file.extension().string());
        return std::find(kImportableModelExtensions.begin(), kImportableModelExtensions.end(), extension) !=
               kImportableModelExtensions.end();
    }

    bool IsModelFile(const std::filesystem::path& file)
    {
        return IsImportableModel(file) || ToLower(file.extension().string()) == Assets::kAtelierModelExtension;
    }

    bool IsModelPackage(const std::filesystem::path& path)
    {
        std::error_code ec;
        if (std::filesystem::is_regular_file(path, ec))
        {
            return ToLower(path.extension().string()) == ".zip";
        }
        if (!std::filesystem::is_directory(path, ec))
        {
            return false;
        }
        // The same search the tool makes: model files a few folders down, all of one name
        constexpr int kDeepest = 4;
        std::set<std::string> names;
        for (std::filesystem::recursive_directory_iterator
                 it(path, std::filesystem::directory_options::skip_permission_denied, ec),
             end;
             !ec && it != end; it.increment(ec))
        {
            if (it->is_directory(ec))
            {
                if (it.depth() >= kDeepest || it->path().filename() == "__MACOSX")
                {
                    it.disable_recursion_pending();
                }
            }
            else if (it->is_regular_file(ec) && IsImportableModel(it->path()))
            {
                names.insert(ToLower(it->path().stem().string()));
            }
        }
        return names.size() == 1;
    }

    std::filesystem::path AssetToolPath()
    {
#ifdef _WIN32
        const std::filesystem::path tool = Platform::ExecutableDir() / "AtelierAssetTool.exe";
#else
        const std::filesystem::path tool = Platform::ExecutableDir() / "AtelierAssetTool";
#endif
        std::error_code ec;
        return std::filesystem::is_regular_file(tool, ec) ? tool : std::filesystem::path{};
    }

    ModelImportResult ImportModel(const GameProject& project, const std::filesystem::path& source,
                                  const std::filesystem::path& modelsFolder, const std::filesystem::path& tool)
    {
        const bool package = IsModelPackage(source);
        if (!package && !IsModelFile(source))
        {
            throw std::runtime_error("Cannot import " + source.filename().string() +
                                     ": expected a model (.obj, .fbx, .gltf, .glb, .dae, .3ds, .ply, .stl, .amodel), "
                                     "or a folder or .zip with one model in it");
        }
        std::error_code ec;
        if (!package && !std::filesystem::is_regular_file(source, ec))
        {
            throw std::runtime_error("Not a file: " + source.string());
        }

        ModelImportResult result;
        const std::string extension = ToLower(source.extension().string());
        // Every model lives in a folder named after it
        const std::filesystem::path ownFolder = modelsFolder / source.stem();
        if (!package && extension == Assets::kAtelierModelExtension)
        {
            result.path = project.ImportAsset(source, ownFolder); // already converted: copied with the images it names
            // Images it named by bare file name came along beside it: they belong in art/textures/<name>/
            if (NeedsLayoutMigration(project.ProjectDir()))
            {
                MigrateAssetLayout(project.ProjectDir());
            }
            return result;
        }
        if (tool.empty() || !std::filesystem::is_regular_file(tool, ec))
        {
            if (package)
            {
                throw std::runtime_error("Cannot import " + source.filename().string() +
                                         ": a folder or .zip needs AtelierAssetTool next to the editor");
            }
            // Without the tool the engine's own loaders still read these formats
            if (extension != ".obj" && extension != ".fbx" && extension != ".glb" && extension != ".gltf")
            {
                throw std::runtime_error("Cannot import " + source.filename().string() +
                                         ": AtelierAssetTool was not found next to the editor");
            }
            result.path = project.ImportAsset(source, ownFolder);
            result.warnings.push_back("AtelierAssetTool was not found, so the model was copied as it is");
            return result;
        }

        // The tool is given the models and textures folders and makes the model's own folder in each, named after the
        // model (the one inside a pack), reusing them when the same model is imported again
        const std::filesystem::path output = project.ProjectDir() / modelsFolder;
        const std::filesystem::path textures = project.ProjectDir() / kTexturesFolder;
        // --center and --fit: models keep the size and place their author used (a car in centimetres, books 76 units
        // from the origin), which leaves them off screen or huge, so they are brought to the origin at a size that
        // suits the editor's camera. The Transform scale changes it from there.
        const Platform::ProcessResult run =
            Platform::RunProcess(tool, {source, output, "--model-folder", "--textures-dir", textures, "--center",
                                        "--fit", std::to_string(kImportedModelSize)});

        std::string error;
        std::string written;
        SplitOutput(run.output, result.warnings, error, written);
        if (!run.started)
        {
            throw std::runtime_error("Could not start " + tool.string());
        }
        if (run.timedOut)
        {
            throw std::runtime_error("Converting " + source.filename().string() + " took too long and was stopped");
        }
        if (run.exitCode != 0)
        {
            throw std::runtime_error(
                "Could not convert " + source.filename().string() + ": " +
                (error.empty() ? "the asset tool failed (code " + std::to_string(run.exitCode) + ")" : error));
        }
        // The tool prints the file as UTF-8
        if (written.empty())
        {
            throw std::runtime_error("Could not convert " + source.filename().string() +
                                     ": the asset tool did not say which model it wrote");
        }
        const std::filesystem::path file(std::u8string(written.begin(), written.end()));
        result.path = file.lexically_relative(project.ProjectDir()).generic_string();
        return result;
    }
}
