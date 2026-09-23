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

#include <QCoreApplication>
#include <QString>

// One entry of the links section: leaving `source` through part of its `side`
// edge enters `target` through part of the opposite edge. Ranges are percentages
// of the edge length, measured left to right or top to bottom, as in the file.
struct ScreenLink
{
    enum class Side { Left, Right, Up, Down };

    QString source;
    Side side = Side::Right;
    double sourceStart = 0;
    double sourceEnd = 100;
    QString target;
    double targetStart = 0;
    double targetEnd = 100;

    bool coversWholeSourceEdge() const { return sourceStart == 0 && sourceEnd == 100; }
    bool coversWholeTargetEdge() const { return targetStart == 0 && targetEnd == 100; }

    // the link that leads back from target to source over the same stretch of edge
    ScreenLink reversed() const
    {
        ScreenLink back;
        back.source = target;
        back.side = opposite(side);
        back.sourceStart = targetStart;
        back.sourceEnd = targetEnd;
        back.target = source;
        back.targetStart = sourceStart;
        back.targetEnd = sourceEnd;
        return back;
    }

    static Side opposite(Side side)
    {
        switch (side) {
            case Side::Left: return Side::Right;
            case Side::Right: return Side::Left;
            case Side::Up: return Side::Down;
            case Side::Down: return Side::Up;
        }
        return side;
    }

    // how the settings window names an edge
    static QString sideLabel(Side side)
    {
        switch (side) {
            case Side::Left: return QCoreApplication::translate("ScreenLink", "left");
            case Side::Right: return QCoreApplication::translate("ScreenLink", "right");
            case Side::Up: return QCoreApplication::translate("ScreenLink", "top");
            case Side::Down: return QCoreApplication::translate("ScreenLink", "bottom");
        }
        return QString();
    }

    // how the configuration file names an edge
    static const char* sideName(Side side)
    {
        switch (side) {
            case Side::Left: return "left";
            case Side::Right: return "right";
            case Side::Up: return "up";
            case Side::Down: return "down";
        }
        return "";
    }

    bool operator==(const ScreenLink& other) const
    {
        return source == other.source && side == other.side &&
               sourceStart == other.sourceStart && sourceEnd == other.sourceEnd &&
               target == other.target &&
               targetStart == other.targetStart && targetEnd == other.targetEnd;
    }
    bool operator!=(const ScreenLink& other) const { return !(*this == other); }
};
