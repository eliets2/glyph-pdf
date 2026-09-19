// SPDX-License-Identifier: Apache-2.0
#include "SigningRequestModel.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>

namespace {

constexpr char kMagicKey[] = "glyphpdf-signrequest";
constexpr int kSchemaVersion = 1;

QJsonObject rectToJson(const QRectF &r)
{
    QJsonObject o;
    o.insert(QStringLiteral("x"), r.x());
    o.insert(QStringLiteral("y"), r.y());
    o.insert(QStringLiteral("w"), r.width());
    o.insert(QStringLiteral("h"), r.height());
    return o;
}

bool rectFromJson(const QJsonValue &v, QRectF *out)
{
    if (!v.isObject()) return false;
    const QJsonObject o = v.toObject();
    const double x = o.value(QStringLiteral("x")).toDouble(-1.0);
    const double y = o.value(QStringLiteral("y")).toDouble(-1.0);
    const double w = o.value(QStringLiteral("w")).toDouble(-1.0);
    const double h = o.value(QStringLiteral("h")).toDouble(-1.0);
    // Reject sentinel/absent values: a stored anchor must be a real rect.
    if (w <= 0.0 || h <= 0.0 || x < 0.0 || y < 0.0) return false;
    *out = QRectF(x, y, w, h);
    return true;
}

} // namespace

// NOTE: loadErrorText's SchemaInvalid branch intentionally repeats `detail`
// (path + reason) inside one sentence; kept simple because the two detail
// pieces are the same string by construction at the call sites below.

QString SigningRequestModel::sidecarPathFor(const QString &docPath)
{
    return docPath + QStringLiteral(".signrequest.json");
}

int SigningRequestModel::currentSchemaVersion() { return kSchemaVersion; }

QString SigningRequestModel::advisoryOrderDisclosure()
{
    return QStringLiteral(
        "Signing order is advisory: GlyphPDF guides signers in this sequence "
        "and verifies every result, but PDF signature fields carry no order "
        "constraint — the order is guidance, not enforcement.");
}

QString SigningRequestModel::mutationRefusalMessage(const QString &docPath,
                                                    const QString &expectedSha256,
                                                    const QString &foundSha256)
{
    return QStringLiteral(
        "The document %1 changed since the signing request was prepared "
        "(expected SHA-256 %2, found %3). Signing steps are refused until you "
        "review and re-confirm the request against the changed document.")
        .arg(docPath, expectedSha256.left(16), foundSha256.left(16));
}

int SigningRequestModel::currentSignerIndex() const
{
    for (int i = 0; i < signers.size(); ++i)
        if (!signers[i].isSigned) return i;
    return signers.size();
}

QStringList SigningRequestModel::boundFieldNames() const
{
    QStringList out;
    for (const Signer &s : signers) out << s.fieldName;
    return out;
}

QJsonDocument SigningRequestModel::toJson() const
{
    QJsonArray signersArr;
    for (const Signer &s : signers) {
        QJsonObject o;
        o.insert(QStringLiteral("order"), signersArr.size() + 1); // 1-based advisory order
        o.insert(QStringLiteral("name"), s.name);
        o.insert(QStringLiteral("fieldName"), s.fieldName);
        o.insert(QStringLiteral("anchorPage"), s.anchorPage);
        if (s.anchorPage >= 0 && s.anchorRect.isValid())
            o.insert(QStringLiteral("anchorRect"), rectToJson(s.anchorRect));
        o.insert(QStringLiteral("createdField"), s.createdField);
        o.insert(QStringLiteral("signed"), s.isSigned);
        o.insert(QStringLiteral("signedAtUtc"), s.signedAtUtc);
        o.insert(QStringLiteral("signedFieldName"), s.signedFieldName);
        o.insert(QStringLiteral("fieldMatch"), s.fieldMatch);
        o.insert(QStringLiteral("attainedLevel"), s.attainedLevel);
        o.insert(QStringLiteral("signatureSummary"), s.signatureSummary);
        signersArr.append(o);
    }
    QJsonObject root;
    root.insert(QLatin1String(kMagicKey), kSchemaVersion);
    root.insert(QStringLiteral("schemaVersion"), kSchemaVersion);
    root.insert(QStringLiteral("createdUtc"), createdUtc);
    root.insert(QStringLiteral("preparedUtc"), preparedUtc);
    root.insert(QStringLiteral("preparedSha256"), preparedSha256);
    root.insert(QStringLiteral("reconfirmedSha256"), reconfirmedSha256);
    root.insert(QStringLiteral("sourcePdf"), sourcePdfName);
    root.insert(QStringLiteral("signers"), signersArr);
    return QJsonDocument(root);
}

