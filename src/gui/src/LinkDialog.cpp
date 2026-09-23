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

#include "LinkDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

static QDoubleSpinBox* percentSpinBox(double value, QWidget* parent)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(0, 100);
    spin->setDecimals(2);
    spin->setSingleStep(1);
    spin->setSuffix(QStringLiteral(" %"));
    spin->setValue(value);
    return spin;
}

static QWidget* rangeRow(QDoubleSpinBox* start, QDoubleSpinBox* end, QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(start);
    layout->addWidget(new QLabel(QObject::tr("to"), row));
    layout->addWidget(end);
    layout->addStretch();
    return row;
}

LinkDialog::LinkDialog(QWidget* parent, const QStringList& screenNames, const ScreenLink& link,
                       bool isNew, bool hasLinkBack) :
    QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
    m_IsNew(isNew)
{
    setWindowTitle(isNew ? tr("Add link") : tr("Edit link"));

    m_pComboSource = new QComboBox(this);
    m_pComboSource->addItems(screenNames);
    m_pComboSource->setCurrentText(link.source);

    m_pComboSide = new QComboBox(this);
    m_pComboSide->addItem(tr("Left edge"), static_cast<int>(ScreenLink::Side::Left));
    m_pComboSide->addItem(tr("Right edge"), static_cast<int>(ScreenLink::Side::Right));
    m_pComboSide->addItem(tr("Top edge"), static_cast<int>(ScreenLink::Side::Up));
    m_pComboSide->addItem(tr("Bottom edge"), static_cast<int>(ScreenLink::Side::Down));
    m_pComboSide->setCurrentIndex(m_pComboSide->findData(static_cast<int>(link.side)));

    m_pSpinSourceStart = percentSpinBox(link.sourceStart, this);
    m_pSpinSourceEnd = percentSpinBox(link.sourceEnd, this);

    m_pComboTarget = new QComboBox(this);
    m_pComboTarget->addItems(screenNames);
    m_pComboTarget->setCurrentText(link.target);

    m_pSpinTargetStart = percentSpinBox(link.targetStart, this);
    m_pSpinTargetEnd = percentSpinBox(link.targetEnd, this);

    m_pCheckLinkBack = new QCheckBox(this);
    m_pCheckLinkBack->setChecked(isNew || hasLinkBack);

    auto* form = new QFormLayout;
    form->addRow(tr("Leave &screen:"), m_pComboSource);
    form->addRow(tr("Through its:"), m_pComboSide);
    form->addRow(tr("Along the stretch:"), rangeRow(m_pSpinSourceStart, m_pSpinSourceEnd, this));
    form->addRow(tr("Enter s&creen:"), m_pComboTarget);
    form->addRow(tr("Along the stretch:"), rangeRow(m_pSpinTargetStart, m_pSpinTargetEnd, this));

    auto* hint = new QLabel(tr("Stretches are percentages along the edge, left to right or top "
                               "to bottom; 0 to 100 is the whole edge. The cursor comes out on "
                               "the opposite edge of the screen it enters."), this);
    hint->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &LinkDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &LinkDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(hint);
    layout->addWidget(m_pCheckLinkBack);
    layout->addWidget(buttons);

    connect(m_pComboSource, &QComboBox::currentTextChanged, this, &LinkDialog::updateLinkBackText);
    connect(m_pComboTarget, &QComboBox::currentTextChanged, this, &LinkDialog::updateLinkBackText);
    connect(m_pComboSide, &QComboBox::currentIndexChanged, this, &LinkDialog::updateLinkBackText);
    updateLinkBackText();
}

ScreenLink LinkDialog::link() const
{
    ScreenLink result;
    result.source = m_pComboSource->currentText();
    result.side = static_cast<ScreenLink::Side>(m_pComboSide->currentData().toInt());
    result.sourceStart = m_pSpinSourceStart->value();
    result.sourceEnd = m_pSpinSourceEnd->value();
    result.target = m_pComboTarget->currentText();
    result.targetStart = m_pSpinTargetStart->value();
    result.targetEnd = m_pSpinTargetEnd->value();
    return result;
}

bool LinkDialog::includeLinkBack() const
{
    return m_pCheckLinkBack->isChecked();
}

void LinkDialog::updateLinkBackText()
{
    ScreenLink back = link().reversed();
    QString text = m_IsNew ? tr("Also add the way back: %1 edge of %2 leads to %3")
                           : tr("Also update the way back: %1 edge of %2 leads to %3");
    m_pCheckLinkBack->setText(text.arg(ScreenLink::sideLabel(back.side),
                                       back.source, back.target));
}

void LinkDialog::accept()
{
    ScreenLink result = link();
    if (result.sourceStart >= result.sourceEnd || result.targetStart >= result.targetEnd) {
        QMessageBox::warning(this, tr("Invalid stretch"),
                             tr("Each stretch has to start before it ends."));
        return;
    }
    QDialog::accept();
}
