// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QRectF>
#include <QDateTime>
#include <QJsonDocument>

// ── R26 send-for-signing P1 — signing-request model + versioned sidecar ──────
//
// One document carries at most one signing request, persisted NEXT TO the PDF
// as `<file>.signrequest.json` (the plan's Contract.sigreq.json, scoped to the
// single-document P1 slice). The model is PURE data + JSON: no engine, no GUI,
// so the whole handshake/validation surface is unit-testable offscreen.
//
// Honesty contract baked into the data (moat M8):
//   - `preparedSha256` binds the request to the PREPARED document bytes; the
//     fill flow refuses steps while the on-disk bytes differ, unless the user
//     explicitly re-confirmed the change in the controller's dialog. SWEEP-W1
//     F3: that authorization travels out of band
//     (FillStepInput::userReconfirmedSha256) — the sidecar's
//     `reconfirmedSha256` is a display/record value and NEVER gates, because
//     the sidecar is unsigned JSON anyone can write.
//   - a signer entry records what the ENGINE ACTUALLY DID (signedFieldName =
//     the field that really received the signature, attainedLevel = the
//     engine's own attained PAdES level label, signatureSummary = the
//     validation outcome) — never what was merely requested.
//   - the signer ORDER is ADVISORY. PDF signature fields carry no order
//     constraint; GlyphPDF guides and verifies but does not enforce
//     (plan §2.6 scope cut D3 — disclosed by advisoryOrderDisclosure()).
//
// Schema handshake (BatchPreset discipline, fail-closed): the file MUST carry
// the magic key "glyphpdf-signrequest": 1 AND "schemaVersion": 1. Anything
// else — missing magic, unknown version, corrupt JSON, wrong types — is a
// structured REFUSAL, never a best-effort parse.
class SigningRequestModel {
public:
    /// Sidecar path convention for a document: `<docPath>.signrequest.json`.
    static QString sidecarPathFor(const QString &docPath);

    /// Current schema version written by this build.
    static int currentSchemaVersion();

    /// The advisory-order disclosure (plan D3 scope cut). One sentence, UI +
    /// tests share the exact wording.
    static QString advisoryOrderDisclosure();

    /// Why a fill step refuses to run against mutated document bytes.
    static QString mutationRefusalMessage(const QString &docPath,
                                          const QString &expectedSha256,
                                          const QString &foundSha256);

    /// One signer of the ordered request.
    struct Signer {
        QString name;              // display label ("A. Buyer"); never empty
        QString fieldName;         // the bound signature field (fully qualified)
        int anchorPage = -1;       // 0-based page of the anchor; -1 = not anchored
        QRectF anchorRect;         // VIEWER convention (top-left, Y down);
                                   // invalid QRectF when anchorPage < 0
        bool createdField = false; // the prepare step created this field
        // ── filled ONLY by an actual engine step (never claimed otherwise) ──
        // (`isSigned`, not `signed` — the latter is a C++ keyword.)
        bool isSigned = false;
        QString signedAtUtc;       // ISO-8601 UTC when the engine wrote it
        QString signedFieldName;   // the field that REALLY received the signature
        bool fieldMatch = false;   // signedFieldName == fieldName
        QString attainedLevel;     // the engine's OWN attained PAdES level label
        QString signatureSummary;  // validation outcome summary for the entry
    };

    QVector<Signer> signers;       // index IS the advisory order (0-based)
    QString createdUtc;            // ISO-8601 UTC of first preparation
    QString preparedUtc;           // ISO-8601 UTC of the last prepare/re-confirm
    QString preparedSha256;        // identity of the PREPARED document bytes
    QString reconfirmedSha256;     // set when the user re-confirms changed bytes
    QString sourcePdfName;         // informational: base file name at prepare

    bool isEmpty() const { return signers.isEmpty(); }

    /// Index of the CURRENT signer: the first entry that is not signed yet.
    /// signers.size() when every entry is signed (request complete).
    int currentSignerIndex() const;

    bool isComplete() const { return currentSignerIndex() >= signers.size(); }

    /// Serialize to the sidecar JSON document (writes currentSchemaVersion).
    QJsonDocument toJson() const;

    // ── Load with the fail-closed handshake ─────────────────────────────────
    enum class LoadError {
        None = 0,
        FileNotFound,     // no sidecar at the path (normal for unprepared docs)
        Unreadable,       // exists but cannot be opened
        CorruptJson,      // not valid JSON / not an object
        MissingMagic,     // "glyphpdf-signrequest" key absent
        UnknownVersion,   // magic present but version != currentSchemaVersion
        SchemaInvalid     // right version, wrong shape (signers not an array,
                          // a signer missing name/fieldName, bad rect types…)
    };

    // Defined AFTER the class (it holds a SigningRequestModel by value —
    // an in-class definition would be self-referentially incomplete).
    struct LoadResult;

    /// Parse the sidecar JSON text. Fail-closed: the model is populated ONLY
    /// on a clean handshake — a refused file never yields a half-parsed model.
    static LoadResult fromJson(const QString &jsonText);

    /// Load from a sidecar file path (thin file wrapper around fromJson).
    static LoadResult load(const QString &sidecarPath);

    /// Write the sidecar to `sidecarPath`. Returns false with `err` on any
    /// I/O failure. Marks the request with the written schema version.
    bool save(const QString &sidecarPath, QString *err) const;

    /// The field names bound to the request, in advisory order (test/pin aid).
    QStringList boundFieldNames() const;
};

/// Outcome of a sidecar load: a structured refusal OR a fully validated model.
struct SigningRequestModel::LoadResult {
    LoadError error = LoadError::None;
    QString detail;   // user-presentable when error != None
    SigningRequestModel model;
};
