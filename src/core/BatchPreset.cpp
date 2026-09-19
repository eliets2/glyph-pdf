// SPDX-License-Identifier: Apache-2.0
#include "core/BatchPreset.h"

#include "core/Capability.h"
#include "core/VersionedJson.h"
#include "engines/PatternRedactor.h" // namedPattern(): the built-in redaction preset keys

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

namespace gp {

// ── Schema vocabulary ─────────────────────────────────────────────────────────

namespace BatchPresetSchema {

QStringList knownOps() {
    return {
        QStringLiteral("compress"),
        QStringLiteral("strip-metadata"),
        QStringLiteral("pdfa-export"),
        QStringLiteral("pdfa-check"),
        QStringLiteral("watermark"),
        QStringLiteral("redact"),
    };
}

bool isKnownOp(const QString& op) { return knownOps().contains(op); }

QString idFromName(const QString& name) {
    QString slug;
    slug.reserve(name.size());
    bool lastDash = true;   // suppress leading dashes
    for (const QChar& ch : name) {
        if (ch.isDigit() || (ch >= QLatin1Char('a') && ch <= QLatin1Char('z')))
            { slug += ch; lastDash = false; }
        else if (ch.isUpper())
            { slug += ch.toLower(); lastDash = false; }
        else if (!lastDash)
            { slug += QLatin1Char('-'); lastDash = true; }
    }
    while (slug.endsWith(QLatin1Char('-'))) slug.chop(1);
    if (slug.isEmpty()) slug = QStringLiteral("preset");
    if (slug.size() > 64) slug = slug.left(64);
    while (slug.endsWith(QLatin1Char('-'))) slug.chop(1);
    return slug;
}

QString defaultNamingTemplate() { return QStringLiteral("{basename}_{preset}.pdf"); }

// Filename sanitation for token replacement values (plan §3.6: no path
// separator can be smuggled through a token).
static QString sanitizeNameComponent(const QString& raw) {
    QString out;
    out.reserve(raw.size());
    for (const QChar& ch : raw) {
        if (ch == QLatin1Char('/') || ch == QLatin1Char('\\') || ch == QLatin1Char(':')
            || ch == QLatin1Char('*') || ch == QLatin1Char('?') || ch == QLatin1Char('"')
            || ch == QLatin1Char('<') || ch == QLatin1Char('>') || ch == QLatin1Char('|'))
            continue;
        out += ch;
    }
    return out;
}

bool resolveNaming(const QString& naming, const QString& basename,
                   const QString& presetId, int fileIndex, const QDate& runDate,
                   QString* outName, QString* err) {
    const QString tmpl = naming.isEmpty() ? defaultNamingTemplate() : naming;

    if (!tmpl.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
        if (err) *err = QStringLiteral("output.naming: %1 — the template must end \".pdf\" "
                                       "(schema v1; every batch preset output is a PDF)")
                                   .arg(tmpl);
        return false;
    }

    QString result;
    result.reserve(tmpl.size());
    for (int i = 0; i < tmpl.size();) {
        if (tmpl[i] == QLatin1Char('{')) {
            const int close = tmpl.indexOf(QLatin1Char('}'), i);
            if (close < 0) {
                if (err) *err = QStringLiteral("output.naming: %1 — unclosed '{' token "
                                               "(schema v1 tokens: {basename}, {preset}, {n}, {date})")
                                               .arg(tmpl);
                return false;
            }
            const QString token = tmpl.mid(i + 1, close - i - 1);
            if (token == QLatin1String("basename"))
                result += sanitizeNameComponent(basename);
            else if (token == QLatin1String("preset"))
                result += sanitizeNameComponent(presetId);
            else if (token == QLatin1String("n"))
                result += QString::number(qMax(1, fileIndex));
            else if (token == QLatin1String("date"))
                result += runDate.toString(Qt::ISODate);
            else {
                // V7: unknown token — reject, never guess (plan §2.4).
                if (err) *err = QStringLiteral("output.naming: {%1} is not a known token "
                                               "(schema v1; this build supports {basename}, "
                                               "{preset}, {n}, {date})").arg(token);
                return false;
            }
            i = close + 1;
        } else {
            result += tmpl[i];
            ++i;
        }
    }
    if (outName) *outName = result;
    return true;
}

int compareVersions(const QString& a, const QString& b) {
    const auto split = [](const QString& v) {
        QList<qint64> parts;
        for (const QString& p : v.split(QLatin1Char('.')))
            parts << p.toLongLong();
        while (parts.size() < 3) parts << 0;
        return parts;
    };
    const QList<qint64> pa = split(a.trimmed());
    const QList<qint64> pb = split(b.trimmed());
    for (int i = 0; i < 3; ++i) {
        if (pa[i] < pb[i]) return -1;
        if (pa[i] > pb[i]) return 1;
    }
    return 0;
}

} // namespace BatchPresetSchema

// ── Validation + codec ────────────────────────────────────────────────────────

namespace {

constexpr char kKind[] = "batch-preset";

[[nodiscard]] bool fail(QString* err, const QString& message) {
    if (err) *err = message;
    return false;
}

// V3 strict typing: an int is a JSON number with an integral value.
bool jsonToInt(const QJsonValue& v, int* out) {
    if (!v.isDouble()) return false;
    const double d = v.toDouble();
    if (d != double(qint64(d))) return false;
    *out = int(d);
    return true;
}

QString supportedOpsText() {
    return BatchPresetSchema::knownOps().join(QStringLiteral(", "));
}

// V2/V3/V4: per-op param spec. Every key must be a named param of the op,
// every value must type- and range-match. Unknown params inside a known op
// are rejected (fail-closed, plan §2.4).
bool validateStepParams(const QString& op, const QVariantMap& params, QString* err,
                        const QString& pathPrefix) {
    const auto isInt = [](const QVariant& v) { return v.typeId() == QMetaType::Int
                                                  || v.typeId() == QMetaType::LongLong; };

    if (op == QLatin1String("compress")) {
        static const QSet<QString> allowed = {
            QStringLiteral("quality"), QStringLiteral("targetDpi") };
        for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
            if (!allowed.contains(it.key()))
                return fail(err, QStringLiteral("%1.params.%2: unknown parameter for op "
                                                "\"compress\" (schema v1; supported: quality, "
                                                "targetDpi)").arg(pathPrefix, it.key()));
            if (!isInt(it.value()))
                return fail(err, QStringLiteral("%1.params.%2: expected an integer "
                                                "(schema v1)").arg(pathPrefix, it.key()));
        }
        if (params.contains(QStringLiteral("quality"))) {
            const int q = params.value(QStringLiteral("quality")).toInt();
            if (q < 10 || q > 100)
                return fail(err, QStringLiteral("%1.params.quality: %2 is out of range 10-100 "
                                                "(schema v1)").arg(pathPrefix).arg(q));
        }
        if (params.contains(QStringLiteral("targetDpi"))) {
            // The schema's DPI range is the engine's documented clamp range
            // (BatchMode::kMinTargetDpi/kMaxTargetDpi — plan §1.3); pinned to
            // it by the static_asserts in BatchMode.cpp.
            constexpr int kMinTargetDpi = 36;
            constexpr int kMaxTargetDpi = 600;
            const int dpi = params.value(QStringLiteral("targetDpi")).toInt();
            if (dpi < kMinTargetDpi || dpi > kMaxTargetDpi)
                return fail(err, QStringLiteral("%1.params.targetDpi: %2 is out of range %3-%4 "
                                                "(schema v1)").arg(pathPrefix).arg(dpi)
                                          .arg(kMinTargetDpi).arg(kMaxTargetDpi));
        }
        return true;
    }

