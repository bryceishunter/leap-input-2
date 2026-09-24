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

#pragma once

#include "platform/MSWindowsClipboard.h"
#include "inputleap/Fwd.h"

#include <string>
#include <vector>

namespace inputleap {

//! Convert files copied in Explorer (CF_HDROP) to a FileClip
/*!
Only describes the files; they are sent when another screen pastes them.
*/
class MSWindowsClipboardFilesConverter : public IMSWindowsClipboardConverter {
public:
    // IMSWindowsClipboardConverter overrides
    IClipboard::EFormat getFormat() const override;
    UINT getWin32Format() const override;
    HANDLE fromIClipboard(const std::string&) const override;
    std::string toIClipboard(HANDLE) const override;

    //! The paths in CF_HDROP data
    static std::vector<std::wstring> get_paths(HANDLE drop);

    //! Describes the files at \p paths in \p clip.  Returns false if any of
    //! them isn't a file that can be read, since only files can be sent.
    static bool make_file_clip(const std::vector<std::wstring>& paths, FileClip& clip);
};

} // namespace inputleap