SigningRequestModel::LoadResult SigningRequestModel::fromJson(const QString &jsonText)
{
    LoadResult result;
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &parseErr);
    if (doc.isNull() || !doc.isObject()) {
        result.error = LoadError::CorruptJson;
        result.detail = QStringLiteral("parse error at offset %1").arg(parseErr.offset);
        return result;
    }
    const QJsonObject root = doc.object();

    // Handshake, fail-closed, in strict order: magic → version → shape.
    const QJsonValue magic = root.value(QLatin1String(kMagicKey));
    if (magic.isUndefined()) {
        result.error = LoadError::MissingMagic;
        return result;
    }
    const int version = root.value(QStringLiteral("schemaVersion")).toInt(-1);
    if (version != kSchemaVersion) {
        result.error = LoadError::UnknownVersion;
        result.detail = QStringLiteral("schema version %1").arg(
            root.value(QStringLiteral("schemaVersion")).toVariant().toString());
        return result;
    }

    const QJsonValue signersVal = root.value(QStringLiteral("signers"));
    if (!signersVal.isArray()) {
        result.error = LoadError::SchemaInvalid;
        result.detail = QStringLiteral("\"signers\" is not an array");
        return result;
    }
    SigningRequestModel m;
    m.createdUtc = root.value(QStringLiteral("createdUtc")).toString();
    m.preparedUtc = root.value(QStringLiteral("preparedUtc")).toString();
    m.preparedSha256 = root.value(QStringLiteral("preparedSha256")).toString();
    m.reconfirmedSha256 = root.value(QStringLiteral("reconfirmedSha256")).toString();
    m.sourcePdfName = root.value(QStringLiteral("sourcePdf")).toString();

    const QJsonArray arr = signersVal.toArray();
    for (int i = 0; i < arr.size(); ++i) {
        const QJsonValue v = arr.at(i);
        if (!v.isObject()) {
            result.error = LoadError::SchemaInvalid;
            result.detail = QStringLiteral("signer %1 is not an object").arg(i + 1);
            return result;
        }
        const QJsonObject o = v.toObject();
        Signer s;
        s.name = o.value(QStringLiteral("name")).toString();
        s.fieldName = o.value(QStringLiteral("fieldName")).toString();
        if (s.name.isEmpty() || s.fieldName.isEmpty()) {
            result.error = LoadError::SchemaInvalid;
            result.detail = QStringLiteral("signer %1 is missing a name or fieldName").arg(i + 1);
            return result;
        }
        s.anchorPage = o.value(QStringLiteral("anchorPage")).toInt(-1);
        if (o.contains(QStringLiteral("anchorRect"))) {
            QRectF r;
            if (rectFromJson(o.value(QStringLiteral("anchorRect")), &r)) {
                s.anchorRect = r;
            } else {
                result.error = LoadError::SchemaInvalid;
                result.detail = QStringLiteral("signer %1 has an invalid anchorRect").arg(i + 1);
                return result;
            }
        }
        s.createdField = o.value(QStringLiteral("createdField")).toBool(false);
        s.isSigned = o.value(QStringLiteral("signed")).toBool(false);
        s.signedAtUtc = o.value(QStringLiteral("signedAtUtc")).toString();
        s.signedFieldName = o.value(QStringLiteral("signedFieldName")).toString();
        s.fieldMatch = o.value(QStringLiteral("fieldMatch")).toBool(false);
        s.attainedLevel = o.value(QStringLiteral("attainedLevel")).toString();
        s.signatureSummary = o.value(QStringLiteral("signatureSummary")).toString();
        m.signers.append(s);
    }

    // W1-02 — the 1-field==1-signer lint at the ONLY untrusted boundary. One
    // signature field carries exactly one signature, so no two entries may
    // bind the same field (compared trimmed — whitespace must not launder an
    // alias). The prepare path already enforces this
    // (SigningRequestDialog::saveRequest: "Signature field %1 is bound more
    // than once.", SignatureFieldCreator refuses duplicates); fromJson is
    // where attacker-crafted bytes enter, so it enforces it too. Scope: a
    // duplicate binding among entries still AWAITING signature is a work
    // order the workflow can never fulfill — a structured SchemaInvalid
    // refusal. A record where every aliased entry claims a COMPLETED
    // signature parses as history; its truth is exactly what
    // SigningRequestRunner::verifyAgainstDocument audits, which flags aliased
    // bindings as out-of-sync (defense in depth, never silent).
    for (int i = 0; i < m.signers.size(); ++i) {
        const QString fieldI = m.signers[i].fieldName.trimmed();
        for (int j = i + 1; j < m.signers.size(); ++j) {
            if (m.signers[j].fieldName.trimmed() != fieldI)
                continue;
            if (!m.signers[i].isSigned || !m.signers[j].isSigned) {
                result.error = LoadError::SchemaInvalid;
                result.detail = QStringLiteral(
                    "Signature field %1 is bound more than once (signers %2 "
                    "and %3) — one signature field carries exactly one "
                    "signer's signature, so the request cannot be fulfilled "
                    "as bound.")
                    .arg(m.signers[i].fieldName.trimmed())
                    .arg(i + 1).arg(j + 1);
                return result;
            }
        }
    }

    result.model = m;
    return result;
}