    if (op == QLatin1String("strip-metadata")) {
        static const QSet<QString> allowed = {
            QStringLiteral("sanitize"), QStringLiteral("clearInfoDict") };
        for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
            if (!allowed.contains(it.key()))
                return fail(err, QStringLiteral("%1.params.%2: unknown parameter for op "
                                                "\"strip-metadata\" (schema v1; supported: "
                                                "sanitize, clearInfoDict)").arg(pathPrefix, it.key()));
            if (it.value().typeId() != QMetaType::Bool)
                return fail(err, QStringLiteral("%1.params.%2: expected a boolean (schema v1)")
                                           .arg(pathPrefix, it.key()));
        }
        const bool sanitize = params.value(QStringLiteral("sanitize"), true).toBool();
        const bool clearInfo = params.value(QStringLiteral("clearInfoDict"), true).toBool();
        if (!sanitize && !clearInfo)
            return fail(err, QStringLiteral("%1.params: at least one of sanitize / clearInfoDict "
                                            "must be true — a strip-metadata step that neither "
                                            "sanitizes nor clears anything would do nothing "
                                            "(schema v1)").arg(pathPrefix));
        return true;
    }

    if (op == QLatin1String("pdfa-export") || op == QLatin1String("pdfa-check")) {
        static const QSet<QString> allowed = { QStringLiteral("level") };
        for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
            if (!allowed.contains(it.key()))
                return fail(err, QStringLiteral("%1.params.%2: unknown parameter for op \"%3\" "
                                                "(schema v1; supported: level)")
                                           .arg(pathPrefix, it.key(), op));
            if (it.value().typeId() != QMetaType::QString)
                return fail(err, QStringLiteral("%1.params.level: expected a string (schema v1)")
                                           .arg(pathPrefix));
        }
        if (params.contains(QStringLiteral("level"))) {
            static const QSet<QString> levels = {
                QStringLiteral("1b"), QStringLiteral("2b"), QStringLiteral("2u"),
                QStringLiteral("3b"), QStringLiteral("3u") };
            const QString level = params.value(QStringLiteral("level")).toString();
            if (!levels.contains(level))
                return fail(err, QStringLiteral("%1.params.level: \"%2\" is not one of 1b, 2b, "
                                                "2u, 3b, 3u (schema v1; the shipped N03 set)")
                                           .arg(pathPrefix, level));
        }
        return true;
    }

    if (op == QLatin1String("watermark")) {
        static const QSet<QString> allowed = {
            QStringLiteral("text"), QStringLiteral("opacity") };
        for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
            if (!allowed.contains(it.key()))
                return fail(err, QStringLiteral("%1.params.%2: unknown parameter for op "
                                                "\"watermark\" (schema v1; supported: text, "
                                                "opacity)").arg(pathPrefix, it.key()));
        }
        if (params.contains(QStringLiteral("text"))) {
            const QVariant v = params.value(QStringLiteral("text"));
            if (v.typeId() != QMetaType::QString)
                return fail(err, QStringLiteral("%1.params.text: expected a string (schema v1)")
                                           .arg(pathPrefix));
            const QString text = v.toString();
            if (text.trimmed().isEmpty() || text.size() > 120)
                return fail(err, QStringLiteral("%1.params.text: must be 1-120 characters "
                                                "(schema v1)").arg(pathPrefix));
        }
        if (params.contains(QStringLiteral("opacity"))) {
            if (!isInt(params.value(QStringLiteral("opacity"))))
                return fail(err, QStringLiteral("%1.params.opacity: expected an integer percent "
                                                "(schema v1)").arg(pathPrefix));
            const int o = params.value(QStringLiteral("opacity")).toInt();
            if (o < 1 || o > 100)
                return fail(err, QStringLiteral("%1.params.opacity: %2 is out of range 1-100 "
                                                "(schema v1)").arg(pathPrefix).arg(o));
        }
        return true;
    }

    if (op == QLatin1String("redact")) {
        static const QSet<QString> allowed = {
            QStringLiteral("presets"), QStringLiteral("patterns") };
        for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
            if (!allowed.contains(it.key()))
                return fail(err, QStringLiteral("%1.params.%2: unknown parameter for op "
                                                "\"redact\" (schema v1; supported: presets, "
                                                "patterns)").arg(pathPrefix, it.key()));
        }
        bool anyPattern = false;
        if (params.contains(QStringLiteral("presets"))) {
            const QVariant v = params.value(QStringLiteral("presets"));
            if (v.typeId() != QMetaType::QStringList)
                return fail(err, QStringLiteral("%1.params.presets: expected an array of strings "
                                                "(schema v1)").arg(pathPrefix));
            const QStringList keys = v.toStringList();
            for (const QString& key : keys) {
                if (!PatternRedactor::namedPattern(key).isValid())
                    return fail(err, QStringLiteral("%1.params.presets: \"%2\" is not a known "
                                                    "named pattern key (schema v1)")
                                               .arg(pathPrefix, key));
                anyPattern = true;
            }
        }
        if (params.contains(QStringLiteral("patterns"))) {
            const QVariant v = params.value(QStringLiteral("patterns"));
            if (v.typeId() != QMetaType::QStringList)
                return fail(err, QStringLiteral("%1.params.patterns: expected an array of strings "
                                                "(schema v1)").arg(pathPrefix));
            const QStringList patterns = v.toStringList();
            for (const QString& pattern : patterns) {
                QRegularExpression rx(pattern);
                if (!rx.isValid())
                    return fail(err, QStringLiteral("%1.params.patterns: \"%2\" is not a valid "
                                                    "regex (%3) (schema v1)")
                                               .arg(pathPrefix, pattern, rx.errorString()));
                if (!pattern.trimmed().isEmpty()) anyPattern = true;
            }
        }
        if (!anyPattern)
            return fail(err, QStringLiteral("%1.params: at least one effective pattern is "
                                            "required — a redact step with no pattern would "
                                            "redact nothing (schema v1)").arg(pathPrefix));
        return true;
    }

    return fail(err, QStringLiteral("%1.op: unknown operation \"%2\" (schema v1; this app "
                                    "supports: %3)").arg(pathPrefix, op, supportedOpsText()));
}

