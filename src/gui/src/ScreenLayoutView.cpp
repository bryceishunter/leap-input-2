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

#include "ScreenLayoutView.h"

#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <queue>

namespace {

// a screen is drawn 16 by 9 units unless a link says it is narrower or wider
const double kWidth = 16;
const double kHeight = 9;
const double kGap = 3;       // between screens that no link places
const double kMargin = 12;   // pixels around the drawing
const double kBandInset = 5; // pixels between an edge and the band drawn for it
const double kHitSlop = 7;   // pixels within which a band or handle can be grabbed

bool isHorizontal(ScreenLink::Side side)
{
    return side == ScreenLink::Side::Up || side == ScreenLink::Side::Down;
}

// the edge from left to right or top to bottom, like the stretches measure it
QLineF edgeOf(const QRectF& rect, ScreenLink::Side side)
{
    switch (side) {
        case ScreenLink::Side::Up: return QLineF(rect.topLeft(), rect.topRight());
        case ScreenLink::Side::Down: return QLineF(rect.bottomLeft(), rect.bottomRight());
        case ScreenLink::Side::Left: return QLineF(rect.topLeft(), rect.bottomLeft());
        case ScreenLink::Side::Right: return QLineF(rect.topRight(), rect.bottomRight());
    }
    return QLineF();
}

QPointF inwardOf(ScreenLink::Side side)
{
    switch (side) {
        case ScreenLink::Side::Up: return QPointF(0, 1);
        case ScreenLink::Side::Down: return QPointF(0, -1);
        case ScreenLink::Side::Left: return QPointF(1, 0);
        case ScreenLink::Side::Right: return QPointF(-1, 0);
    }
    return QPointF();
}

QLineF stretchOf(const QLineF& edge, double start, double end, ScreenLink::Side side)
{
    QLineF line(edge.pointAt(start / 100), edge.pointAt(end / 100));
    return line.translated(inwardOf(side) * kBandInset);
}

double distanceToSegment(const QPointF& p, const QLineF& line)
{
    QPointF d = line.p2() - line.p1();
    double length2 = d.x() * d.x() + d.y() * d.y();
    double t = length2 > 0 ? QPointF::dotProduct(p - line.p1(), d) / length2 : 0;
    t = std::clamp(t, 0.0, 1.0);
    QPointF nearest = line.p1() + d * t;
    return std::hypot(p.x() - nearest.x(), p.y() - nearest.y());
}

// where along the edge p is, as a percentage
double percentAlong(const QPointF& p, const QLineF& edge)
{
    QPointF d = edge.p2() - edge.p1();
    double length2 = d.x() * d.x() + d.y() * d.y();
    if (length2 <= 0)
        return 0;
    return std::clamp(QPointF::dotProduct(p - edge.p1(), d) / length2 * 100, 0.0, 100.0);
}

// Places `to` against the stretch of `from` that the link leaves through, sized
// so that the stretch it enters on has the same length.
QRectF placeAgainst(const QRectF& from, const ScreenLink& link)
{
    double sourceLength = (isHorizontal(link.side) ? from.width() : from.height()) *
                          (link.sourceEnd - link.sourceStart) / 100;
    double targetFraction = (link.targetEnd - link.targetStart) / 100;
    double base = isHorizontal(link.side) ? kWidth : kHeight;
    double length = std::clamp(sourceLength / targetFraction, base / 4, base * 4);
    double scale = length / base;
    QSizeF size(kWidth * scale, kHeight * scale);

    QLineF stretch(edgeOf(from, link.side).pointAt(link.sourceStart / 100),
                   edgeOf(from, link.side).pointAt(link.sourceEnd / 100));
    QRectF to(QPointF(), size);
    switch (link.side) {
        case ScreenLink::Side::Down:
            to.moveTopLeft(QPointF(stretch.p1().x() - link.targetStart / 100 * size.width(), from.bottom()));
            break;
        case ScreenLink::Side::Up:
            to.moveBottomLeft(QPointF(stretch.p1().x() - link.targetStart / 100 * size.width(), from.top()));
            break;
        case ScreenLink::Side::Right:
            to.moveTopLeft(QPointF(from.right(), stretch.p1().y() - link.targetStart / 100 * size.height()));
            break;
        case ScreenLink::Side::Left:
            to.moveTopRight(QPointF(from.left(), stretch.p1().y() - link.targetStart / 100 * size.height()));
            break;
    }
    return to;
}

} // namespace

ScreenLayoutView::ScreenLayoutView(QWidget* parent) :
    QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ScreenLayoutView::setConfiguration(const std::vector<Screen>* screens,
                                        std::vector<ScreenLink>* links, const QString& serverName)
{
    m_pScreens = screens;
    m_pLinks = links;
    m_ServerName = serverName;
    refresh();
}

