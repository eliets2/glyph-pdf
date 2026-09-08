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

    // ARC01 (TEAM-ARCHITECTURE-REVIEW-2026-09-07): publish a NEW document
    // identity at the one open/switch boundary. Unlike setPath() (a defensive
    // path re-sync, a no-op for the same path) this unconditionally mints a new
    // identity: clears dirty state, resets the autosave stamp and advances
    // documentGeneration() — including for a same-path reopen (A→A), which is
    // a NEW revision of the document with no inherited history.
    void beginDocument(const QString &path);

    // ARC01: identity of the OPEN document — advances once per successful
    // beginDocument() (open / switch / same-path reopen). Content revisions
    // *within* an open document are tracked by mutationRevision().
    qint64 documentGeneration() const;

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
    // Step-3 history truthfulness (EC03/EC05/V02, TEAM-ENGINE-CODE-REVIEW-
    // 2026-09-07): a command whose initial mutation or restoration FAILED
    // reports it here, so the shell can surface what the user's action actually
    // did. A failed mutation never changes the document; this is the honest
    // counterpart of markReload() (which is only ever called after a mutation
    // that really happened).
    void mutationFailed(const QString &reason);

private:
    QString m_path;
    bool m_dirty = false;
    QDateTime m_lastAutosave;
    qint64 m_mutationRevision = 0;
    qint64 m_documentGeneration = 0;
};
