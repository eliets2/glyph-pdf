// SPDX-License-Identifier: Apache-2.0
#include "SigningRequestRunner.h"

#include "engines/SignatureFieldCreator.h"
#include "engines/SignatureManager.h"
#include "engines/SafeSave.h"
#include "shell/controllers/SecurityController.h" // R19c attainedLevelLabel (pure static)

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSet>

namespace SafeSave = gp::SafeSave;        // gp::SafeSave is a NAMESPACE of primitives
using gp::SecurityController;
using gp::SignatureFieldCreator;

namespace {

// validateSignatures appends UNSIGNED fields as entries with
// trustStatus "Unsigned" (the walk's no-/ByteRange branch) — those are not
// signatures. A field counts as SIGNED exactly when the walk did NOT classify
// it Unsigned (any real /ByteRange — valid or malformed — blocks re-signing,
// the same rule SignatureManager's own unsigned-field scan applies).
bool isSignedEntry(const SignatureInfo &info)
{
    return info.trustStatus != QStringLiteral("Unsigned");
}

QString sha256OfFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    char buf[65536];
    qint64 n;
    while ((n = f.read(buf, sizeof(buf))) > 0)
        hash.addData(QByteArrayView(buf, static_cast<int>(n)));
    return hash.result().toHex();
}

} // namespace

QString SigningRequestRunner::documentSha256(const QString &path)
{
    return sha256OfFile(path);
}

SigningRequestRunner::Refusal SigningRequestRunner::precheck(SignatureManager &signing,
                                                             const FillStepInput &in)
{
    Refusal r;
    if (in.docPath.isEmpty() || !QFileInfo::exists(in.docPath)) {
        r.code = StepRefusal::NoDocument;
        r.message = QStringLiteral("The document %1 does not exist.").arg(in.docPath);
        return r;
    }
    if (in.model.isComplete()) {
        r.code = StepRefusal::Complete;
        r.message = QStringLiteral("Every signer of this request has already signed.");
        return r;
    }
    if (in.signerIndex < 0 || in.signerIndex >= in.model.signers.size()) {
        r.code = StepRefusal::BadIndex;
        r.message = QStringLiteral("Signer %1 is not part of this request.").arg(in.signerIndex + 1);
        return r;
    }
    if (in.model.signers[in.signerIndex].isSigned) {
        r.code = StepRefusal::AlreadySigned;
        r.message = QStringLiteral("Signer %1 has already signed.").arg(in.signerIndex + 1);
        return r;
    }
    // Mutation gate (honesty rule 1): the request binds to the PREPARED bytes.
    // Fail-closed: a request that does NOT record the prepared-bytes hash can
    // never prove the document unmutated, so it refuses too (only a hand-edited
    // sidecar can be in that state — prepare always writes the hash).
    const QString prepared = in.model.preparedSha256;
    const QString reconfirmed = in.model.reconfirmedSha256;
    const QString current = sha256OfFile(in.docPath);
    if (current.isEmpty()) {
        r.code = StepRefusal::NoDocument;
        r.message = QStringLiteral("The document %1 could not be read.").arg(in.docPath);
        return r;
    }
    r.documentSha256 = current;
    const bool preparedUnrecorded = prepared.isEmpty();
    const bool preparedMatches = (!prepared.isEmpty() && current == prepared)
                                 || (!reconfirmed.isEmpty() && current == reconfirmed);
    if (preparedUnrecorded || !preparedMatches) {
        r.code = StepRefusal::DocumentChanged;
        r.message = preparedUnrecorded
            ? QStringLiteral("This signing request does not record the prepared document "
                             "bytes, so it cannot prove the document is unmutated — it was "
                             "refused. Re-prepare the request.")
            : SigningRequestModel::mutationRefusalMessage(in.docPath, prepared, current);
        return r;
    }
    // Binding gate: the bound field must either exist unsigned or be lazily
    // creatable from the entry's anchor (LAZY PLACEMENT — see runFillStep).
    // A binding onto an already-signed field would double-claim one field for
    // two signers, so it is refused outright.
    const SigningRequestModel::Signer &entry = in.model.signers[in.signerIndex];
    const QString bound = entry.fieldName;
    const QList<ISignatureManager::SignatureFieldAnchor> anchors =
        signing.signatureFieldAnchors(in.docPath);
    bool fieldExists = false;
    for (const auto &a : anchors)
        if (a.fieldName == bound) { fieldExists = true; break; }
    if (!fieldExists) {
        const bool canCreate = entry.createdField && entry.anchorPage >= 0
                               && entry.anchorRect.isValid();
        if (!canCreate) {
            r.code = StepRefusal::MissingField;
            r.message = QStringLiteral("Signature field %1 does not exist in the document "
                                       "and the request entry carries no anchor to create "
                                       "it — the request cannot be filled as prepared.")
                            .arg(bound);
            return r;
        }
    }
    const QList<SignatureInfo> infos = signing.validateSignatures(in.docPath);
    for (const SignatureInfo &info : infos) {
        if (info.fieldName == bound && isSignedEntry(info)) {
            r.code = StepRefusal::FieldAlreadySigned;
            r.message = QStringLiteral("Signature field %1 already carries a signature — "
                                       "it cannot be signed again for signer %2.")
                            .arg(bound).arg(in.signerIndex + 1);
            return r;
        }
    }
    return r;
}

