/* !
@file    	    FileDialog.cpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		06/11/2026
@brief
    Implements PickFiles with the Win32 Open dialog (GetOpenFileNameW), owned
    by the editor's window so it stays in front of it. Other platforms have no
    dialog: PickFiles returns nothing there.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/FileDialog.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <commdlg.h>

#ifdef _MSC_VER
#pragma comment(lib, "comdlg32.lib")
#endif
#endif

#include <cstddef>

namespace Editor
{
#ifdef _WIN32
    namespace
    {
        std::wstring Wide(std::string_view text)
        {
            if (text.empty())
            {
                return {};
            }
            const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
            std::wstring wide(static_cast<std::size_t>(size), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
            return wide;
        }

        // "Sounds\0*.wav;*.ogg\0All files\0*.*\0\0": the dialog's list of names and patterns, each ended by a null
        std::wstring FilterList(const std::vector<FileFilter>& filters)
        {
            std::wstring list;
            for (const FileFilter& filter : filters)
            {
                std::wstring patterns;
                for (const std::string_view extension : filter.extensions)
                {
                    patterns += (patterns.empty() ? L"*" : L";*") + Wide(extension);
                }
                list += Wide(filter.name) + L'\0' + patterns + L'\0';
            }
            list += std::wstring(L"All files") + L'\0' + L"*.*" + L'\0';
            return list; // c_str() adds the second null that ends the list
        }
    }

    bool CanPickFiles()
    {
        return true;
    }

    std::vector<std::filesystem::path> PickFiles(const std::string& title, const std::vector<FileFilter>& filters,
                                                 bool many)
    {
        // Room for many files at once: the folder, then each file's name, each ended by a null
        std::vector<wchar_t> buffer(64 * 1024, L'\0');
        const std::wstring filterList = FilterList(filters);
        const std::wstring wideTitle = Wide(title);

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetActiveWindow(); // the editor's: the dialog stays in front of it
        dialog.lpstrFilter = filterList.c_str();
        dialog.nFilterIndex = 1;
        dialog.lpstrFile = buffer.data();
        dialog.nMaxFile = static_cast<DWORD>(buffer.size());
        dialog.lpstrTitle = wideTitle.c_str();
        // NOCHANGEDIR: the editor finds its files relative to where it was started
        dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY |
                       (many ? OFN_ALLOWMULTISELECT : 0);
        if (!GetOpenFileNameW(&dialog))
        {
            return {}; // cancelled, or the dialog failed
        }

        // One file: its full path. Several: the folder, then each name, the list ended by an empty one
        std::vector<std::filesystem::path> files;
        const std::wstring first(buffer.data());
        const wchar_t* name = buffer.data() + first.size() + 1;
        if (*name == L'\0')
        {
            files.emplace_back(first);
            return files;
        }
        const std::filesystem::path folder(first);
        while (*name != L'\0')
        {
            const std::wstring each(name);
            files.push_back(folder / each);
            name += each.size() + 1;
        }
        return files;
    }
#else
    bool CanPickFiles()
    {
        return false;
    }

    std::vector<std::filesystem::path> PickFiles(const std::string&, const std::vector<FileFilter>&, bool)
    {
        return {};
    }
#endif
}
