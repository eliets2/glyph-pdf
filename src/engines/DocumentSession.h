// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QStringList>

class DocumentSession : public QObject {
    Q_OBJECT
public:
    explicit DocumentSession(QObject *parent = nullptr);

    QString path() const;
    void setPath(const QString &path);
    void markReload();
    bool isDirty() const;
    void setClean();
    void markDirty();

    QDateTime lastAutosave() const;
    void setLastAutosave(const QDateTime &time);

    static QStringList findOrphanedAutosaves(const QStringList &recentFiles);

signals:
    void reloadRequested();
    void dirtyChanged(bool dirty);
    void lastAutosaveChanged(const QDateTime &time);

private:
    QString m_path;
    bool m_dirty = false;
    QDateTime m_lastAutosave;
};