SigningRequestModel::LoadResult SigningRequestModel::load(const QString &sidecarPath)
{
    LoadResult result;
    QFile f(sidecarPath);
    if (!f.exists()) {
        result.error = LoadError::FileNotFound;
        result.detail = sidecarPath;
        return result;
    }
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = LoadError::Unreadable;
        result.detail = sidecarPath;
        return result;
    }
    LoadResult parsed = fromJson(QString::fromUtf8(f.readAll()));
    if (parsed.error != LoadError::None && parsed.detail.isEmpty())
        parsed.detail = sidecarPath;
    // SchemaInvalid's sentence wants the PATH first; rebuild it with both.
    if (parsed.error == LoadError::SchemaInvalid)
        parsed.detail = QStringLiteral("%1 (%2)").arg(sidecarPath, parsed.detail);
    return parsed;
}

bool SigningRequestModel::save(const QString &sidecarPath, QString *err) const
{
    const QFileInfo fi(sidecarPath);
    if (!fi.dir().exists() && !QDir().mkpath(fi.absolutePath())) {
        if (err) *err = QStringLiteral("cannot create directory %1").arg(fi.absolutePath());
        return false;
    }
    // QSaveFile for the sidecar too: a crashed write must never leave a
    // half-written request behind (the fill flow refuses corrupt sidecars).
    QSaveFile sf(sidecarPath);
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("cannot open %1 for writing").arg(sidecarPath);
        return false;
    }
    const QByteArray bytes = toJson().toJson(QJsonDocument::Indented);
    if (sf.write(bytes) != bytes.size() || !sf.commit()) {
        if (err) *err = QStringLiteral("writing %1 failed").arg(sidecarPath);
        return false;
    }
    return true;
}
