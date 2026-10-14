/*
 * SPDX-FileCopyrightText: 2014 David Edmundson <david@davidedmundson.co.uk>
 * SPDX-FileCopyrightText: 2026 Plasma Tabbed Notes contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "filesystemnoteloader.h"
#include "note.h"
#include <KDirWatch>
#include <KLocalizedString>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

class FileNote : public Note
{
public:
    FileNote(const QString &path, const QString &id)
        : Note(id)
        , m_path(path)
    {
        if (!path.isEmpty()) {
            m_watcher.addFile(path);
        }
        const auto changed = [this](const QString &p) {
            if (p == m_path)
                load();
        };
        connect(&m_watcher, &KDirWatch::created, this, changed);
        connect(&m_watcher, &KDirWatch::dirty, this, changed);
        connect(&m_watcher, &KDirWatch::deleted, this, changed);
        load();
    }

    bool flush() override
    {
        if (!ready())
            return false;
        if (!dirty())
            return true;
        // Do not overwrite an externally changed or removed file with a stale
        // editor.
        QFile existing(m_path);
        if (QFileInfo(m_path).isSymLink() || !existing.open(QIODevice::ReadOnly) || existing.readAll() != m_diskText
            || existing.error() != QFileDevice::NoError) {
            setError(i18n("The note file changed outside this widget. Your edits are "
                          "still here. Copy them before resolving the file conflict."));
            return false;
        }
        existing.close();
        const QByteArray bytes = noteText().toUtf8();
        QSaveFile file(m_path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            setError(i18n("Could not save the note: %1", file.errorString()));
            return false;
        }
        m_diskText = bytes;
        saved();
        return true;
    }

private:
    void load()
    {
        if (m_path.isEmpty() || QFileInfo(m_path).isSymLink()) {
            setError(i18n("The note filename is invalid or is a symbolic link."));
            return;
        }
        QFile file(m_path);
        if (!file.open(QIODevice::ReadOnly)) {
            setError(i18n("Could not read the note: %1", file.errorString()));
            return;
        }
        const auto bytes = file.readAll();
        if (file.error() != QFileDevice::NoError) {
            setError(i18n("Could not read the note: %1", file.errorString()));
            return;
        }
        if (dirty()) {
            if (bytes != m_diskText)
                setError(i18n("The note file changed outside this widget. Your unsaved "
                              "edits are preserved."));
            return;
        }
        m_diskText = bytes;
        loaded(QString::fromUtf8(bytes));
    }
    QString m_path;
    QByteArray m_diskText;
    KDirWatch m_watcher;
};

FileSystemNoteLoader::FileSystemNoteLoader()
{
    m_notesDir.setPath(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/plasma_tabbed_notes"));
    QDir().mkpath(m_notesDir.absolutePath());
}

bool FileSystemNoteLoader::validId(const QString &id)
{
    // Also accept the truncated UUIDs written by the upstream single-note
    // baseline.
    static const QRegularExpression re(QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]"
                                                      "{4}-[0-9a-fA-F]{10,12}$"));
    return re.match(id).hasMatch();
}

QStringList FileSystemNoteLoader::allNoteIds()
{
    QStringList ids;
    for (const auto &id : m_notesDir.entryList(QDir::Files | QDir::NoSymLinks, QDir::Time)) {
        if (validId(id))
            ids.append(id);
    }
    return ids;
}

Note *FileSystemNoteLoader::loadNote(const QString &id)
{
    // Invalid configuration must never escape the fork's data directory.
    return new FileNote(validId(id) ? m_notesDir.absoluteFilePath(id) : QString(), id);
}

Note *FileSystemNoteLoader::createNote(const QString &text)
{
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto path = m_notesDir.absoluteFilePath(id);
    const auto bytes = text.toUtf8();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return nullptr;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(bytes) != bytes.size() || !file.commit())
        return nullptr;
    return new FileNote(path, id);
}
