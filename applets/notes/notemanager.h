/*
 * SPDX-FileCopyrightText: 2014 David Edmundson <david@davidedmundson.co.uk>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#pragma once
#include "abstractnoteloader.h"
#include <QObject>
#include <QSharedPointer>
class Note;

// Retains the upstream shared filesystem backend. TabModel owns returned notes.
class NoteManager : public QObject
{
    Q_OBJECT
public:
    explicit NoteManager(QObject *parent = nullptr);
    Note *loadNote(const QString &id);
    Note *createNote(const QString &text = { });
    Note *copyNote(const QString &id);
    QStringList allNoteIds();

private:
    static QSharedPointer<AbstractNoteLoader> loadBackend();
    QSharedPointer<AbstractNoteLoader> m_backend;
};