SigningRequestRunner::FillStepResult SigningRequestRunner::runFillStep(SignatureManager &signing,
                                                                       const FillStepInput &in)
{
    FillStepResult result;
    const Refusal refusal = precheck(signing, in);
    if (refusal.code != StepRefusal::None) {
        result.error = refusal.message;
        return result;
    }
    result.attempted = true;

    // LAZY PLACEMENT: create the anchored field for THIS step when it does not
    // exist yet. After the creation the document contains exactly ONE unsigned
    // signature field (this step's own) — the engine's post-condition
    // re-validation demands that (see runFillStep header note).
    {
        const SigningRequestModel::Signer &stepEntry = in.model.signers[in.signerIndex];
        bool fieldExists = false;
        for (const auto &a : signing.signatureFieldAnchors(in.docPath))
            if (a.fieldName == stepEntry.fieldName) { fieldExists = true; break; }
        if (!fieldExists) {
            QVector<SignatureFieldCreator::Spec> specs;
            SignatureFieldCreator::Spec spec;
            spec.fieldName = stepEntry.fieldName;
            spec.pageIndex = stepEntry.anchorPage;
            spec.viewerRect = stepEntry.anchorRect;
            specs.append(spec);
            QString createErr;
            if (!SignatureFieldCreator::createSignatureFields(in.docPath, specs, in.docPath,
                                                              &createErr)) {
                result.error = QStringLiteral("The signature field %1 could not be placed: %2")
                                   .arg(stepEntry.fieldName, createErr);
                return result;
            }
            result.fieldCreated = true;
            // The creation is the workflow's OWN mutation: publish the
            // post-create hash so the caller can advance the request's
            // prepared identity even if the signing half of the step fails
            // (a retried step must not refuse its own creation).
            result.documentSha256 = sha256OfFile(in.docPath);
        }
    }

    // Sign to OUR candidate via the engine's own N06 replacement contract:
    // input != output → the engine stages/validates a private candidate and
    // only then writes the destination through SafeSave; a failed attempt
    // leaves every pre-existing file untouched. We then commit the validated
    // candidate onto the document ourselves (the fill flow signs "in place"
    // from the user's point of view) with the shell's handle coordinator.
    QString candidate;
    QString candErr;
    if (!SafeSave::makeUniqueCandidate(&candidate, &candErr)) {
        result.error = candErr;
        return result;
    }
    auto dropCandidate = [&candidate]() { QFile::remove(candidate); };

    const SigningRequestModel::Signer &entry = in.model.signers[in.signerIndex];

    // Signature field inventory BEFORE the step — the diff identifies the
    // field the engine actually signed (it signs the first unsigned field in
    // document order, which for an in-order request IS the bound field; an
    // out-of-order request records the honest mismatch below).
    QSet<QString> signedBefore;
    for (const SignatureInfo &info : signing.validateSignatures(in.docPath))
        if (isSignedEntry(info))
            signedBefore.insert(info.fieldName);

    // Apply the caller's signing configuration (R19(b) discipline): the SAME
    // values the user's settings dictate are set before EVERY dispatch.
    signing.setTsaUrl(in.tsaUrl);
    signing.setSignatureLevel(in.requestedLevel);

    const SignOutcome outcome = signing.signDocumentWithAppearance(
        in.docPath, candidate, in.certPath, in.password,
        in.appearance, in.reason, in.location);
    result.outcome = outcome;
    result.detail = signing.lastSignOutcomeDetail();
    // Honesty rule 3: the ATTAINED level (R19c), never the requested one.
    result.attainedLevel = SecurityController::attainedLevelLabel(in.requestedLevel,
                                                                  result.detail);

    if (outcome == SignOutcome::Failed) {
        result.error = QStringLiteral("The signing engine could not write a signature "
                                      "for signer %1 — the document is unchanged.")
                           .arg(in.signerIndex + 1);
        dropCandidate();
        return result;
    }

    // Bytes exist on the candidate (Success or PartialLtvMissing). Validate
    // and identify the field that actually received THIS signature.
    const QList<SignatureInfo> after = signing.validateSignatures(candidate);
    QString actualField;
    SignatureInfo actualInfo;
    for (const SignatureInfo &info : after) {
        if (!signedBefore.contains(info.fieldName)) {
            actualField = info.fieldName;
            actualInfo = info;
            break;   // the engine signs exactly one field per call
        }
    }
    if (actualField.isEmpty()) {
        // No NEW signature field observable — refuse to claim anything.
        result.error = QStringLiteral("The signed result carries no new signature that "
                                      "this request can attribute — nothing was claimed "
                                      "or committed.");
        dropCandidate();
        return result;
    }
    result.signedFieldName = actualField;
    result.fieldMatch = (actualField == entry.fieldName);
    // Honesty rule 4: record the engine's OWN validation of the new signature.
    // NOTE: `isValid` is a TRUST verdict (Valid/UntrustedChain/InvalidEKU/… —
    // a self-signed signer attests UntrustedChain forever); the ByteRange-
    // coverage claim is `integrityIntact`. The request records both, and
    // never conflates the two.
    result.signatureSummary = QStringLiteral("integrity %1 (trust: %2)").arg(
        actualInfo.integrityIntact ? QStringLiteral("intact")
                                   : QStringLiteral("BROKEN"),
        actualInfo.trustStatus.isEmpty() ? QStringLiteral("unknown")
                                         : actualInfo.trustStatus);

    result.documentSha256 = sha256OfFile(candidate);
    QString commitErr;
    if (!SafeSave::commitFileToDestination(candidate, in.docPath, &commitErr)) {
        result.error = QStringLiteral("The signed document could not be committed to %1 "
                                      "— the previous document is preserved: %2")
                           .arg(in.docPath, commitErr);
        dropCandidate();
        return result;
    }
    // commitFileToDestination COPIES — the committed candidate is ours and is
    // dropped on the success path too (the 5c8fd08 leak discipline).
    dropCandidate();
    result.committed = true;
    return result;
}

