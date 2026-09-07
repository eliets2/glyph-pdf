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

    // V05 (PARITY-BRANCH-REVIEW-2026-09-05): monotonically increasing identity
    // of the document's CONTENT. markDirty()/markReload() — the boundaries every
    // mutating command and successful edit path already call — advance it, so
    // OCR review sessions can capture "the revision I reviewed" and detect an
    // in-place mutation (page replace/reorder, text edit, redaction) that
    // leaves path and page count untouched. Ordinary page navigation never
    // reaches those boundaries and therefore never advances it.
    qint64 mutationRevision() const;

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
    qint64 m_mutationRevision = 0;
};
