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

#include "inputleap/FileClip.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace inputleap {

//! Sends files with Taildrop
/*!
Runs "tailscale file cp" for one file after another on a thread of its own,
and reports progress through a callback, from that thread.  One paste is
sent at a time.
*/
class MSWindowsTaildrop {
public:
    using Report = std::function<void(const FilePasteStatus&)>;

    explicit MSWindowsTaildrop(Report report);
    //! Stops a transfer in progress
    ~MSWindowsTaildrop();

    //! Sends the file at each of \p paths, under the name at the same index of
    //! \p names, to \p address for paste \p request.  Returns false, sending
    //! nothing, if a paste is still being sent.
    bool send(const std::string& request, const std::vector<std::wstring>& paths,
              const std::vector<std::string>& names, const std::string& address);

    //! The command line that makes \p program (tailscale.exe) send the file at
    //! \p path as \p name to \p address
    static std::wstring command_line(const std::wstring& program, const std::wstring& path,
                                     const std::string& name, const std::string& address);

    //! Quotes \p argument so that the program receives it unchanged
    static std::wstring quote_argument(const std::wstring& argument);

    //! Where tailscale.exe is installed, or empty if it isn't
    static std::wstring find_program();

    //! This machine's Tailscale IPv4 address, or empty if it has none
    static std::string own_address();

    //! The folder Taildrop saves the files it receives in: the signed-in
    //! user's Downloads
    static std::wstring received_folder();

private:
    // a running program and the read end of its output
    struct Child {
        HANDLE process = nullptr;
        HANDLE output = nullptr;
    };

    void run(const std::string& request, const std::vector<std::wstring>& paths,
             const std::vector<std::string>& names, const std::string& address);
    void report(const FilePasteStatus& status);

    // runs one "tailscale file cp", which the destructor can stop; returns
    // its exit code and output
    bool run_program(const std::wstring& program, const std::wstring& command,
                     DWORD& exit_code, std::string& output);

    // starts \p command with its output going to a pipe, as the signed-in
    // user if this runs as SYSTEM
    static bool start_program(const std::wstring& program, const std::wstring& command,
                              Child& child);
    // reads the output of \p child until it exits; leaves its process handle open
    static void finish_program(Child& child, DWORD& exit_code, std::string& output);

    Report report_;
    std::thread thread_;

    std::mutex mutex_;
    bool busy_ = false;
    bool stopping_ = false;
    HANDLE process_ = nullptr;     // the tailscale.exe running now, so it can be stopped
};

} // namespace inputleap
