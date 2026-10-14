// SPDX-License-Identifier: GPL-2.0-or-later
#include "tabmodel.h"
#include "filesystemnoteloader.h"
#include <KConfigGroup>
#include <KConfigLoader>
#include <KConfigPropertyMap>
#include <KLocalizedString>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTextDocument>
#include <cmath>

namespace
{
QHash<QString, TabModel *> owners;
}

TabModel::TabModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_stateTimer.setSingleShot(true);
    m_stateTimer.setInterval(1000);
    connect(&m_stateTimer, &QTimer::timeout, this, &TabModel::persist);
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
        flushAll();
        persist();
    });
}

TabModel::~TabModel()
{
    flushAll();
    for (const auto &tab : std::as_const(m_tabs)) {
        if (owners.value(tab.id) == this)
            owners.remove(tab.id);
    }
}

int TabModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_tabs.size(); }
QHash<int, QByteArray> TabModel::roleNames() const
{
    return { { NoteIdRole, "noteId" }, { TitleRole, "title" }, { VisitedRole, "visited" } };
}
QVariant TabModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tabs.size())
        return { };
    const auto &tab = m_tabs.at(index.row());
    switch (role) {
    case NoteIdRole:
        return tab.id;
    case TitleRole:
        return tab.title;
    case VisitedRole:
        return tab.visited;
    default:
        return { };
    }
}

void TabModel::setApplet(Plasma::Applet *applet)
{
    if (m_applet || !applet)
        return;
    m_applet = applet;
    connect(applet, &Plasma::Applet::destroyedChanged, this, [this](bool removed) {
        if (removed)
            flushAll();
    });
    Q_EMIT appletChanged();
}

QString TabModel::owner() const { return m_applet->config().config()->name() + QLatin1Char(':') + QString::number(m_applet->id()); }

int TabModel::indexOf(const QString &id) const
{
    for (int i = 0; i < m_tabs.size(); ++i)
        if (m_tabs.at(i).id == id)
            return i;
    return -1;
}

void TabModel::fail(const QString &message)
{
    m_error = message;
    Q_EMIT errorChanged();
}

void TabModel::initialize()
{
    if (m_ready || !m_applet || !m_tabs.isEmpty())
        return;
    const auto config = m_applet->configuration();
    const QString state = config->value(QStringLiteral("tabState")).toString();
    QList<Tab> tabs;
    QString active;
    bool copy = false;
    if (!state.isEmpty()) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(state.toUtf8(), &parseError);
        const auto object = document.object();
        const auto array = object.value(QStringLiteral("tabs")).toArray();
        if (parseError.error != QJsonParseError::NoError || object.value(QStringLiteral("version")).toInt() != 1 || array.isEmpty()) {
            fail(i18n("The saved tab configuration could not be read. It has been "
                      "left unchanged; your note files are still available."));
            return;
        }
        QSet<QString> ids;
        for (const auto &value : array) {
            const auto entry = value.toObject();
            const QString id = entry.value(QStringLiteral("id")).toString();
            if (!FileSystemNoteLoader::validId(id) || ids.contains(id) || !entry.value(QStringLiteral("title")).isString()) {
                fail(i18n("The saved tab configuration contains invalid or repeated "
                          "note IDs. It has been left unchanged."));
                return;
            }
            ids.insert(id);
            Tab tab;
            tab.id = id;
            tab.title = entry.value(QStringLiteral("title")).toString();
            tab.cursor = qMax(0, entry.value(QStringLiteral("cursor")).toInt());
            tab.x = entry.value(QStringLiteral("x")).toDouble();
            tab.y = entry.value(QStringLiteral("y")).toDouble();
            tabs.append(tab);
            copy |= owners.contains(id);
        }
        active = object.value(QStringLiteral("active")).toString();
        copy |= object.value(QStringLiteral("owner")).toString() != owner();
    } else {
        const QString legacyId = config->value(QStringLiteral("noteId")).toString();
        if (!legacyId.isEmpty()) {
            if (!FileSystemNoteLoader::validId(legacyId)) {
                fail(i18n("The saved note ID is invalid. The configuration has been "
                          "left unchanged."));
                return;
            }
            Tab tab;
            tab.id = legacyId;
            tab.title = i18n("Note 1");
            tab.cursor = qMax(0, config->value(QStringLiteral("cursorPosition")).toInt());
            tab.x = config->value(QStringLiteral("scrollX")).toDouble();
            tab.y = config->value(QStringLiteral("scrollY")).toDouble();
            tabs.append(tab);
            // Migration also copies: an old single-note configuration may itself be a
            // clone.
            copy = true;
        } else {
            auto *note = m_manager.createNote();
            if (!note) {
                fail(i18n("Could not create a note file. Check the note storage directory."));
                return;
            }
            Tab tab;
            tab.id = note->id();
            tab.title = i18n("Note 1");
            tab.note = note;
            tabs.append(tab);
        }
    }
    if (copy) {
        // Copy every note before publishing any new IDs, including unopened tabs.
        for (auto &tab : tabs) {
            auto *note = m_manager.copyNote(tab.id);
            if (!note) {
                fail(i18n("Could not copy all notes for this widget. The saved "
                          "configuration has been left unchanged."));
                return;
            }
            if (active == tab.id)
                active = note->id();
            tab.id = note->id();
            tab.note = note;
        }
    }
    beginResetModel();
    m_tabs = tabs;
    m_activeId = indexOf(active) >= 0 ? active : m_tabs.first().id;
    m_tabs[indexOf(m_activeId)].visited = true;
    for (const auto &tab : std::as_const(m_tabs))
        owners.insert(tab.id, this);
    endResetModel();
    m_ready = persist();
    Q_EMIT countChanged();
    Q_EMIT activeIdChanged();
    Q_EMIT readyChanged();
}