bool SigningRequestRunner::applyStepToModel(SigningRequestModel &model, int signerIndex,
                                            const FillStepResult &result)
{
    // A claim needs ALL of: committed bytes, an attested field, and an engine
    // outcome that says the signature exists (Failed never records anything).
    if (!result.committed || result.signedFieldName.isEmpty()
        || result.outcome == SignOutcome::Failed
        || signerIndex < 0 || signerIndex >= model.signers.size())
        return false;
    SigningRequestModel::Signer &entry = model.signers[signerIndex];
    entry.isSigned = true;
    entry.signedAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    entry.signedFieldName = result.signedFieldName;
    entry.fieldMatch = result.fieldMatch;
    entry.attainedLevel = result.attainedLevel;
    entry.signatureSummary = result.signatureSummary;
    // The engine-attested post-step bytes become the new prepared identity:
    // the NEXT step's mutation gate compares against the document the
    // workflow itself produced, not the stale prepare-time revision.
    if (!result.documentSha256.isEmpty())
        model.preparedSha256 = result.documentSha256;
    model.preparedUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    return true;
}

SigningRequestRunner::VerificationReport SigningRequestRunner::verifyAgainstDocument(
    SignatureManager &signing, const SigningRequestModel &model, const QString &docPath)
{
    VerificationReport report;
    const QList<SignatureInfo> infos = signing.validateSignatures(docPath);
    // Only ByteRange-covered signatures count — the walk's "Unsigned"
    // entries are empty fields, not signatures.
    int docSignatureCount = 0;
    for (const SignatureInfo &info : infos)
        if (isSignedEntry(info)) ++docSignatureCount;
    report.documentSignatureCount = docSignatureCount;

    for (int i = 0; i < model.signers.size(); ++i) {
        const SigningRequestModel::Signer &s = model.signers[i];
        SignerVerification v;
        v.index = i;
        v.fieldName = s.fieldName;
        v.entrySigned = s.isSigned;
        for (const SignatureInfo &info : infos) {
            if (info.fieldName == s.fieldName) {
                // The coverage claim is the ByteRange-attested INTEGRITY, not
                // the trust verdict: `isValid` is false for every self-signed
                // / untrusted-chain signer even when the signature is intact,
                // and the request records what the ENGINE attests (a real,
                // undamaged signature on the field) — not who is trusted.
                v.fieldHasValidSignature = isSignedEntry(info) && info.integrityIntact;
                break;
            }
        }
        if (v.entrySigned && !v.fieldHasValidSignature)
            report.warnings << QStringLiteral(
                "Signer %1 is recorded as signed, but the document has no intact "
                "(ByteRange-covered) signature on field %2 — the request is out "
                "of sync with the document.")
                .arg(i + 1).arg(s.fieldName);
        if (v.entrySigned) ++report.signedEntryCount;
        report.perSigner.append(v);
    }

    // W1-02 defense-in-depth: one field carries one signature, so two entries
    // bound to the same field can never both be honestly fulfilled. Parse
    // refuses that shape while any aliased entry is still unsigned
    // (SigningRequestModel::fromJson); a record whose aliased entries ALL
    // claim completed signatures parses as history and is audited HERE —
    // per-field coverage alone cannot see the alias (both entries match the
    // same field's signature) and the count check passes whenever the
    // document carries as many real signatures as entries. An aliased
    // binding is an out-of-sync request: disclosed, never "consistent".
    {
        QMap<QString, QList<int>> bindings;   // trimmed field name -> entry indexes
        for (int i = 0; i < model.signers.size(); ++i)
            bindings[model.signers[i].fieldName.trimmed()].append(i);
        for (auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
            if (it.value().size() < 2) continue;
            QStringList who;
            for (int idx : it.value())
                who << QString::number(idx + 1);
            report.warnings << QStringLiteral(
                "Signers %1 are all bound to the same field %2 — a signature "
                "field carries exactly one signature, so this request cannot "
                "be fulfilled as bound.")
                .arg(who.join(QStringLiteral(", ")), it.key());
        }
    }

    if (report.documentSignatureCount < report.signedEntryCount)
        report.warnings << QStringLiteral(
            "The document carries %1 signature(s), fewer than the %2 recorded by the "
            "signing request — the request is out of sync with the document.")
            .arg(report.documentSignatureCount).arg(report.signedEntryCount);
    else if (report.documentSignatureCount > report.signedEntryCount)
        report.warnings << QStringLiteral(
            "The document carries %1 signature(s) but the signing request records %2 — "
            "%3 signature(s) are outside this request.")
            .arg(report.documentSignatureCount).arg(report.signedEntryCount)
            .arg(report.documentSignatureCount - report.signedEntryCount);
    report.consistent = report.warnings.isEmpty();
    return report;
}