bool validateSteps(const QList<BatchPresetStep>& steps, QString* err) {
    if (steps.isEmpty())
        return fail(err, QStringLiteral("steps: a preset must contain at least one step "
                                        "(schema v1 allows 1-%1)").arg(BatchPresetSchema::kMaxSteps));
    if (steps.size() > BatchPresetSchema::kMaxSteps)
        return fail(err, QStringLiteral("steps: %1 steps exceed the schema v1 maximum of %2")
                                   .arg(steps.size()).arg(BatchPresetSchema::kMaxSteps));
    for (int i = 0; i < steps.size(); ++i) {
        const BatchPresetStep& step = steps.at(i);
        const QString prefix = QStringLiteral("steps[%1]").arg(i);
        if (step.label.size() > 120)
            return fail(err, QStringLiteral("%1.label: exceeds 120 characters (schema v1)")
                                       .arg(prefix));
        if (!BatchPresetSchema::isKnownOp(step.op))
            return fail(err, QStringLiteral("%1.op: unknown operation \"%2\" (file schema v1; "
                                            "this app supports: %3)")
                                       .arg(prefix, step.op, supportedOpsText()));
        if (!validateStepParams(step.op, step.params, err, prefix))
            return false;
    }
    return true;
}

} // namespace

namespace BatchPresetCodec {

bool validate(const BatchPreset& p, QString* err) {
    if (p.schemaVersion != BatchPresetSchema::kSchemaVersion)
        return fail(err, QStringLiteral("glyphpreset.schemaVersion: %1 is not understood by this "
                                        "build (this app understands schema v1 — update GlyphPDF "
                                        "or ask for a v1 export)")
                                   .arg(p.schemaVersion));
    const static QRegularExpression idRx(QStringLiteral("^[a-z0-9-]{1,64}$"));
    if (!idRx.match(p.id).hasMatch())
        return fail(err, QStringLiteral("id: \"%1\" must match [a-z0-9-]{1,64} (schema v1)")
                                   .arg(p.id));
    if (p.name.trimmed().isEmpty() || p.name.size() > 80)
        return fail(err, QStringLiteral("name: must be 1-80 characters after trim (schema v1)"));
    if (p.description.size() > 300)
        return fail(err, QStringLiteral("description: exceeds 300 characters (schema v1)"));
    if (p.authorApp.size() > 200)
        return fail(err, QStringLiteral("authorApp: exceeds 200 characters (schema v1)"));
    if (!p.created.isValid() || !p.modified.isValid())
        return fail(err, QStringLiteral("created/modified: must be valid timestamps"));
    return validateSteps(p.steps, err);
}

QByteArray serialize(const BatchPreset& p) {
    QJsonObject root;

    QJsonObject envelope;
    envelope.insert(QStringLiteral("schemaVersion"), p.schemaVersion);
    envelope.insert(QStringLiteral("kind"), QLatin1String(kKind));
    if (!p.minAppVersion.isEmpty())
        envelope.insert(QStringLiteral("minAppVersion"), p.minAppVersion);
    root.insert(QStringLiteral("glyphpreset"), envelope);

    root.insert(QStringLiteral("id"), p.id);
    root.insert(QStringLiteral("name"), p.name);
    if (!p.description.isEmpty())
        root.insert(QStringLiteral("description"), p.description);
    root.insert(QStringLiteral("created"),
                p.created.toUTC().toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("modified"),
                p.modified.toUTC().toString(Qt::ISODateWithMs));
    if (!p.authorApp.isEmpty())
        root.insert(QStringLiteral("authorApp"), p.authorApp);

    QJsonArray steps;
    for (const BatchPresetStep& step : p.steps) {
        QJsonObject s;
        s.insert(QStringLiteral("op"), step.op);
        if (!step.label.isEmpty())
            s.insert(QStringLiteral("label"), step.label);
        QJsonObject params;
        for (auto it = step.params.constBegin(); it != step.params.constEnd(); ++it) {
            const QVariant& v = it.value();
            switch (v.typeId()) {
            case QMetaType::Bool:
                params.insert(it.key(), v.toBool()); break;
            case QMetaType::Int:
            case QMetaType::LongLong:
                params.insert(it.key(), double(v.toLongLong())); break;
            case QMetaType::QStringList: {
                QJsonArray arr;
                for (const QString& s2 : v.toStringList()) arr.append(s2);
                params.insert(it.key(), arr);
                break;
            }
            default:
                params.insert(it.key(), v.toString()); break;
            }
        }
        s.insert(QStringLiteral("params"), params);
        steps.append(s);
    }
    root.insert(QStringLiteral("steps"), steps);

    QJsonObject output;
    if (!p.outputNaming.isEmpty() && p.outputNaming != BatchPresetSchema::defaultNamingTemplate())
        output.insert(QStringLiteral("naming"), p.outputNaming);
    if (!p.onConflict.isEmpty() && p.onConflict != QLatin1String("ask"))
        output.insert(QStringLiteral("onConflict"), p.onConflict);
    if (!output.isEmpty())
        root.insert(QStringLiteral("output"), output);

    if (!p.onFileFailure.isEmpty() && p.onFileFailure != QLatin1String("continue"))
        root.insert(QStringLiteral("onFileFailure"), p.onFileFailure);

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool parse(const QByteArray& json, BatchPreset* out, QString* err) {
    if (json.size() > BatchPresetSchema::kMaxFileBytes)
        return fail(err, QStringLiteral("(root): %1-byte file exceeds the 256 KiB schema v1 "
                                        "resource cap — refusing to parse").arg(json.size()));

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject())
        return fail(err, QStringLiteral("(root): not a valid JSON object — %1")
                                   .arg(parseErr.errorString()));
    const QJsonObject root = doc.object();

    // Unknown-key discipline (V5): exact key sets everywhere, fail-closed.
    static const QSet<QString> rootKeys = {
        QStringLiteral("glyphpreset"), QStringLiteral("id"), QStringLiteral("name"),
        QStringLiteral("description"), QStringLiteral("created"), QStringLiteral("modified"),
        QStringLiteral("authorApp"), QStringLiteral("steps"), QStringLiteral("output"),
        QStringLiteral("onFileFailure") };
    for (auto it = root.constBegin(); it != root.constEnd(); ++it)
        if (!rootKeys.contains(it.key()))
            return fail(err, QStringLiteral("%1: unknown key (schema v1; this build "
                                            "understands: %2)")
                                       .arg(it.key(), QStringList(rootKeys.values())
                                                       .join(QStringLiteral(", "))));

    const QJsonObject envelope =
        root.value(QStringLiteral("glyphpreset")).toObject();
    static const QSet<QString> envelopeKeys = {
        QStringLiteral("schemaVersion"), QStringLiteral("kind"),
        QStringLiteral("minAppVersion") };
    for (auto it = envelope.constBegin(); it != envelope.constEnd(); ++it)
        if (!envelopeKeys.contains(it.key()))
            return fail(err, QStringLiteral("glyphpreset.%1: unknown key (schema v1; this build "
                                            "understands: schemaVersion, kind, minAppVersion)")
                                       .arg(it.key()));

    // Version handshake FIRST — never misparse a newer file (plan §2.4).
    int schemaVersion = -1;
    if (!jsonToInt(envelope.value(QStringLiteral("schemaVersion")), &schemaVersion))
        return fail(err, QStringLiteral("glyphpreset.schemaVersion: expected an integer "
                                        "(schema v1)"));
    if (schemaVersion != BatchPresetSchema::kSchemaVersion)
        return fail(err, QStringLiteral("glyphpreset.schemaVersion: this preset uses schema v%1, "
                                        "but this app understands v1. Update GlyphPDF or ask for "
                                        "a v1 export.").arg(schemaVersion));

    const QString kind = envelope.value(QStringLiteral("kind")).toString();
    if (kind != QLatin1String(kKind))
        return fail(err, QStringLiteral("glyphpreset.kind: \"%1\" is not \"%2\" — refusing a file "
                                        "that is not a batch preset (schema v1)")
                                   .arg(kind, QLatin1String(kKind)));
    QString minAppVersion;
    if (envelope.contains(QStringLiteral("minAppVersion"))) {
        if (envelope.value(QStringLiteral("minAppVersion")).type() != QJsonValue::String)
            return fail(err, QStringLiteral("glyphpreset.minAppVersion: expected a string "
                                            "(schema v1)"));
        minAppVersion = envelope.value(QStringLiteral("minAppVersion")).toString();
    }

    // id / name / description / stamps / authorApp
    BatchPreset p;
    p.schemaVersion = schemaVersion;
    p.minAppVersion = minAppVersion;
    p.id = root.value(QStringLiteral("id")).toString();
    p.name = root.value(QStringLiteral("name")).toString();
    if (root.contains(QStringLiteral("description"))) {
        if (root.value(QStringLiteral("description")).type() != QJsonValue::String)
            return fail(err, QStringLiteral("description: expected a string (schema v1)"));
        p.description = root.value(QStringLiteral("description")).toString();
    }
    if (root.contains(QStringLiteral("authorApp"))) {
        if (root.value(QStringLiteral("authorApp")).type() != QJsonValue::String)
            return fail(err, QStringLiteral("authorApp: expected a string (schema v1)"));
        p.authorApp = root.value(QStringLiteral("authorApp")).toString();
    }
    const QDateTime created =
        QDateTime::fromString(root.value(QStringLiteral("created")).toString(),
                              Qt::ISODateWithMs);
    const QDateTime modified =
        QDateTime::fromString(root.value(QStringLiteral("modified")).toString(),
                              Qt::ISODateWithMs);
    if (!created.isValid() || !modified.isValid())
        return fail(err, QStringLiteral("created/modified: expected ISO-8601 UTC timestamps "
                                        "(schema v1)"));
    p.created = created; p.modified = modified;

    // steps — exact key set {op, label, params} per item (V5).
    if (!root.value(QStringLiteral("steps")).isArray())
        return fail(err, QStringLiteral("steps: expected an array (schema v1)"));
    const QJsonArray steps = root.value(QStringLiteral("steps")).toArray();
    if (steps.isEmpty())
        return fail(err, QStringLiteral("steps: a preset must contain at least one step "
                                        "(schema v1 allows 1-%1)")
                                   .arg(BatchPresetSchema::kMaxSteps));
    if (steps.size() > BatchPresetSchema::kMaxSteps)
        return fail(err, QStringLiteral("steps: %1 steps exceed the schema v1 maximum of %2")
                                   .arg(steps.size()).arg(BatchPresetSchema::kMaxSteps));
    for (int i = 0; i < steps.size(); ++i) {
        const QString prefix = QStringLiteral("steps[%1]").arg(i);
        if (!steps.at(i).isObject())
            return fail(err, QStringLiteral("%1: expected an object (schema v1)").arg(prefix));
        const QJsonObject s = steps.at(i).toObject();
        static const QSet<QString> stepKeys = {
            QStringLiteral("op"), QStringLiteral("label"), QStringLiteral("params") };
        for (auto it = s.constBegin(); it != s.constEnd(); ++it)
            if (!stepKeys.contains(it.key()))
                return fail(err, QStringLiteral("%1.%2: unknown key (schema v1; a step "
                                                "understands: op, label, params)")
                                           .arg(prefix, it.key()));
        BatchPresetStep step;
        step.op = s.value(QStringLiteral("op")).toString();
        if (s.contains(QStringLiteral("label"))) {
            if (s.value(QStringLiteral("label")).type() != QJsonValue::String)
                return fail(err, QStringLiteral("%1.label: expected a string (schema v1)")
                                           .arg(prefix));
            step.label = s.value(QStringLiteral("label")).toString();
            if (step.label.size() > 120)
                return fail(err, QStringLiteral("%1.label: exceeds 120 characters (schema v1)")
                                           .arg(prefix));
        }
        if (!s.contains(QStringLiteral("params")))
            return fail(err, QStringLiteral("%1.params: missing — every step carries its "
                                            "parameter object (schema v1)").arg(prefix));
        if (!s.value(QStringLiteral("params")).isObject())
            return fail(err, QStringLiteral("%1.params: expected an object (schema v1)")
                                       .arg(prefix));
        const QJsonObject paramsObj = s.value(QStringLiteral("params")).toObject();
        for (auto it = paramsObj.constBegin(); it != paramsObj.constEnd(); ++it) {
            const QJsonValue& v = it.value();
            if (v.isBool())
                step.params.insert(it.key(), v.toBool());
            else if (v.isDouble()) {
                const double d = v.toDouble();
                if (d != double(qint64(d)))
                    return fail(err, QStringLiteral("%1.params.%2: non-integer number (schema v1)")
                                           .arg(prefix, it.key()));
                step.params.insert(it.key(), int(d));
            } else if (v.isArray()) {
                QStringList list;
                for (const QJsonValue& e : v.toArray()) {
                    if (!e.isString())
                        return fail(err, QStringLiteral("%1.params.%2: expected an array of "
                                                        "strings (schema v1)")
                                                   .arg(prefix, it.key()));
                    list << e.toString();
                    if (e.toString().size() > BatchPresetSchema::kMaxStringParamBytes)
                        return fail(err, QStringLiteral("%1.params.%2: string exceeds the 1 MiB "
                                                        "schema v1 resource cap")
                                                   .arg(prefix, it.key()));
                }
                step.params.insert(it.key(), list);
            } else if (v.isString()) {
                if (v.toString().size() > BatchPresetSchema::kMaxStringParamBytes)
                    return fail(err, QStringLiteral("%1.params.%2: string exceeds the 1 MiB "
                                                    "schema v1 resource cap")
                                               .arg(prefix, it.key()));
                step.params.insert(it.key(), v.toString());
            } else {
                return fail(err, QStringLiteral("%1.params.%2: unsupported value type (schema v1 "
                                                "params are string, integer, boolean or array of "
                                                "strings)").arg(prefix, it.key()));
            }
        }
        if (!validateStepParams(step.op, step.params, err, prefix))
            return false;
        p.steps.append(step);
    }

    // Effective defaults: an omitted policy field parses as its default so a
    // parsed preset and its re-serialization compare equal.
    p.onConflict = QStringLiteral("ask");
    p.onFileFailure = QStringLiteral("continue");

    // output (optional): keys {naming, onConflict}.
    if (root.contains(QStringLiteral("output"))) {
        if (!root.value(QStringLiteral("output")).isObject())
            return fail(err, QStringLiteral("output: expected an object (schema v1)"));
        const QJsonObject output = root.value(QStringLiteral("output")).toObject();
        static const QSet<QString> outputKeys = {
            QStringLiteral("naming"), QStringLiteral("onConflict") };
        for (auto it = output.constBegin(); it != output.constEnd(); ++it)
            if (!outputKeys.contains(it.key()))
                return fail(err, QStringLiteral("output.%1: unknown key (schema v1; output "
                                                "understands: naming, onConflict)").arg(it.key()));
        if (output.contains(QStringLiteral("naming"))) {
            if (output.value(QStringLiteral("naming")).type() != QJsonValue::String)
                return fail(err, QStringLiteral("output.naming: expected a string (schema v1)"));
            p.outputNaming = output.value(QStringLiteral("naming")).toString();
            QString resolvedName;
            QString namingErr;
            if (!BatchPresetSchema::resolveNaming(p.outputNaming, {}, {}, 1, QDate::currentDate(),
                                                  &resolvedName, &namingErr))
                return fail(err, namingErr);   // V7 + ".pdf" suffix rule
        }
        if (output.contains(QStringLiteral("onConflict"))) {
            if (output.value(QStringLiteral("onConflict")).type() != QJsonValue::String)
                return fail(err, QStringLiteral("output.onConflict: expected a string (schema v1)"));
            p.onConflict = output.value(QStringLiteral("onConflict")).toString();
            if (p.onConflict != QLatin1String("ask") && p.onConflict != QLatin1String("overwrite"))
                return fail(err, QStringLiteral("output.onConflict: \"%1\" is not implemented by "
                                                "this build (schema v1 offers ask/overwrite/rename; "
                                                "this app supports: ask, overwrite)").arg(p.onConflict));
        }
    }

    if (root.contains(QStringLiteral("onFileFailure"))) {
        if (root.value(QStringLiteral("onFileFailure")).type() != QJsonValue::String)
            return fail(err, QStringLiteral("onFileFailure: expected a string (schema v1)"));
        p.onFileFailure = root.value(QStringLiteral("onFileFailure")).toString();
        if (p.onFileFailure != QLatin1String("continue"))
            return fail(err, QStringLiteral("onFileFailure: \"%1\" is not implemented by this "
                                            "build (schema v1 offers continue/stop; this app "
                                            "supports: continue)").arg(p.onFileFailure));
    }

    if (!validate(p, err))
        return false;
    if (out) *out = p;
    return true;
}

bool loadFile(const QString& path, BatchPreset* out, QString* err) {
    QFileInfo fi(path);
    if (fi.size() > BatchPresetSchema::kMaxFileBytes)
        return fail(err, QStringLiteral("%1: %2-byte file exceeds the 256 KiB schema v1 "
                                        "resource cap").arg(path).arg(fi.size()));
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return fail(err, QStringLiteral("%1: cannot open — %2").arg(path, f.errorString()));
    if (!parse(f.readAll(), out, err))
        return false;
    // V8 import rule: the id must equal the file stem (ids are stable
    // references; a renamed copy would silently fork the identity otherwise).
    // The double extension ".glyphpreset.json" is stripped as a whole.
    QString stem = fi.fileName();
    const QString doubleExt = QStringLiteral(".glyphpreset.json");
    if (!stem.endsWith(doubleExt))
        return fail(err, QStringLiteral("%1: not a .glyphpreset.json file (schema v1 store "
                                        "layout)").arg(path));
    stem.chop(doubleExt.size());
    if (out && out->id != stem)
        return fail(err, QStringLiteral("id: \"%1\" does not match the filename stem \"%2\" — "
                                        "rename the file (or re-export the preset) so the id and "
                                        "the filename agree (schema v1)").arg(out->id, stem));
    return true;
}

} // namespace BatchPresetCodec

