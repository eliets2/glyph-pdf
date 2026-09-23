// SPDX-License-Identifier: Apache-2.0
#ifndef SIGNINGREQUESTDIALOG_H
#define SIGNINGREQUESTDIALOG_H

#include <QDialog>
#include <QSet>
#include <QVector>

#include "core/SigningRequestModel.h"

class QLineEdit;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QListWidget;
class QLabel;

class SignatureManager;

// ── R26 send-for-signing P1 — "Prepare signing request…" dialog ──────────────
//
// The prepare flow: build the ordered signer list of a request and persist it
// as the per-document sidecar (<file>.signrequest.json). Each signer binds to
// EITHER an existing UNSIGNED signature field (picked from the document) OR a
// rect anchor — the anchor path creates a REAL signature field at that rect
// through the additive gp::SignatureFieldCreator seam (GlyphPDF had no
// signature-field creation before; the Form Builder "signature" button places
// a TEXT box — disclosed in the dialog, the ledger and the plan).
//
// The dialog is thin glue over the pure model/runner/creator pieces: the
// whole save path is a public method (saveRequest) tests drive without exec().
class SigningRequestDialog : public QDialog
{
    Q_OBJECT
public:
    // `signing` and `docPath` must be live/valid; the dialog reads the
    // document's signature fields and existing signatures once on entry.
    SigningRequestDialog(SignatureManager *signing, const QString &docPath,
                         QWidget *parent = nullptr);

    // The request this dialog builds (mirrors the in-editor signer list).
    SigningRequestModel model() const { return m_model; }

    // ── Programmatic editing API (the buttons call exactly these; tests use
    //    them directly instead of mouse-driving the dialog). ─────────────────
    void addSigner(const SigningRequestModel::Signer &signer);
    void removeSigner(int index);
    void moveSigner(int fromIndex, int toIndex); // reorder = advisory order

    // Save path: validates every entry, creates any anchored (new) fields as
    // ONE R01 transaction, then writes the sidecar with the prepared-bytes
    // SHA-256. Returns false with a user-presentable `err` — nothing is
    // written on refusal. Public so tests (and the OK button) share one path.
    bool saveRequest(QString *err);

    // Pure name-uniquifier: `prefix` + the first free numeric suffix whose
    // composed name is not in `taken`.
    static QString uniqueFieldName(const QStringList &taken, const QString &prefix);

    // Pure deviation disclosure: non-empty when `requestedOrder` (the request's
    // field order) differs from `documentFieldOrder` restricted to those fields.
    // The engine signs the FIRST unsigned field in document order, so an
    // out-of-order request is run honestly but fills fields in a different
    // sequence than the list shows — this must be disclosed at prepare time.
    static QString orderDeviationWarning(const QStringList &requestedOrder,
                                         const QStringList &documentFieldOrder);

signals:
    // Emitted after a successful saveRequest that created signature fields —
    // the document changed on disk, so the session must reload it.
    void requestSaved(bool fieldsCreated);

private:
    void refreshUi();
    void loadFromUi(int index);   // form → model entry
    void syncEntryList();

    SignatureManager *m_signing = nullptr;
    QString m_docPath;
    SigningRequestModel m_model;

    // Document inventory captured on entry
    QStringList m_documentFieldOrder;  // ALL signature fields, document order
    QSet<QString> m_signedFields;      // fields already carrying a signature

    QListWidget *m_signerList = nullptr;
    int m_prevRow = -1;                    // list row the form was last loaded from
    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_bindingCombo = nullptr;   // existing unsigned fields + "[new field]"
    QWidget *m_anchorWidget = nullptr;     // enabled for the "[new field]" role
    QSpinBox *m_pageSpin = nullptr;
    QDoubleSpinBox *m_xSpin = nullptr;
    QDoubleSpinBox *m_ySpin = nullptr;
    QDoubleSpinBox *m_wSpin = nullptr;
    QDoubleSpinBox *m_hSpin = nullptr;
    QLabel *m_disclosure = nullptr;        // advisory-order + engine-behavior text
};

#endif // SIGNINGREQUESTDIALOG_H