Note *TabModel::noteForId(const QString &id)
{
    const int i = indexOf(id);
    if (i < 0)
        return nullptr;
    if (!m_tabs[i].note)
        m_tabs[i].note = m_manager.loadNote(id);
    return m_tabs[i].note;
}

QVariantMap TabModel::viewState(const QString &id) const
{
    const int i = indexOf(id);
    if (i < 0)
        return { };
    const auto &t = m_tabs.at(i);
    return { { QStringLiteral("cursor"), t.cursor }, { QStringLiteral("x"), t.x }, { QStringLiteral("y"), t.y } };
}

void TabModel::setViewState(const QString &id, int cursor, double x, double y)
{
    const int i = indexOf(id);
    if (i < 0 || !m_ready || !std::isfinite(x) || !std::isfinite(y))
        return;
    auto &t = m_tabs[i];
    if (t.cursor == cursor && t.x == x && t.y == y)
        return;
    t.cursor = qMax(0, cursor);
    t.x = x;
    t.y = y;
    m_stateTimer.start();
}

bool TabModel::persist()
{
    if (!m_applet || m_tabs.isEmpty())
        return false;
    m_stateTimer.stop();
    QJsonArray tabs;
    for (const auto &tab : std::as_const(m_tabs)) {
        tabs.append(QJsonObject { { QStringLiteral("id"), tab.id }, { QStringLiteral("title"), tab.title },
            { QStringLiteral("cursor"), tab.cursor }, { QStringLiteral("x"), tab.x }, { QStringLiteral("y"), tab.y } });
    }
    const QJsonObject state { { QStringLiteral("version"), 1 }, { QStringLiteral("owner"), owner() },
        { QStringLiteral("active"), m_activeId }, { QStringLiteral("tabs"), tabs } };
    const auto text = QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
    auto *scheme = m_applet->configScheme();
    auto *entry = scheme->findItemByName(QStringLiteral("tabState"));
    if (!entry) {
        fail(i18n("The tab configuration schema is unavailable."));
        return false;
    }
    entry->setProperty(text);
    m_applet->configuration()->insert(QStringLiteral("tabState"), text);
    if (!scheme->save()) {
        fail(i18n("Could not save the tab configuration. Your notes are still "
                  "available."));
        return false;
    }
    if (!m_error.isEmpty())
        fail(QString());
    return true;
}