// ── Capability honesty ────────────────────────────────────────────────────────

Capability batchPresetStepCapability(const BatchPresetStep& step,
                                     const CapabilityRegistry* registry) {
    Capability available;
    available.status = Availability::Available;
    available.detail = QStringLiteral("Runs in this build via the built-in PDF editor engine.");

    if (step.op == QLatin1String("pdfa-check")) {
        // veraPDF CLI probe (runtime). A check step that cannot run MUST be
        // disclosed and refused, never silently skipped.
        if (registry)
            return registry->query(CapId::PdfAValidation);
        Capability c;
        c.status = Availability::UnavailableRuntime;
        c.whyNot = QStringLiteral("PDF/A conformance checking requires the veraPDF validator, "
                                  "whose availability cannot be probed right now.");
        c.alternative = QStringLiteral("Remove the pdfa-check step, or restart the application.");
        return c;
    }
    if (step.op == QLatin1String("pdfa-export")) {
        if (registry)
            return registry->query(CapId::PdfAExport);
        return available;
    }
    if (BatchPresetSchema::isKnownOp(step.op))
        return available;

    Capability unknown;
    unknown.status = Availability::UnavailableBuild;
    unknown.whyNot = QStringLiteral("Operation \"%1\" is not implemented by this build (supported "
                                    "ops: %2).").arg(step.op,
                                                     BatchPresetSchema::knownOps()
                                                         .join(QStringLiteral(", ")));
    unknown.alternative = QStringLiteral("Edit the preset to use supported operations only.");
    return unknown;
}

