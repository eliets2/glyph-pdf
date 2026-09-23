// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QMutex>

struct FormJsFailure; // core/interfaces/IFormManager.h

namespace gp {

// R18(a): the PERSISTENT stale-calculated-field disclosure.
//
// The transaction policy (R01 + R05/JS-01) keeps a failed calculated field's
// committed /V and names every calculated field an aborted cascade never
// reached ("skipped"). A transient dialog is not enough: the disclosure must
// SURVIVE until the field recomputes or the user acknowledges it. This tracker
// is the store behind that persistent warning; it outlives the dialogs and the
// properties panel (AppContext lifetime) and is keyed by document path, so the
// warning follows the document across panel rebuilds and in-session document
// switches.
//
// SCOPE OF "persistent" (r18-review F3, 2026-09-13 — decision, not an
// accident): the warnings persist WITHIN THE SESSION — across recomputes,
// field switches, panel rebuilds and in-session document switches — NOT
// across application restarts. The tracker is deliberately in-memory only
// (no QSettings, no file): a stale flag describes the relationship between a
// stored value and the fields it is calculated from, which the next session
// re-derives from the document itself on its first cascade. A fresh tracker
// instance starts clean; there is no durable/shared store behind it.
//
// Update rule (per document): a cascade outcome REPLACES the stale set.
//   * every calculated field NAMED in the cascade's failures is stale — it
//     kept its committed value while its inputs moved (timeout / memory /
//     syntax / exception / rejected / engine), or the aborted cascade never
//     reached it ("skipped");
//   * every field NOT named was recomputed in the author's dependency order
//     (or is no longer cascade-reachable) — its warning clears. In particular
//     a fully-successful cascade clears every warning for the document.
// An operation that did not commit (failed transaction) must NOT call this —
// nothing was recomputed, so the previous state stays truthful.
class FormStaleFieldTracker {
public:
    // Records the outcome of one calculate cascade committed to `docPath`.
    void applyCascadeOutcome(const QString& docPath,
                             const QList<FormJsFailure>& failures);

    bool isStale(const QString& docPath, const QString& fieldName) const;
    QString staleReason(const QString& docPath, const QString& fieldName) const;
    QStringList staleFields(const QString& docPath) const;

    // The user acknowledged the warning for one field (it stays flagged in the
    // cascade report / on disk — only the persistent warning is dismissed).
    void acknowledge(const QString& docPath, const QString& fieldName);

    void clearDocument(const QString& docPath);

private:
    struct Entry {
        QString kind;
        QString reason;
    };
    mutable QMutex m_mutex;
    QMap<QString, QMap<QString, Entry>> m_byDoc;
};

} // namespace gp
