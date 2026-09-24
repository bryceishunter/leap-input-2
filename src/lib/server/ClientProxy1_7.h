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

#include "server/ClientProxy1_6.h"

namespace inputleap {

//! Proxy for client implementing protocol version 1.7
/*!
Adds pasting files that were copied on another screen.
*/
class ClientProxy1_7 : public ClientProxy1_6 {
public:
    ClientProxy1_7(const std::string& name, std::unique_ptr<IClientConnection> backend,
                   Server* server, IEventQueue* events);
    ~ClientProxy1_7() override;

    // BaseClientProxy overrides
    bool supports_file_paste() const override { return true; }
    void send_files(const FilePasteRequest& request) override;
    void file_paste_status(const FilePasteStatus& status) override;

protected:
    bool parseMessage(const std::uint8_t* code) override;

private:
    bool recv_file_paste();
    bool recv_file_status();
};

} // namespace inputleap