// ── Store ─────────────────────────────────────────────────────────────────────

BatchPresetStore::BatchPresetStore(const QString& rootDir)
    : m_rootDir(rootDir.isEmpty() ? defaultRootDir() : rootDir) {}

QString BatchPresetStore::defaultRootDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
         + QStringLiteral("/presets");
}

QList<BatchPreset> BatchPresetStore::list() const {
    QList<BatchPreset> presets;
    const auto files = QDir(m_rootDir)
                           .entryInfoList(QStringList() << QStringLiteral("*.glyphpreset.json"),
                                          QDir::Files, QDir::Name);
    for (const QFileInfo& fi : files) {
        BatchPreset p;
        QString err;
        if (BatchPresetCodec::loadFile(fi.absoluteFilePath(), &p, &err))
            presets.append(p);
        // Broken files are reported by brokenFiles(), never silently hidden.
    }
    std::sort(presets.begin(), presets.end(),
              [](const BatchPreset& a, const BatchPreset& b) {
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });
    return presets;
}

QList<BatchPresetStore::BrokenFile> BatchPresetStore::brokenFiles() const {
    QList<BrokenFile> broken;
    const auto files = QDir(m_rootDir)
                           .entryInfoList(QStringList() << QStringLiteral("*.glyphpreset.json"),
                                          QDir::Files, QDir::Name);
    for (const QFileInfo& fi : files) {
        BatchPreset p;
        QString err;
        if (!BatchPresetCodec::loadFile(fi.absoluteFilePath(), &p, &err))
            broken.append({ fi.absoluteFilePath(), err });
    }
    return broken;
}

