// SPDX-FileCopyrightText: 2026 Plasma Tabbed Notes contributors
// SPDX-License-Identifier: BSD-2-Clause
#include "filesystemnoteloader.h"
#include "note.h"
#include "notemanager.h"
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <csignal>
#include <sys/resource.h>
using namespace Qt::StringLiterals;

class StorageTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void atomicFailurePreservesOriginalAndDraft()
    {
        FileSystemNoteLoader loader;
        std::unique_ptr<Note> note(loader.createNote(u"original"_s));
        QVERIFY(note && note->ready());
        note->edit(QString(65536, QLatin1Char('x')));
        const QString path = qEnvironmentVariable("XDG_DATA_HOME") + u"/plasma_tabbed_notes/"_s + note->id();
        struct rlimit oldLimit;
        QVERIFY(getrlimit(RLIMIT_FSIZE, &oldLimit) == 0);
        struct rlimit smallLimit = oldLimit;
        smallLimit.rlim_cur = 128;
        auto oldSignal = std::signal(SIGXFSZ, SIG_IGN);
        QVERIFY(setrlimit(RLIMIT_FSIZE, &smallLimit) == 0);
        const bool saved = note->flush();
        const int reset = setrlimit(RLIMIT_FSIZE, &oldLimit);
        std::signal(SIGXFSZ, oldSignal);
        QVERIFY(reset == 0);
        QVERIFY(!saved);
        QVERIFY(note->dirty());
        QCOMPARE(note->noteText().size(), 65536);
        QVERIFY(!note->error().isEmpty());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("original"));
        file.close();
        QVERIFY(note->flush());
        QVERIFY(!note->dirty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll().size(), 65536);
    }
    void externalConflictAndMissingFiles()
    {
        FileSystemNoteLoader loader;
        std::unique_ptr<Note> note(loader.createNote(u"original"_s));
        QVERIFY(note);
        note->edit(u"unsaved local edit"_s);
        QFile file(qEnvironmentVariable("XDG_DATA_HOME") + u"/plasma_tabbed_notes/"_s + note->id());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("external edit");
        file.close();
        QVERIFY(!note->flush());
        QCOMPARE(note->noteText(), u"unsaved local edit"_s);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("external edit"));
        file.close();
        QVERIFY(file.remove());
        QVERIFY(!note->flush());
        QVERIFY(note->dirty());
        std::unique_ptr<Note> missing(loader.loadNote(u"00000000-0000-0000-0000-000000000000"_s));
        QVERIFY(!missing->ready());
        missing->edit(u"must not overwrite"_s);
        QVERIFY(!missing->flush());
        QVERIFY(missing->noteText().isEmpty());
        std::unique_ptr<Note> invalid(loader.loadNote(u"../plasma_notes/some-note"_s));
        QVERIFY(!invalid->ready());
    }
    void managerCopiesPendingEditsAndRetainsFiles()
    {
        QString id;
        {
            NoteManager manager;
            auto *note = manager.createNote(u"saved"_s);
            QVERIFY(note);
            id = note->id();
            note->edit(u"draft Ω 日本語"_s);
            auto *copy = manager.copyNote(id);
            QVERIFY(copy);
            QVERIFY(copy->id() != id);
            QCOMPARE(copy->noteText(), u"draft Ω 日本語"_s);
            QVERIFY(!copy->dirty());
            QVERIFY(note->dirty());
            QVERIFY(!manager.copyNote(u"00000000-0000-0000-0000-000000000000"_s));
            QVERIFY(manager.allNoteIds().contains(id));
        }
        QFile original(qEnvironmentVariable("XDG_DATA_HOME") + u"/plasma_tabbed_notes/"_s + id);
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), QByteArray("saved"));
    }
};
int main(int argc, char **argv)
{
    QTemporaryDir dir;
    qputenv("XDG_DATA_HOME", dir.path().toUtf8());
    QCoreApplication app(argc, argv);
    StorageTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "storage.moc"
