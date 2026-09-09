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

    // ARC07 (TEAM-ARCHITECTURE-REVIEW-2026-09-07): the ONE read-only state.
    // Read-only used to live only on PdfViewerWidget (a cursor/tool-mode
    // gate), so controller actions that push commands or call engines
    // directly still mutated a read-only document. The session is the shared
    // state every controller already holds, so editability policy lives HERE;
    // the viewer mirrors it (readOnlyChanged → setReadOnly, wired by the
    // MainWindow) and the shell's shared gate (shell/EditPolicy.h) consults
    // it at every mutation boundary. Note: beginDocument() deliberately does
    // NOT reset this — the open boundary decides it explicitly per document
    // (expiry evaluation), exactly like the dirty baseline.
    bool isReadOnly() const { return m_readOnly; }

public slots:
    // A slot (not just a method) so callers/tests can reach it through the
    // metaobject; emits readOnlyChanged only on an actual change.
    void setReadOnly(bool readOnly);

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

    // G05 (P1, QUALITY-GATE-2026-09-09): recovery identity. A recovered
    // document has TWO paths: the session path() is the ORIGINAL (the intended
    // save DESTINATION), while the editing inputs (engine/viewer) hold the
    // recovery copy `<original>.autosave.pdf` (the recovery INPUT). Save must
    // commit the recovered content to the DESTINATION and only then clear the
    // binding — a Save that merely rewrote the recovery input left the original
    // byte-identical while the session stayed dirty.
    QString recoverySource() const;
    void setRecoverySource(const QString &autosaveInputPath);
    void clearRecoverySource();

    QDateTime lastAutosave() const;
    void setLastAutosave(const QDateTime &time);

    static QStringList findOrphanedAutosaves(const QStringList &recentFiles);

signals:
    void reloadRequested();
    void dirtyChanged(bool dirty);
    void lastAutosaveChanged(const QDateTime &time);
    // ARC07: the session's editability changed — consumers (viewer tool gate,
    // action enablement) re-sync from this one signal.
    void readOnlyChanged(bool readOnly);
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
    bool m_readOnly = false;
    QDateTime m_lastAutosave;
    qint64 m_mutationRevision = 0;
    qint64 m_documentGeneration = 0;
    QString m_recoverySource;
};