bool TabModel::flushTab(const QString &id)
{
    const int i = indexOf(id);
    if (i < 0 || !m_tabs.at(i).note || !m_tabs.at(i).note->dirty())
        return true;
    if (m_tabs.at(i).note->flush())
        return true;
    fail(m_tabs.at(i).note->error());
    return false;
}

bool TabModel::flushAll()
{
    bool ok = true;
    for (const auto &tab : std::as_const(m_tabs))
        ok = flushTab(tab.id) && ok;
    return ok;
}

bool TabModel::activate(const QString &id)
{
    const int i = indexOf(id);
    if (!m_ready || i < 0)
        return false;
    if (id == m_activeId)
        return true;
    if (!flushTab(m_activeId))
        return false;
    m_activeId = id;
    m_tabs[i].visited = true;
    Q_EMIT dataChanged(index(i), index(i), { VisitedRole });
    Q_EMIT activeIdChanged();
    return persist();
}

bool TabModel::next(int direction)
{
    if (m_tabs.isEmpty())
        return false;
    return activate(m_tabs.at((indexOf(m_activeId) + (direction < 0 ? m_tabs.size() - 1 : 1)) % m_tabs.size()).id);
}

bool TabModel::appendNote(Note *note, const QString &title)
{
    if (!note) {
        fail(i18n("Could not create the note file."));
        return false;
    }
    Tab tab;
    tab.id = note->id();
    tab.title = title;
    tab.note = note;
    tab.visited = true;
    beginInsertRows({ }, m_tabs.size(), m_tabs.size());
    m_tabs.append(tab);
    owners.insert(tab.id, this);
    endInsertRows();
    m_activeId = tab.id;
    Q_EMIT countChanged();
    Q_EMIT activeIdChanged();
    return persist();
}

bool TabModel::addTab()
{
    return m_ready && flushTab(m_activeId) && appendNote(m_manager.createNote(), i18n("Note %1", m_tabs.size() + 1));
}

bool TabModel::renameTab(const QString &id, const QString &title)
{
    const int i = indexOf(id);
    if (!m_ready || i < 0 || title.trimmed().isEmpty())
        return false;
    m_tabs[i].title = title.trimmed();
    Q_EMIT dataChanged(index(i), index(i), { TitleRole });
    return persist();
}

bool TabModel::hasText(const QString &id)
{
    // Inspect the stored document even when its editor has never been
    // instantiated.
    auto *note = noteForId(id);
    if (!note || !note->ready())
        return true;
    QTextDocument document;
    document.setHtml(note->noteText());
    return !document.isEmpty();
}

bool TabModel::closeTab(const QString &id)
{
    if (!m_ready || indexOf(id) < 0 || !flushTab(id))
        return false;
    if (m_tabs.size() == 1 && !addTab())
        return false;
    const int i = indexOf(id);
    const bool current = m_activeId == id;
    beginRemoveRows({ }, i, i);
    const Tab removed = m_tabs.takeAt(i);
    owners.remove(id);
    endRemoveRows();
    // Retain the file. Only the editor and its in-memory note object are
    // released.
    if (removed.note)
        removed.note->deleteLater();
    if (current) {
        const int next = qMin(i, int(m_tabs.size() - 1));
        m_activeId = m_tabs.at(next).id;
        m_tabs[next].visited = true;
        Q_EMIT dataChanged(index(next), index(next), { VisitedRole });
    }
    Q_EMIT countChanged();
    Q_EMIT activeIdChanged();
    return persist();
}

QVariantList TabModel::recoverableNotes()
{
    QVariantList result;
    FileSystemNoteLoader reader;
    for (const auto &id : m_manager.allNoteIds()) {
        if (owners.contains(id))
            continue;
        std::unique_ptr<Note> note(reader.loadNote(id));
        if (!note->ready())
            continue;
        QTextDocument document;
        document.setHtml(note->noteText());
        result.append(
            QVariantMap { { QStringLiteral("id"), id }, { QStringLiteral("preview"), document.toPlainText().simplified().left(80) } });
    }
    return result;
}

bool TabModel::recoverNote(const QString &id)
{
    if (!m_ready || !FileSystemNoteLoader::validId(id) || !flushTab(m_activeId))
        return false;
    return appendNote(m_manager.copyNote(id), i18n("Recovered Note"));
}
