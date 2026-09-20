// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QImage>

#include "core/SigningRequestModel.h"
#include "core/interfaces/ISignatureManager.h"

class SignatureManager;

// ── R26 send-for-signing P1 — the fill-flow orchestrator ─────────────────────
//
// Drives the EXISTING public signing engine (SignatureManager::signDocument… —
// the same seam SecurityController::runSigning drives) for ONE step of a
// signing request, and enforces the workflow's honesty gates around it:
//
//   1. MUTATION GATE — the step refuses to run while the document bytes differ
//      from preparedSha256, unless the USER re-confirmed the changed bytes via
//      the controller's dialog — an authorization that travels OUT OF BAND in
//      FillStepInput::userReconfirmedSha256, never from the unsigned sidecar
//      (SWEEP-W1 F3: a sidecar-written reconfirm must not skip the user)
//      (the DocumentSession
//      mutationRevision lineage is session-local; the sidecar must gate across
//      sessions, so the SHA-256 of the prepared bytes is the identity).
//   2. BINDING GATE — the bound field must exist as an UNSIGNED signature
//      field before the step; the engine signs the first unsigned signature
//      field in document order, so a step that binds a field which is not
//      reachable-first is still RUN (order is advisory) and then recorded
//      with `fieldMatch=false` — what actually happened is what the sidecar
//      stores. Binding an ALREADY-SIGNED field is refused outright.
//   3. ATTAINED-LEVEL HONESTY — the sidecar records the label derived from
//      the engine's OWN outcome detail (SecurityController::attainedLevelLabel,
//      R19c) — a requested level is never claimed when it degraded.
//   4. COVERAGE HONESTY — after signing, the step validates the signed bytes
//      and records the validation outcome of the field that actually received
//      the signature (a ByteRange-covered signature the engine's
//      validateSignatures reports). A signature is never claimed that the
//      engine did not produce and validate.
//
// GUI-free by construction: the controller wraps runFillStep in a worker
// thread; tests call it synchronously with the real fixtures.
class SigningRequestRunner {
public:
    /// Inputs for ONE fill step (one signer, one real signing operation).
    struct FillStepInput {
        QString docPath;
        SigningRequestModel model;  // snapshot BEFORE the step
        int signerIndex = -1;       // the entry to sign (any unsigned entry)
        QString certPath;           // P12/PFX chosen through SignatureDialog
        QString password;
        QString reason;
        QString location;
        QImage appearance;          // optional visible-signature image
        PAdESLevel requestedLevel = PAdESLevel::B_B; // signing config at step time
        QString tsaUrl;             // signing config at step time
        // SWEEP-W1 F3: the user's re-confirm authorization, OUT OF BAND. The
        // sidecar is unsigned JSON — a `reconfirmedSha256` written by anyone
        // with file access must never stand in for the user's decision, so
        // the mutation gate honors the re-confirm ONLY from this field, which
        // the controller sets solely after its Yes/No dialog (the sidecar
        // copy remains as a display/record value and never gates).
        QString userReconfirmedSha256;
    };

    /// WHY a step refuses before any engine call (checked in this order).
    enum class StepRefusal {
        None = 0,
        NoDocument,        // docPath missing/unreadable
        Complete,          // every entry is already signed
        BadIndex,          // signerIndex out of range
        AlreadySigned,     // the chosen entry claims signed=true
        DocumentChanged,   // bytes differ from prepared AND reconfirmed hashes
        MissingField,      // bound field does not exist and no anchor to create it
        FieldAlreadySigned, // the bound field already carries a signature
        FieldCreateFailed, // the step's own anchored field could not be created
        ForeignUnsignedField // W1-03: an unsigned field NO entry binds survives
                             // on the document — the engine's one-unsigned-field
                             // precondition can never be met for this request;
                             // refused in precheck, before any mutation
    };
    struct Refusal {
        StepRefusal code = StepRefusal::None;
        QString message;        // user-presentable when code != None
        QString documentSha256; // current bytes hash (DocumentChanged: what the
                                // re-confirm must accept; empty elsewhere)
    };