void ScreenLayoutView::refresh()
{
    layoutScreens();
    if (m_pLinks && m_SelectedLink >= static_cast<int>(m_pLinks->size()))
        m_SelectedLink = -1;
    if (!m_Layout.contains(m_SelectedScreen))
        m_SelectedScreen.clear();
    update();
}

void ScreenLayoutView::selectScreen(const QString& name)
{
    m_SelectedScreen = m_Layout.contains(name) ? name : QString();
    m_SelectedLink = -1;
    update();
}

void ScreenLayoutView::selectLink(int index)
{
    m_SelectedLink = (m_pLinks && index >= 0 && index < static_cast<int>(m_pLinks->size())) ? index : -1;
    if (m_SelectedLink >= 0)
        m_SelectedScreen.clear();
    update();
}

void ScreenLayoutView::layoutScreens()
{
    m_Layout.clear();
    if (!m_pScreens || !m_pLinks)
        return;

    QStringList order;
    for (const Screen& screen : *m_pScreens) {
        if (screen.name().compare(m_ServerName, Qt::CaseInsensitive) == 0)
            order.prepend(screen.name());
        else
            order.append(screen.name());
    }

    // each group of linked screens is laid out from its first screen, to the
    // right of the groups before it
    double nextGroupX = 0;
    for (const QString& first : order) {
        if (m_Layout.contains(first))
            continue;

        QMap<QString, QRectF> group;
        group.insert(first, QRectF(0, 0, kWidth, kHeight));
        std::queue<QString> pending;
        pending.push(first);
        while (!pending.empty()) {
            QString current = pending.front();
            pending.pop();
            for (const ScreenLink& link : *m_pLinks) {
                ScreenLink outward;
                if (link.source == current)
                    outward = link;
                else if (link.target == current)
                    outward = link.reversed();
                else
                    continue;
                if (group.contains(outward.target) || m_Layout.contains(outward.target) ||
                    !order.contains(outward.target))
                    continue;
                group.insert(outward.target, placeAgainst(group[current], outward));
                pending.push(outward.target);
            }
        }

        QRectF bounds;
        for (const QRectF& rect : group)
            bounds = bounds.isNull() ? rect : bounds.united(rect);
        QPointF shift(nextGroupX - bounds.left(), -bounds.top());
        for (auto it = group.begin(); it != group.end(); ++it)
            m_Layout.insert(it.key(), it.value().translated(shift));
        nextGroupX += bounds.width() + kGap;
    }

    m_Bounds = QRectF();
    for (const QRectF& rect : m_Layout)
        m_Bounds = m_Bounds.isNull() ? rect : m_Bounds.united(rect);

    if (!m_Bounds.isEmpty()) {
        double available = std::max(1.0, width() - 2 * kMargin);
        double availableHeight = std::max(1.0, height() - 2 * kMargin);
        m_Scale = std::min(available / m_Bounds.width(), availableHeight / m_Bounds.height());
        QSizeF drawn = m_Bounds.size() * m_Scale;
        m_Offset = QPointF((width() - drawn.width()) / 2, (height() - drawn.height()) / 2) -
                   m_Bounds.topLeft() * m_Scale;
    }
}

QRectF ScreenLayoutView::toWidget(const QRectF& units) const
{
    return QRectF(units.topLeft() * m_Scale + m_Offset, units.size() * m_Scale);
}

QLineF ScreenLayoutView::edgeInWidget(const QString& screen, ScreenLink::Side side) const
{
    if (!m_Layout.contains(screen))
        return QLineF();
    return edgeOf(toWidget(m_Layout[screen]).adjusted(1, 1, -1, -1), side);
}

QLineF ScreenLayoutView::sourceBand(const ScreenLink& link) const
{
    QLineF edge = edgeInWidget(link.source, link.side);
    return edge.isNull() ? edge : stretchOf(edge, link.sourceStart, link.sourceEnd, link.side);
}

QLineF ScreenLayoutView::targetBand(const ScreenLink& link) const
{
    ScreenLink::Side side = ScreenLink::opposite(link.side);
    QLineF edge = edgeInWidget(link.target, side);
    return edge.isNull() ? edge : stretchOf(edge, link.targetStart, link.targetEnd, side);
}

int ScreenLayoutView::reverseOf(int index) const
{
    const ScreenLink back = (*m_pLinks)[index].reversed();
    for (int i = 0; i < static_cast<int>(m_pLinks->size()); i++) {
        if (i != index && (*m_pLinks)[i] == back)
            return i;
    }
    return -1;
}

