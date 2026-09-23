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

#include "Hotkey.h"

#include <QSettings>

Hotkey::Hotkey() :
    m_KeySequence(),
    m_Actions()
{
}

QString Hotkey::text() const
{
    QString text = m_KeySequence.toString();

    if (m_KeySequence.isMouseButton())
        return "mousebutton(" + text + ")";

    return "keystroke(" + text + ")";
}

void Hotkey::loadSettings(QSettings& settings)
{
    m_KeySequence.loadSettings(settings);

    m_Actions.clear();
    int num = settings.beginReadArray("actions");
    for (int i = 0; i < num; i++)
    {
        settings.setArrayIndex(i);
        Action a;
        a.loadSettings(settings);
        m_Actions.push_back(a);
    }

    settings.endArray();
}

void Hotkey::saveSettings(QSettings& settings) const
{
    m_KeySequence.saveSettings(settings);

    settings.beginWriteArray("actions");
    for (std::size_t i = 0; i < m_Actions.size(); i++)
    {
        settings.setArrayIndex(static_cast<int>(i));
        m_Actions[i].saveSettings(settings);
    }
    settings.endArray();
}

QTextStream& operator<<(QTextStream& outStream, const Hotkey& hotkey)
{
    // Don't write config if there are no actions
    if (hotkey.actions().size() == 0) {
        return outStream;
    }

    outStream << "\t" << hotkey.text() << " = ";
    for (std::size_t i = 0; i < hotkey.actions().size(); i++) {
        outStream << hotkey.actions()[i];
        if (i != hotkey.actions().size() - 1) {
            outStream << ", ";
        }
    }

    outStream << "\n";

    return outStream;
}

// splits at separators that are not inside parentheses
static QStringList splitTopLevel(const QString& text, QChar separator)
{
    QStringList parts;
    int depth = 0;
    int start = 0;
    for (int i = 0; i < text.size(); i++) {
        if (text[i] == QLatin1Char('('))
            depth++;
        else if (text[i] == QLatin1Char(')'))
            depth--;
        else if (text[i] == separator && depth == 0) {
            parts.append(text.mid(start, i - start));
            start = i + 1;
        }
    }
    parts.append(text.mid(start));
    return parts;
}

bool Hotkey::fromText(const QString& text, Hotkey& hotkey)
{
    int equals = text.indexOf(QLatin1Char('='));
    if (equals < 0)
        return false;

    QString condition = text.left(equals).trimmed();
    bool mouseButton = false;
    QString keys;
    if (condition.startsWith(QLatin1String("keystroke(")) && condition.endsWith(QLatin1Char(')'))) {
        keys = condition.mid(10, condition.size() - 11);
    } else if (condition.startsWith(QLatin1String("mousebutton(")) && condition.endsWith(QLatin1Char(')'))) {
        keys = condition.mid(12, condition.size() - 13);
        mouseButton = true;
    } else {
        return false;
    }

    Hotkey result;
    if (!KeySequence::fromString(keys.trimmed(), mouseButton, result.m_KeySequence))
        return false;

    QStringList phases = splitTopLevel(text.mid(equals + 1), QLatin1Char(';'));
    if (phases.size() > 2)
        return false;

    for (int phase = 0; phase < phases.size(); phase++) {
        if (phases[phase].trimmed().isEmpty())
            continue;
        for (const QString& actionText : splitTopLevel(phases[phase], QLatin1Char(','))) {
            Action action;
            if (!Action::fromText(actionText, phase == 1, action))
                return false;
            result.appendAction(action);
        }
    }

    if (result.actions().empty())
        return false;

    hotkey = result;
    return true;
}
