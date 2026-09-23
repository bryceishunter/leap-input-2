/*
 * Leapdesk KVM -- mouse and keyboard sharing utility
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

#include "ScreenLink.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;

// Edits one link: which edge of which screen, which stretch of that edge, and
// where on the other screen the cursor comes out.
class LinkDialog : public QDialog
{
    Q_OBJECT

public:
    LinkDialog(QWidget* parent, const QStringList& screenNames, const ScreenLink& link,
               bool isNew, bool hasLinkBack);

    ScreenLink link() const;
    // whether the caller should also add or update the link leading back
    bool includeLinkBack() const;

    void accept() override;

private:
    void updateLinkBackText();

    QComboBox* m_pComboSource;
    QComboBox* m_pComboSide;
    QDoubleSpinBox* m_pSpinSourceStart;
    QDoubleSpinBox* m_pSpinSourceEnd;
    QComboBox* m_pComboTarget;
    QDoubleSpinBox* m_pSpinTargetStart;
    QDoubleSpinBox* m_pSpinTargetEnd;
    QCheckBox* m_pCheckLinkBack;
    bool m_IsNew;
};