    /// Pure precheck (reads document bytes/anchors, runs NO engine mutation).
    static Refusal precheck(SignatureManager &signing, const FillStepInput &in);

    /// SHA-256 of the current on-disk bytes (empty when unreadable). The
    /// controller's re-confirm flow stores this into
    /// FillStepInput::userReconfirmedSha256 (the gate's ONLY re-confirm
    /// input) after the user accepts, and records it in the sidecar's
    /// reconfirmedSha256 for display.
    static QString documentSha256(const QString &path);

    struct FillStepResult {
        bool attempted = false;       // got past precheck
        SignOutcome outcome = SignOutcome::NotRun;
        SignatureOutcomeDetail detail;
        QString attainedLevel;        // engine's own attained level label
        QString signedFieldName;      // field that REALLY received the signature
        bool fieldMatch = false;      // signedFieldName == the bound fieldName
        bool committed = false;       // signed bytes replaced the document
        bool fieldCreated = false;    // the step created its anchored field first
        QString signatureSummary;     // validation outcome of the new signature
        QString documentSha256;       // SHA-256 of the post-step (or post-create)
                                      // document bytes — the next gate identity
        QString error;                // non-empty = the step failed (with why)
    };

    /// Run ONE real signing step: create the signer's anchored field if it
    /// does not exist yet (LAZY placement — see the note below), then sign the
    /// document to a SafeSave candidate, validate the candidate, and
    /// atomically commit it over docPath. On any failure the on-disk document
    /// is untouched (a created field is the one exception: `fieldCreated`
    /// carries the post-creation hash so the caller can advance the request's
    /// prepared identity — a retried step must not refuse its own creation).
    ///
    /// LAZY PLACEMENT (engine-seam note): SignatureManager's post-condition
    /// refuses EVERY sign whose document still contains ANY unsigned signature
    /// field (its D6 re-validation lists empty fields with integrityIntact
    /// = false). Sequential multi-signer filling therefore cannot have the
    /// future signers' /Sig fields on the document while an earlier step
    /// signs — each step creates exactly ITS OWN field, then signs it, and
    /// the post-condition sees only signed fields. The sidecar carries the
    /// anchors for the not-yet-placed fields; nothing about the recorded
    /// honesty changes. The plan's own multi-signer model (§4.1) is
    /// "each recipient signs a separate returned copy in a separate
    /// incremental update" — the same one-unsigned-field-at-a-time shape.
    static FillStepResult runFillStep(SignatureManager &signing, const FillStepInput &in);

    /// Fold a successful result into the model entry (marks signed + timestamp
    /// + the engine's own attained level + what actually happened) and ADVANCE
    /// preparedSha256 to the engine-attested post-step bytes — the workflow's
    /// own step must never trip the mutation gate for the NEXT step (only
    /// out-of-band document changes do). Returns false (model untouched) when
    /// `result` does not describe a committed, engine-attested signature — the
    /// model must never claim an unverified one.
    static bool applyStepToModel(SigningRequestModel &model, int signerIndex,
                                 const FillStepResult &result);

    // ── Already-signed verification (fill-flow open + completion) ───────────
    struct SignerVerification {
        int index = -1;
        QString fieldName;
        bool entrySigned = false;
        bool fieldHasValidSignature = false; // engine-validated signature on the field
        QString note;
    };
    struct VerificationReport {
        QVector<SignerVerification> perSigner;
        int documentSignatureCount = 0; // ByteRange-covered signatures in the doc
        int signedEntryCount = 0;       // entries the sidecar claims signed
        bool consistent = true;         // no warnings
        QStringList warnings;           // honest mismatch disclosures
    };

    /// Verify the request's claims against the document's ACTUAL signatures
    /// (count + per-field coverage). Every mismatch becomes an explicit
    /// warning — out-of-sync sidecars are flagged, never hidden and never
    /// silently "fixed".
    static VerificationReport verifyAgainstDocument(SignatureManager &signing,
                                                    const SigningRequestModel &model,
                                                    const QString &docPath);
};