bool BatchPresetStore::contains(const QString& id) const {
    return QFileInfo::exists(QDir(m_rootDir).filePath(id + QStringLiteral(".glyphpreset.json")));
}

bool BatchPresetStore::get(const QString& id, BatchPreset* out, QString* err) const {
    return BatchPresetCodec::loadFile(
        QDir(m_rootDir).filePath(id + QStringLiteral(".glyphpreset.json")), out, err);
}

bool BatchPresetStore::save(BatchPreset* preset, QString* err) {
    if (!preset)
        return fail(err, QStringLiteral("internal: no preset to save"));

    // Stamp creation/modification when the caller did not.
    if (!preset->created.isValid())
        preset->created = QDateTime::currentDateTimeUtc();
    if (!preset->modified.isValid())
        preset->modified = QDateTime::currentDateTimeUtc();

    // Assign a unique slug id from the display name when creating.
    if (preset->id.isEmpty()) {
        const QString base = BatchPresetSchema::idFromName(preset->name);
        QString id = base;
        int suffix = 2;
        while (contains(id))
            id = base + QStringLiteral("-%1").arg(suffix++);
        preset->id = id;
    }

    QString validationErr;
    if (!BatchPresetCodec::validate(*preset, &validationErr))
        return fail(err, validationErr);

    const QString path =
        QDir(m_rootDir).filePath(preset->id + QStringLiteral(".glyphpreset.json"));
    // Canonical versioned-artifact commit (shared mechanic; the store never
    // creates directories — a missing root is an honest write refusal).
    return VersionedJson::atomicWrite(path, BatchPresetCodec::serialize(*preset), err);
}

bool BatchPresetStore::rename(const QString& id, const QString& newName, QString* err) {
    BatchPreset p;
    if (!get(id, &p, err))
        return false;
    p.name = newName;
    p.modified = QDateTime::currentDateTimeUtc();
    QString validationErr;
    if (!BatchPresetCodec::validate(p, &validationErr))
        return fail(err, validationErr);
    const QString path =
        QDir(m_rootDir).filePath(id + QStringLiteral(".glyphpreset.json"));
    return VersionedJson::atomicWrite(path, BatchPresetCodec::serialize(p), err);
}

bool BatchPresetStore::remove(const QString& id, QString* err) {
    const QString path =
        QDir(m_rootDir).filePath(id + QStringLiteral(".glyphpreset.json"));
    if (!QFileInfo::exists(path))
        return fail(err, QStringLiteral("%1 does not exist — nothing to delete").arg(path));
    if (!QFile::remove(path))
        return fail(err, QStringLiteral("%1: could not delete — %2")
                                   .arg(path, QFile(path).errorString()));
    return true;
}

} // namespace gp
