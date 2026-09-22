# InputLeap -- mouse and keyboard sharing utility
# Copyright (C) 2012-2016 Symless Ltd.
# Copyright (C) 2012 Nick Bolton
#
# This package is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# found in the file LICENSE that should have accompanied this file.
#
# This package is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


set(sources leapdesk-daemon.cpp)
if (WIN32)
    list(APPEND sources leapdesk-daemon.rc)
    set_property(SOURCE leapdesk-daemon.rc PROPERTY COMPILE_FLAGS /nologo)
endif()

add_executable (leapdesk-daemon WIN32 ${sources})

target_link_libraries (leapdesk-daemon
    arch base common io ipc mt net platform synlib ${libs} ${OPENSSL_LIBS})

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    install(TARGETS leapdesk-daemon DESTINATION ${INPUTLEAP_BUNDLE_BINARY_DIR})
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    install (TARGETS leapdesk-daemon DESTINATION bin)
else()
    install (TARGETS leapdesk-daemon DESTINATION .)
endif()
