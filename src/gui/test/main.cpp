/*  InputLeap -- mouse and keyboard sharing utility
    Copyright (C) 2021 Povilas Kanapickas <povilas@radix.lt>

    This package is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    found in the file LICENSE that should have accompanied this file.

    This package is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "arch/Arch.h"
#include "base/Log.h"
#if SYSAPI_WIN32
#include "arch/win32/ArchMiscWindows.h"
#endif

#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    // the configuration tests run the server's parser, as the settings window does
#if SYSAPI_WIN32
    inputleap::ArchMiscWindows::setInstanceWin32(GetModuleHandle(nullptr));
#endif
    inputleap::Arch arch;
    arch.init();
    inputleap::Log log;

    testing::InitGoogleTest(&argc, argv);
    return (RUN_ALL_TESTS() == 1) ? 1 : 0;
}
