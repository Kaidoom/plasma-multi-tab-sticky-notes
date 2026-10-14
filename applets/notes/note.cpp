/*
 * SPDX-FileCopyrightText: 2014 David Edmundson <david@davidedmundson.co.uk>
 * SPDX-FileCopyrightText: 2026 Plasma Tabbed Notes contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include "note.h"

Note::Note(const QString &id)
    : m_id(id)
{
}

void Note::edit(const QString &text)
{
    if (!m_ready || text == m_text)
        return;
    m_text = text;
    m_dirty = true;
    Q_EMIT stateChanged();
    Q_EMIT noteTextChanged();
}

void Note::loaded(const QString &text)
{
    const bool changed = text != m_text;
    m_text = text;
    m_dirty = false;
    m_ready = true;
    m_error.clear();
    Q_EMIT stateChanged();
    if (changed)
        Q_EMIT noteTextChanged();
}

void Note::saved()
{
    m_dirty = false;
    m_error.clear();
    Q_EMIT stateChanged();
}

void Note::setError(const QString &message)
{
    m_error = message;
    Q_EMIT stateChanged();
}
