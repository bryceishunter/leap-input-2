/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2008 Volker Lanz (vl@fidra.de)
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

#include "ServerConfig.h"

#include <QDialog>
#include <memory>

namespace Ui
{
    class ServerConfigDialog;
}

class ServerConfigDialog : public QDialog
{
    Q_OBJECT

    public:
        ServerConfigDialog(QWidget* parent, ServerConfig& config, const QString& defaultScreenName);
        ~ServerConfigDialog() override;

    public slots:
        void accept() override;
        void message(const QString& message) { m_Message = message; }

    protected slots:
        void on_m_pButtonNewHotkey_clicked();
        void on_m_pListHotkeys_itemSelectionChanged();
        void on_m_pButtonEditHotkey_clicked();
        void on_m_pButtonRemoveHotkey_clicked();

        void on_m_pButtonNewAction_clicked();
        void on_m_pListActions_itemSelectionChanged();
        void on_m_pButtonEditAction_clicked();
        void on_m_pButtonRemoveAction_clicked();
        void on_m_pCheckBoxEnableClipboard_stateChanged(int state);

        void on_m_pButtonAddScreen_clicked();
        void on_m_pButtonEditScreen_clicked();
        void on_m_pButtonRemoveScreen_clicked();

        void on_m_pButtonAddLink_clicked();
        void on_m_pButtonEditLink_clicked();
        void on_m_pButtonRemoveLink_clicked();
        void on_m_pTableLinks_itemSelectionChanged();
        void on_m_pTableLinks_cellDoubleClicked();

    protected:
        ServerConfig& serverConfig() { return m_ServerConfig; }
        void setOrigServerConfig(const ServerConfig& s) { m_OrigServerConfig = s; }
        void showEvent(QShowEvent* event) override;

    private:
        void refreshScreens(const QString& select = QString());
        void refreshLinks(int select = -1);
        void updateLinkRows();
        void updateScreenButtons();
        QStringList screenNames() const;
        int selectedLinkRow() const;
        QString selectedScreenName() const;
        // puts link into the list, replacing the entry at index unless it is -1;
        // returns false, after telling the user, when it overlaps another link
        bool storeLink(const ScreenLink& link, int index);
        int findLink(const QString& source, ScreenLink::Side side, double start, double end) const;
        bool checkScreenName(const QString& name, const QString& currentName);

    private:
        std::unique_ptr<Ui::ServerConfigDialog> ui_;
        ServerConfig& m_OrigServerConfig;
        ServerConfig m_ServerConfig;
        QString m_Message;
};
