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

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace inputleap {

//! The token of the user signed in to this process's session
/*!
Only when this runs as SYSTEM, as it does under the Leapdesk service; the
caller closes it.  Null otherwise, since this then runs as that user already.
*/
HANDLE session_user_token();

//! Acts as the user signed in to this session on this thread while it exists
/*!
Does nothing unless this runs as SYSTEM.  Work on files in the user's
folders is done this way, so that a link the user put there can't turn it
into something only SYSTEM may do.
*/
class ImpersonateSessionUser {
public:
    ImpersonateSessionUser();
    ~ImpersonateSessionUser();
    ImpersonateSessionUser(const ImpersonateSessionUser&) = delete;
    ImpersonateSessionUser& operator=(const ImpersonateSessionUser&) = delete;

    //! Whether this runs as SYSTEM but couldn't act as the user, in which
    //! case the work must not be done
    bool failed() const { return failed_; }

private:
    bool impersonating_ = false;
    bool failed_ = false;
};

} // namespace inputleap
