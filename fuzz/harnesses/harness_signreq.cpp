// SPDX-License-Identifier: Apache-2.0
// harness_signreq.cpp — W1 sweep S1: signing-request sidecar JSON decode.
//
// Surface under test (src/core/SigningRequestModel.cpp):
//   SigningRequestModel::fromJson(QString) — the fail-closed magic → version
//   → shape handshake, rect sentinel rejection, preparedSha256 field intake.
//
// Property oracles (finding = ACCEPTS-INVALID or roundtrip failure):
//   P1 magic-value handshake: a document whose "glyphpdf-signrequest" key is
//      PRESENT with a value other than 1 must be refused (the header pins the
//      magic as a "key: 1" pair). Accepted ⇒ ACCEPTS-INVALID.
//   P2 shape invariants on an accepted model: every signer carries a non-empty
//      name AND fieldName (the decoder's own contract).
//   P3 round-trip: toJson() of an accepted model must re-decode cleanly with
//      the same signer count (self-consistency of the codec).
//
// Deterministic driver (sweep_common.h): campaign / one / materialize modes.
#include <cstdio>
#include <string>

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include "core/SigningRequestModel.h"
#include "sweep_common.h"

// SigningRequestModel lives at global scope (no gp:: namespace — matching
// src/core/SigningRequestModel.h).

static const char* kVerdicts[] = {
    "None", "FileNotFound", "Unreadable", "CorruptJson", "MissingMagic",
    "UnknownVersion", "SchemaInvalid", "P1_ACCEPTS_INVALID_MAGIC_VALUE",
    "P2_SIGNER_SHAPE_BROKEN", "P3_ROUNDTRIP_FAIL", "CRASH_GUARD_SURVIVED",
};

static const char* runOne(const std::vector<uint8_t>& data) {
    // Decode exactly as the loader does: bytes → UTF-8 QString → fromJson.
    const QString text = QString::fromUtf8(
        reinterpret_cast<const char*>(data.data()), (qsizetype)data.size());
    SigningRequestModel::LoadResult r = SigningRequestModel::fromJson(text);

    if (r.error == SigningRequestModel::LoadError::None) {
        // ── P1: magic VALUE handshake ────────────────────────────────────
        // The decoder only tests key presence; the header contract says the
        // magic is the pair "glyphpdf-signrequest": 1. Any other value that
        // still yields a clean accept is an accepts-invalid finding.
        const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
        if (doc.isObject()) {
            const QJsonValue m =
                doc.object().value(QLatin1String("glyphpdf-signrequest"));
            if (!m.isUndefined() && (!m.isDouble() || m.toInt() != 1))
                return "FINDING P1_ACCEPTS_INVALID_MAGIC_VALUE";
        }
        // ── P2: accepted ⇒ signer shape holds ────────────────────────────
        for (const SigningRequestModel::Signer& s : r.model.signers) {
            if (s.name.isEmpty() || s.fieldName.isEmpty())
                return "FINDING P2_SIGNER_SHAPE_BROKEN";
        }
        // ── P3: round-trip re-decode ─────────────────────────────────────
        const QString reserialized = QString::fromUtf8(
            r.model.toJson().toJson(QJsonDocument::Indented));
        SigningRequestModel::LoadResult r2 =
            SigningRequestModel::fromJson(reserialized);
        if (r2.error != SigningRequestModel::LoadError::None
            || r2.model.signers.size() != r.model.signers.size())
            return "FINDING P3_ROUNDTRIP_FAIL";
        return "OK";
    }
    if (r.error == SigningRequestModel::LoadError::CorruptJson) return "REJECT CorruptJson";
    if (r.error == SigningRequestModel::LoadError::MissingMagic) return "REJECT MissingMagic";
    if (r.error == SigningRequestModel::LoadError::UnknownVersion) return "REJECT UnknownVersion";
    if (r.error == SigningRequestModel::LoadError::SchemaInvalid) return "REJECT SchemaInvalid";
    return kVerdicts[(int)r.error];
}

int main(int argc, char** argv) { return sweep::driverMain(argc, argv, runOne); }
