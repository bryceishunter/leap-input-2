/*
 * Leapdesk KVM -- mouse and keyboard sharing utility
 * Copyright (C) 2026 Leapdesk KVM contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "platform/MSWindowsClipboardFilesConverter.h"

#include "inputleap/FileClip.h"
#include "common/win32/encoding_utilities.h"
#include "base/Log.h"

#include <shellapi.h>

namespace inputleap {

IClipboard::EFormat MSWindowsClipboardFilesConverter::getFormat() const
{
    return IClipboard::kFiles;
}

UINT MSWindowsClipboardFilesConverter::getWin32Format() const
{
    return CF_HDROP;
}

HANDLE MSWindowsClipboardFilesConverter::fromIClipboard(const std::string&) const
{
    // MSWindowsFilePaste offers these instead, since the files have to be
    // fetched from the screen that copied them when an application pastes
    return nullptr;
}

std::string MSWindowsClipboardFilesConverter::toIClipboard(HANDLE data) const
{
    FileClip clip;
    if (!make_file_clip(get_paths(data), clip)) {
        return {};
    }
    return clip.marshall();
}

std::vector<std::wstring> MSWindowsClipboardFilesConverter::get_paths(HANDLE drop)
{
    std::vector<std::wstring> paths;
    HDROP files = static_cast<HDROP>(drop);
    UINT count = DragQueryFileW(files, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; ++i) {
        UINT length = DragQueryFileW(files, i, nullptr, 0);
        if (length == 0) {
            return {};
        }
        std::wstring path(length, L'\0');
        if (DragQueryFileW(files, i, &path[0], length + 1) != length) {
            return {};
        }
        paths.push_back(path);
    }
    return paths;
}

bool MSWindowsClipboardFilesConverter::make_file_clip(const std::vector<std::wstring>& paths,
                                                      FileClip& clip)
{
    if (paths.empty() || paths.size() > FileClip::kMaxFiles) {
        return false;
    }

    clip.files.clear();
    std::vector<std::string> utf8_paths;
    for (const auto& path : paths) {
        WIN32_FILE_ATTRIBUTE_DATA info;
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info)) {
            LOG_DEBUG("copied file %s can't be read", win_wchar_to_utf8(path.c_str()).c_str());
            return false;
        }
        if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            LOG_DEBUG("copied folders can't be pasted on other screens");
            return false;
        }

        std::wstring::size_type slash = path.find_last_of(L"\\/");
        std::wstring name = slash == std::wstring::npos ? path : path.substr(slash + 1);

        FileClip::File file;
        file.name = win_wchar_to_utf8(name.c_str());
        file.size = (static_cast<std::uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
        file.modified = (static_cast<std::uint64_t>(info.ftLastWriteTime.dwHighDateTime) << 32) |
                        info.ftLastWriteTime.dwLowDateTime;
        clip.files.push_back(file);
        utf8_paths.push_back(win_wchar_to_utf8(path.c_str()));
    }
    clip.id = FileClip::make_id(utf8_paths, clip.files);
    return true;
}

} // namespace inputleap
