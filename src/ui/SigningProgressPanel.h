// SPDX-License-Identifier: Apache-2.0
#ifndef SIGNINGPROGRESSPANEL_H
#define SIGNINGPROGRESSPANEL_H

#include <QDialog>

#include "core/SigningRequestModel.h"
#include "core/SigningRequestRunner.h"

class QLabel;
class QListWidget;
class QPushButton;

class SignatureManager;

// ── R26 send-for-signing P1 — the signing-progress surface ───────────────────
//
// Shown when a document with a signing-request sidecar is opened (and after a
// request is prepared). MODELESS and dismissible — the surface guides, it
// never blocks: closing it changes nothing on disk; the workflow resumes from
// the sidecar on the next open.
//
// Thin glue over the pure pieces: the text is built by the static
// buildStatusText (offscreen-testable); "Sign as this signer" only emits
// signRequested() — the CONTROLLER drives the real signing flow (real
// SignatureDialog → real engine) and calls refresh() afterwards.
class SigningProgressPanel : public QDialog
{
    Q_OBJECT
public:
    SigningProgressPanel(SignatureManager *signing, const QString &docPath,
                         QWidget *parent = nullptr);

    // Reload the model + verification report from disk and rebuild the text.
    // Returns false when the sidecar has become unreadable (the panel then
    // shows the refusal and disables signing — never a half-claimed state).
    bool refresh();

    // Pure status builder (offscreen pin target): current signer k of N, the
    // current signer's field + page, the advisory-order disclosure, and every
    // verification warning verbatim. `model` empty → "complete" state.
    static QString buildStatusText(const SigningRequestModel &model,
                                   const SigningRequestRunner::VerificationReport &report);

    SigningRequestModel model() const { return m_model; }

    // PGR-36 (D2 delta review 2026-09-23): the document this panel is bound
    // to. The controller consults it so a MODELESS panel left open across a
    // document switch can never execute a signing step against a DIFFERENT
    // document's sidecar than the request it displays (cross-document replay
    // through a stale surface).
    QString documentPath() const { return m_docPath; }

signals:
    // The current signer's step was requested (index of the unsigned entry).
    // The controller runs the real sign flow, persists the sidecar and calls
    // refresh(); the panel never signs by itself.
    void signRequested(int signerIndex);
    void verifyRequested();

private:
    void rebuild();

    SignatureManager *m_signing = nullptr;
    QString m_docPath;
    SigningRequestModel m_model;
    SigningRequestRunner::VerificationReport m_report;

    QLabel *m_title = nullptr;
    QListWidget *m_stateList = nullptr;
    QLabel *m_disclosure = nullptr;
    QPushButton *m_signBtn = nullptr;
};

#endif // SIGNINGPROGRESSPANEL_H
