/* !
@file    	    RecycleBin.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		01/11/2026
@brief
    Implements MoveToRecycleBin.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/RecycleBin.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <shellapi.h>
#endif

#include <string>
#include <system_error>

namespace Editor
{
    bool MoveToRecycleBin(const std::filesystem::path& path, const std::filesystem::path& fallbackFolder,
                          std::string& error)
    {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
        {
            error = "it is not there any more";
            return false;
        }
#ifdef _WIN32
        // The shell's own delete, allowing undo: the Recycle Bin. pFrom is a list ending in an empty name.
        std::wstring from = std::filesystem::absolute(path, ec).wstring();
        from.push_back(L'\0');
        SHFILEOPSTRUCTW operation{};
        operation.wFunc = FO_DELETE;
        operation.pFrom = from.c_str();
        operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
        if (SHFileOperationW(&operation) == 0 && operation.fAnyOperationsAborted == FALSE &&
            !std::filesystem::exists(path, ec))
        {
            return true;
        }
#endif
        if (fallbackFolder.empty())
        {
            error = "there is no Recycle Bin";
            return false;
        }
        // No Recycle Bin: into the fallback folder, numbered if one of that name is already there
        std::filesystem::create_directories(fallbackFolder, ec);
        std::filesystem::path target = fallbackFolder / path.filename();
        for (int copy = 2; std::filesystem::exists(target, ec); ++copy)
        {
            target = fallbackFolder / (path.stem().string() + "_" + std::to_string(copy) + path.extension().string());
        }
        std::filesystem::rename(path, target, ec);
        if (ec)
        {
            error = ec.message();
            return false;
        }
        return true;
    }
}
