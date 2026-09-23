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

#include "Screen.h"
#include "ScreenLink.h"

#include <QMap>
#include <QRectF>
#include <QWidget>

#include <vector>

// Draws the screens where their links put them and lets the stretch of an edge
// that a link covers be changed by dragging its ends.
//
// The configuration holds links, not positions, so the layout is worked out
// from the links: the server screen goes first, and each linked screen is placed
// against the stretch it is linked on, sized so the two stretches line up.
class ScreenLayoutView : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenLayoutView(QWidget* parent = nullptr);

    // the view edits links in place; call refresh() after changing either list
    void setConfiguration(const std::vector<Screen>* screens, std::vector<ScreenLink>* links,
                          const QString& serverName);
    void refresh();

    QString selectedScreen() const { return m_SelectedScreen; }
    int selectedLink() const { return m_SelectedLink; }
    void selectScreen(const QString& name);
    void selectLink(int index);

    QSize sizeHint() const override { return QSize(520, 280); }
    QSize minimumSizeHint() const override { return QSize(240, 180); }

signals:
    void screenSelected(const QString& name);
    void screenActivated(const QString& name);
    void linkSelected(int index);
    void linkActivated(int index);
    // a drag changed the stretch of the link at index (and of its way back)
    void linkEdited(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    bool event(QEvent* event) override;

private:
    enum class Handle { None, Start, End };

    void layoutScreens();
    QRectF toWidget(const QRectF& units) const;
    // the stretch of the link, drawn just inside the screen it leaves from
    QLineF sourceBand(const ScreenLink& link) const;
    // the stretch it arrives on, drawn just inside the screen it enters
    QLineF targetBand(const ScreenLink& link) const;
    QLineF edgeInWidget(const QString& screen, ScreenLink::Side side) const;
    int linkAt(const QPointF& pos) const;
    Handle handleAt(const QPointF& pos) const;
    QString screenAt(const QPointF& pos) const;
    QColor linkColor(int index) const;
    int reverseOf(int index) const;
    QString describe(const ScreenLink& link) const;

    const std::vector<Screen>* m_pScreens = nullptr;
    std::vector<ScreenLink>* m_pLinks = nullptr;
    QString m_ServerName;

    QMap<QString, QRectF> m_Layout;  // in layout units
    QRectF m_Bounds;
    double m_Scale = 1;
    QPointF m_Offset;

    QString m_SelectedScreen;
    int m_SelectedLink = -1;

    Handle m_DragHandle = Handle::None;
    int m_DragReverse = -1;
};
