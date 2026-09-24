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

#include "platform/MSWindowsSessionUser.h"

#include "base/Log.h"

#include <wtsapi32.h>

namespace inputleap {

HANDLE session_user_token()
{
    // WTSQueryUserToken needs a privilege only SYSTEM has, so it fails when
    // this runs as the user
    DWORD session = 0;
    HANDLE token = nullptr;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session) ||
        !WTSQueryUserToken(session, &token)) {
        return nullptr;
    }
    return token;
}

ImpersonateSessionUser::ImpersonateSessionUser()
{
    HANDLE token = session_user_token();
    if (token != nullptr) {
        impersonating_ = ImpersonateLoggedOnUser(token) != FALSE;
        if (!impersonating_) {
            LOG_WARN("couldn't act as the signed-in user: %d", GetLastError());
            failed_ = true;
        }
        CloseHandle(token);
    }
}

ImpersonateSessionUser::~ImpersonateSessionUser()
{
    if (impersonating_) {
        RevertToSelf();
    }
}

} // namespace inputleap
