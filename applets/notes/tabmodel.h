// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "note.h"
#include "notemanager.h"
#include <Plasma/Applet>
#include <QAbstractListModel>
#include <QPointer>
#include <QTimer>
#include <qqmlregistration.h>

class TabModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Plasma::Applet *applet READ applet WRITE setApplet NOTIFY appletChanged)
    Q_PROPERTY(QString activeId READ activeId NOTIFY activeIdChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeIdChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    enum Role { NoteIdRole = Qt::UserRole + 1, TitleRole, VisitedRole };
    explicit TabModel(QObject *parent = nullptr);
    ~TabModel() override;
    int rowCount(const QModelIndex &parent = { }) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Plasma::Applet *applet() const { return m_applet; }
    void setApplet(Plasma::Applet *applet);
    QString activeId() const { return m_activeId; }
    int activeIndex() const { return indexOf(m_activeId); }
    bool ready() const { return m_ready; }
    QString error() const { return m_error; }
    Q_INVOKABLE void initialize();
    Q_INVOKABLE Note *noteForId(const QString &id);
    Q_INVOKABLE QVariantMap viewState(const QString &id) const;
    Q_INVOKABLE void setViewState(const QString &id, int cursor, double x, double y);
    Q_INVOKABLE bool activate(const QString &id);
    Q_INVOKABLE bool next(int direction);
    Q_INVOKABLE bool addTab();
    Q_INVOKABLE bool renameTab(const QString &id, const QString &title);
    Q_INVOKABLE bool hasText(const QString &id);
    Q_INVOKABLE bool closeTab(const QString &id);
    Q_INVOKABLE bool flushAll();
    Q_INVOKABLE QVariantList recoverableNotes();
    Q_INVOKABLE bool recoverNote(const QString &id);
Q_SIGNALS:
    void appletChanged();
    void activeIdChanged();
    void countChanged();
    void readyChanged();
    void errorChanged();

private:
    struct Tab {
        QString id;
        QString title;
        int cursor = 0;
        double x = 0;
        double y = 0;
        bool visited = false;
        Note *note = nullptr;
    };
    int indexOf(const QString &id) const;
    bool persist();
    bool flushTab(const QString &id);
    bool appendNote(Note *note, const QString &title);
    void fail(const QString &message);
    QString owner() const;
    QPointer<Plasma::Applet> m_applet;
    NoteManager m_manager;
    QList<Tab> m_tabs;
    QString m_activeId;
    QString m_error;
    bool m_ready = false;
    QTimer m_stateTimer;
};