QColor ScreenLayoutView::linkColor(int index) const
{
    // a link and its way back share a colour
    std::vector<int> groups(m_pLinks->size(), -1);
    int nextGroup = 0;
    for (int i = 0; i <= index; i++) {
        int back = reverseOf(i);
        groups[i] = (back >= 0 && back < i) ? groups[back] : nextGroup++;
    }
    static const int hues[] = { 205, 30, 140, 280, 350, 60, 170, 250 };
    return QColor::fromHsv(hues[groups[index] % std::size(hues)], 170, 215);
}

QString ScreenLayoutView::describe(const ScreenLink& link) const
{
    auto stretch = [](double start, double end) {
        if (start == 0 && end == 100)
            return tr("the whole edge");
        return tr("%1 to %2 %").arg(QString::number(start), QString::number(end));
    };
    return tr("Leaving %1 through %2 of its %3 edge enters %4 along %5 of its %6 edge")
        .arg(link.source, stretch(link.sourceStart, link.sourceEnd),
             ScreenLink::sideLabel(link.side), link.target,
             stretch(link.targetStart, link.targetEnd),
             ScreenLink::sideLabel(ScreenLink::opposite(link.side)));
}

void ScreenLayoutView::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPalette& pal = palette();

    painter.fillRect(rect(), pal.color(QPalette::Base));
    if (!m_pScreens || m_Layout.isEmpty()) {
        painter.setPen(pal.color(QPalette::PlaceholderText));
        painter.drawText(rect(), Qt::AlignCenter, tr("No screens yet. Add one to start."));
        return;
    }

    // screens
    for (auto it = m_Layout.begin(); it != m_Layout.end(); ++it) {
        QRectF r = toWidget(it.value()).adjusted(1, 1, -1, -1);
        bool isServer = it.key().compare(m_ServerName, Qt::CaseInsensitive) == 0;
        bool selected = it.key() == m_SelectedScreen;

        QPainterPath path;
        path.addRoundedRect(r, 4, 4);
        painter.fillPath(path, pal.color(isServer ? QPalette::AlternateBase : QPalette::Window));
        QPen outline(selected ? pal.color(QPalette::Highlight) : pal.color(QPalette::Mid),
                     selected ? 2.5 : 1);
        painter.setPen(outline);
        painter.drawPath(path);

        QString label = isServer ? tr("%1\n(this computer)").arg(it.key()) : it.key();
        painter.setPen(pal.color(QPalette::Text));
        QFont font = painter.font();
        font.setBold(isServer);
        painter.setFont(font);
        painter.drawText(r.adjusted(10, 10, -10, -10), Qt::AlignCenter | Qt::TextWordWrap, label);
    }

    // where each link arrives, drawn thin and first so that the bands a link
    // leaves through stay on top
    for (int i = 0; i < static_cast<int>(m_pLinks->size()); i++) {
        QLineF arrival = targetBand((*m_pLinks)[i]);
        if (arrival.isNull())
            continue;
        QColor color = i == m_SelectedLink ? pal.color(QPalette::Highlight) : linkColor(i);
        color.setAlpha(i == m_SelectedLink ? 200 : 120);
        painter.setPen(QPen(color, 2, Qt::DashLine, Qt::RoundCap));
        painter.drawLine(arrival);
    }

    // links: a band inside each screen along the stretch it covers
    for (int i = 0; i < static_cast<int>(m_pLinks->size()); i++) {
        const ScreenLink& link = (*m_pLinks)[i];
        bool selected = i == m_SelectedLink;
        QColor color = selected ? pal.color(QPalette::Highlight) : linkColor(i);

        QLineF band = sourceBand(link);
        if (band.isNull())
            continue;
        painter.setPen(QPen(color, selected ? 7 : 5, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(band);

        // an arrow at the middle, pointing out of the edge the cursor leaves through
        QPointF mid = band.pointAt(0.5);
        QPointF out = -inwardOf(link.side);
        QPointF across(out.y(), -out.x());
        QPolygonF arrow({mid + out * 1, mid - out * 9 + across * 6, mid - out * 9 - across * 6});
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPolygon(arrow.translated(-out * 6));
    }

    // handles and figures for the selected link
    if (m_SelectedLink >= 0) {
        const ScreenLink& link = (*m_pLinks)[m_SelectedLink];
        QLineF band = sourceBand(link);
        if (!band.isNull()) {
            painter.setPen(QPen(pal.color(QPalette::Base), 2));
            painter.setBrush(pal.color(QPalette::Highlight));
            for (const QPointF& end : {band.p1(), band.p2()})
                painter.drawEllipse(end, 6, 6);

            painter.setPen(pal.color(QPalette::Text));
            QPointF in = inwardOf(link.side) * 18;
            QRectF box(0, 0, 70, 16);
            box.moveCenter(band.p1() + in);
            painter.drawText(box, Qt::AlignCenter, QString::number(link.sourceStart) + QStringLiteral(" %"));
            box.moveCenter(band.p2() + in);
            painter.drawText(box, Qt::AlignCenter, QString::number(link.sourceEnd) + QStringLiteral(" %"));
        }
    }
}

int ScreenLayoutView::linkAt(const QPointF& pos) const
{
    if (!m_pLinks)
        return -1;
    int best = -1;
    double bestDistance = kHitSlop;
    for (int i = 0; i < static_cast<int>(m_pLinks->size()); i++) {
        QLineF band = sourceBand((*m_pLinks)[i]);
        if (band.isNull())
            continue;
        double distance = distanceToSegment(pos, band);
        if (distance <= bestDistance) {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

ScreenLayoutView::Handle ScreenLayoutView::handleAt(const QPointF& pos) const
{
    if (m_SelectedLink < 0)
        return Handle::None;
    QLineF band = sourceBand((*m_pLinks)[m_SelectedLink]);
    if (band.isNull())
        return Handle::None;
    if (QLineF(pos, band.p1()).length() <= kHitSlop + 2)
        return Handle::Start;
    if (QLineF(pos, band.p2()).length() <= kHitSlop + 2)
        return Handle::End;
    return Handle::None;
}

QString ScreenLayoutView::screenAt(const QPointF& pos) const
{
    for (auto it = m_Layout.begin(); it != m_Layout.end(); ++it) {
        if (toWidget(it.value()).contains(pos))
            return it.key();
    }
    return QString();
}

void ScreenLayoutView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    QPointF pos = event->position();

    m_DragHandle = handleAt(pos);
    if (m_DragHandle != Handle::None) {
        m_DragReverse = reverseOf(m_SelectedLink);
        return;
    }

    int link = linkAt(pos);
    if (link >= 0) {
        selectLink(link);
        emit linkSelected(link);
        return;
    }

    selectScreen(screenAt(pos));
    emit screenSelected(m_SelectedScreen);
}

void ScreenLayoutView::mouseMoveEvent(QMouseEvent* event)
{
    QPointF pos = event->position();
    if (m_DragHandle == Handle::None) {
        bool overHandle = handleAt(pos) != Handle::None;
        bool overLink = linkAt(pos) >= 0;
        if (overHandle) {
            ScreenLink::Side side = (*m_pLinks)[m_SelectedLink].side;
            setCursor(isHorizontal(side) ? Qt::SizeHorCursor : Qt::SizeVerCursor);
        } else {
            setCursor(overLink ? Qt::PointingHandCursor : Qt::ArrowCursor);
        }
        return;
    }

    // the layout stays put while dragging, so the edge doesn't move under the cursor
    ScreenLink& link = (*m_pLinks)[m_SelectedLink];
    QLineF edge = edgeInWidget(link.source, link.side);
    double percent = percentAlong(pos, edge);

    // whole percents unless Shift is held; the edge ends snap
    if (!(event->modifiers() & Qt::ShiftModifier))
        percent = std::round(percent);
    else
        percent = std::round(percent * 100) / 100;
    if (percent < 1)
        percent = 0;
    if (percent > 99)
        percent = 100;

    ScreenLink before = link;
    const double kMinimum = 1;
    if (m_DragHandle == Handle::Start)
        link.sourceStart = std::min(percent, link.sourceEnd - kMinimum);
    else
        link.sourceEnd = std::max(percent, link.sourceStart + kMinimum);

    if (link != before) {
        // keep the way back covering the same stretch
        if (m_DragReverse >= 0) {
            ScreenLink& back = (*m_pLinks)[m_DragReverse];
            back.targetStart = link.sourceStart;
            back.targetEnd = link.sourceEnd;
        }
        emit linkEdited(m_SelectedLink);
        update();
    }
    QToolTip::showText(event->globalPosition().toPoint(), describe(link), this);
}

void ScreenLayoutView::mouseReleaseEvent(QMouseEvent*)
{
    if (m_DragHandle == Handle::None)
        return;
    m_DragHandle = Handle::None;
    m_DragReverse = -1;
    layoutScreens();
    update();
}

void ScreenLayoutView::mouseDoubleClickEvent(QMouseEvent* event)
{
    QPointF pos = event->position();
    int link = linkAt(pos);
    if (link >= 0) {
        emit linkActivated(link);
        return;
    }
    QString screen = screenAt(pos);
    if (!screen.isEmpty())
        emit screenActivated(screen);
}

bool ScreenLayoutView::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(event);
        int link = linkAt(help->pos());
        if (link >= 0)
            QToolTip::showText(help->globalPos(), describe((*m_pLinks)[link]), this);
        else
            QToolTip::hideText();
        return true;
    }
    if (event->type() == QEvent::Resize) {
        bool result = QWidget::event(event);
        layoutScreens();
        return result;
    }
    return QWidget::event(event);
}
