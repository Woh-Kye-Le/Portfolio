/* !
@file               ZipReader.cpp
@author             w.kyele@digipen.edu [100%]
@date               30/10/2026
@brief
    Implements ExtractZip: reads the archive's central directory and writes
    each entry out, inflating deflated ones with the zlib decoder that
    stb_image already contains (so no extra library is needed).

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "tool/ZipReader.hpp"

// stb_image's inflate is the only part used; it is compiled privately into this file (static, PNG reader only) so it
// cannot clash with the copy inside assimp
#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

// Standard Library Headers
#include <algorithm>
#include <climits>
#include <cstdint>
#include <fstream>
#include <optional>
#include <system_error>

namespace Atelier::Tool
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr std::uint32_t kEndOfDirectory = 0x06054b50;
        constexpr std::uint32_t kDirectoryEntry = 0x02014b50;
        constexpr std::uint32_t kLocalHeader = 0x04034b50;
        constexpr std::size_t kEndOfDirectorySize = 22;
        constexpr std::size_t kLongestComment = 0xFFFF;

        std::uint16_t U16(const std::vector<char>& bytes, std::size_t at)
        {
            const auto b = [&](std::size_t i)
            { return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i])); };
            return static_cast<std::uint16_t>(b(at) | (b(at + 1) << 8));
        }

        std::uint32_t U32(const std::vector<char>& bytes, std::size_t at)
        {
            return static_cast<std::uint32_t>(U16(bytes, at)) | (static_cast<std::uint32_t>(U16(bytes, at + 2)) << 16);
        }

        // count bytes from offset, or nothing when the file is shorter
        bool ReadAt(std::ifstream& file, std::uint64_t offset, std::size_t count, std::vector<char>& out)
        {
            out.resize(count);
            file.clear();
            file.seekg(static_cast<std::streamoff>(offset));
            file.read(out.data(), static_cast<std::streamsize>(count));
            return file.gcount() == static_cast<std::streamsize>(count);
        }

        // The entry's path inside the target folder, or nothing when it would land outside it
        std::optional<fs::path> SafeRelative(std::string name)
        {
            std::replace(name.begin(), name.end(), '\\', '/');
            if (name.empty() || name.front() == '/' || name.find(':') != std::string::npos)
            {
                return std::nullopt;
            }
            fs::path relative;
            std::size_t start = 0;
            while (start <= name.size())
            {
                const std::size_t end = std::min(name.find('/', start), name.size());
                const std::string part = name.substr(start, end - start);
                if (part == "..")
                {
                    return std::nullopt;
                }
                if (!part.empty() && part != ".")
                {
                    relative /= fs::path(std::u8string(part.begin(), part.end()));
                }
                start = end + 1;
            }
            if (relative.empty())
            {
                return std::nullopt;
            }
            return relative;
        }
    }

    bool ExtractZip(const fs::path& zip, const fs::path& folder, std::vector<std::string>& warnings, std::string& error)
    {
        std::ifstream file(zip, std::ios::binary);
        if (!file)
        {
            error = "the file cannot be opened";
            return false;
        }
        file.seekg(0, std::ios::end);
        const auto size = static_cast<std::uint64_t>(file.tellg());

        // The end-of-directory record is the last thing in the file, followed only by an optional comment
        const std::size_t tail =
            static_cast<std::size_t>(std::min<std::uint64_t>(size, kEndOfDirectorySize + kLongestComment));
        std::vector<char> end;
        if (tail < kEndOfDirectorySize || !ReadAt(file, size - tail, tail, end))
        {
            error = "not a zip file";
            return false;
        }
        std::size_t record = std::string::npos;
        for (std::size_t at = tail - kEndOfDirectorySize + 1; at-- > 0;)
        {
            if (U32(end, at) == kEndOfDirectory)
            {
                record = at;
                break;
            }
        }
        if (record == std::string::npos)
        {
            error = "not a zip file";
            return false;
        }
        const std::uint16_t entries = U16(end, record + 10);
        const std::uint32_t directorySize = U32(end, record + 12);
        const std::uint32_t directoryOffset = U32(end, record + 16);
        if (entries == 0xFFFF || directorySize == 0xFFFFFFFF || directoryOffset == 0xFFFFFFFF)
        {
            error = "ZIP64 archives (over 4 GB or 65535 files) are not supported; unpack it and import the folder";
            return false;
        }
        std::vector<char> directory;
        if (!ReadAt(file, directoryOffset, directorySize, directory))
        {
            error = "the archive is damaged (its file list is cut short)";
            return false;
        }

        std::error_code ec;
        fs::create_directories(folder, ec);
        std::size_t at = 0;
        for (std::uint16_t i = 0; i < entries; ++i)
        {
            if (at + 46 > directory.size() || U32(directory, at) != kDirectoryEntry)
            {
                error = "the archive is damaged (its file list is broken)";
                return false;
            }
            const std::uint16_t flags = U16(directory, at + 8);
            const std::uint16_t method = U16(directory, at + 10);
            const std::uint32_t packedSize = U32(directory, at + 20);
            const std::uint32_t fullSize = U32(directory, at + 24);
            const std::uint16_t nameLength = U16(directory, at + 28);
            const std::uint16_t extraLength = U16(directory, at + 30);
            const std::uint16_t commentLength = U16(directory, at + 32);
            const std::uint32_t localOffset = U32(directory, at + 42);
            if (at + 46 + nameLength > directory.size())
            {
                error = "the archive is damaged (its file list is broken)";
                return false;
            }
            const std::string name(directory.data() + at + 46, nameLength);
            at += 46 + static_cast<std::size_t>(nameLength) + extraLength + commentLength;

            if (name.empty() || name.back() == '/' || name.back() == '\\')
            {
                continue; // a folder; folders are made as their files are written
            }
            if (name.rfind("__MACOSX/", 0) == 0)
            {
                continue; // macOS's resource-fork copies, not real files
            }
            const auto relative = SafeRelative(name);
            if (!relative)
            {
                warnings.push_back("zip entry skipped (it would land outside the folder): " + name);
                continue;
            }
            if ((flags & 1u) != 0)
            {
                warnings.push_back("zip entry skipped (it is encrypted): " + name);
                continue;
            }
            if (method != 0 && method != 8)
            {
                warnings.push_back("zip entry skipped (compression method " + std::to_string(method) +
                                   " is not supported): " + name);
                continue;
            }
            if (fullSize > static_cast<std::uint32_t>(INT_MAX) || packedSize > static_cast<std::uint32_t>(INT_MAX))
            {
                warnings.push_back("zip entry skipped (too large): " + name);
                continue;
            }

            // The local header repeats the name and has its own extra field; the data starts after both
            std::vector<char> local;
            if (!ReadAt(file, localOffset, 30, local) || U32(local, 0) != kLocalHeader)
            {
                warnings.push_back("zip entry skipped (damaged): " + name);
                continue;
            }
            const std::uint64_t dataOffset =
                static_cast<std::uint64_t>(localOffset) + 30 + U16(local, 26) + U16(local, 28);
            std::vector<char> packed;
            if (!ReadAt(file, dataOffset, packedSize, packed))
            {
                warnings.push_back("zip entry skipped (cut short): " + name);
                continue;
            }
            std::vector<char> data;
            if (method == 0)
            {
                data = std::move(packed);
            }
            else if (fullSize > 0)
            {
                data.resize(fullSize);
                const int written = stbi_zlib_decode_noheader_buffer(data.data(), static_cast<int>(fullSize),
                                                                     packed.data(), static_cast<int>(packedSize));
                if (written != static_cast<int>(fullSize))
                {
                    warnings.push_back("zip entry skipped (it does not unpack): " + name);
                    continue;
                }
            }

            const fs::path target = folder / *relative;
            fs::create_directories(target.parent_path(), ec);
            std::ofstream out(target, std::ios::binary);
            out.write(data.data(), static_cast<std::streamsize>(data.size()));
            if (!out)
            {
                warnings.push_back("zip entry could not be written: " + name);
            }
        }
        return true;
    }
}
