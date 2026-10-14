/*
 * SPDX-FileCopyrightText: 2014 David Edmundson <david@davidedmundson.co.uk>
 * SPDX-FileCopyrightText: 2026 Plasma Tabbed Notes contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#pragma once

#include <QObject>
#include <qqmlregistration.h>

class Note : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Create through NoteManager")
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString noteText READ noteText NOTIFY noteTextChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
public:
    explicit Note(const QString &id);
    QString id() const { return m_id; }
    QString noteText() const { return m_text; }
    bool dirty() const { return m_dirty; }
    bool ready() const { return m_ready; }
    QString error() const { return m_error; }
    Q_INVOKABLE void edit(const QString &text);
    Q_INVOKABLE virtual bool flush() = 0;
Q_SIGNALS:
    void noteTextChanged();
    void stateChanged();

protected:
    void loaded(const QString &text);
    void saved();
    void setError(const QString &message);

private:
    const QString m_id;
    QString m_text;
    QString m_error;
    bool m_dirty = false;
    bool m_ready = false;
};
