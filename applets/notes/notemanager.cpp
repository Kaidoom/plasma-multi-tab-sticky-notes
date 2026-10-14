/*
 * SPDX-FileCopyrightText: 2014 David Edmundson <david@davidedmundson.co.uk>
 * SPDX-FileCopyrightText: 2026 Plasma Tabbed Notes contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include "notemanager.h"

#include <QDebug>
#include <QMutex>
#include <QPointer>
#include <QQmlEngine>

#include "filesystemnoteloader.h"
#include "note.h"

NoteManager::NoteManager(QObject *parent)
    : QObject(parent)
    , m_backend(loadBackend())
{
}

namespace
{
QHash<QString, QPointer<Note>> liveNotes;
}

Note *NoteManager::loadNote(const QString &id)
{
    auto *note = m_backend->loadNote(id);
    note->setParent(this);
    liveNotes.insert(note->id(), note);
    connect(note, &QObject::destroyed, [id] {
        if (liveNotes.value(id).isNull())
            liveNotes.remove(id);
    });
    return note;
}

Note *NoteManager::createNote(const QString &text)
{
    auto *note = m_backend->createNote(text);
    if (note) {
        note->setParent(this);
        liveNotes.insert(note->id(), note);
        const auto id = note->id();
        connect(note, &QObject::destroyed, [id] {
            if (liveNotes.value(id).isNull())
                liveNotes.remove(id);
        });
    }
    return note;
}

Note *NoteManager::copyNote(const QString &id)
{
    // A live source may contain edits whose debounce has not yet fired.
    if (auto note = liveNotes.value(id); note && note->ready()) {
        return createNote(note->noteText());
    }
    std::unique_ptr<Note> source(m_backend->loadNote(id));
    return source->ready() ? createNote(source->noteText()) : nullptr;
}

QStringList NoteManager::allNoteIds() { return m_backend->allNoteIds(); }

QSharedPointer<AbstractNoteLoader> NoteManager::loadBackend()
{
    static QMutex mutex;
    static QWeakPointer<AbstractNoteLoader> s_backend;

    mutex.lock();
    QSharedPointer<AbstractNoteLoader> manager = s_backend.toStrongRef();
    if (manager.isNull()) {
        manager.reset(new FileSystemNoteLoader);
        s_backend = manager;
    }
    mutex.unlock();
    return manager;
}
