// SPDX-License-Identifier: Apache-2.0
// harness_batchpreset.cpp — W1 sweep S2: BatchPreset JSON codec + the naming
// RENDER path.
//
// Surfaces under test (src/core/BatchPreset.cpp):
//   BatchPresetCodec::parse(QByteArray)      — the V1–V9 fail-closed decode
//   BatchPresetSchema::resolveNaming(...)    — token rendering: {basename},
//                                              {preset}, {n}, {date}
//   BatchPresetSchema::idFromName(...)       — slug identity generation
//
// Property oracles:
//   P1 rendered-name containment (header contract: "a FILE NAME — no directory
//      components"): for a hostile basename set (traversal ..\..\, drive
//      letters, reserved device names CON/NUL/AUX/COM1/LPT1, extreme lengths,
//      unicode) the RENDERED name must contain no path separator, no drive
//      letter, no "..", no reserved device name — else PATH_ESCAPE.
//   P2 template literal containment: the template itself is a filename, so a
//      template that resolves (parse accepts it) must render a contained name.
//      A preset whose output.naming renders outside the output dir ⇒ finding
//      (the consumer joins it with QDir(outDir).filePath(name) — BatchMode
//      confirmOverwrite/commit path).
//   P3 codec round-trip: parse(serialize(parse(x))) must re-accept with the
//      same step count.
//   P4 idFromName output must always match ^[a-z0-9-]{1,64}$ (or "preset").
//
// Deterministic driver (sweep_common.h): campaign / one / materialize modes.
#include <cstdio>
#include <string>

#include <QDate>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include "core/BatchPreset.h"
#include "sweep_common.h"

using gp::BatchPreset;
namespace BatchPresetSchema = gp::BatchPresetSchema;
namespace BatchPresetCodec = gp::BatchPresetCodec;

namespace {

// Hostile token replacement values (plan §3.6: no separator may be smuggled
// through a token) — the render path must sanitize every one of these.
const std::string kHostileBasenames[] = {
    "plain",
    "..\\..\\evil",
    "../../etc/passwd",
    "C:\\Windows\\evil",
    "C:/drive-escape",
    "CON",
    "NUL.pdf",
    "AUX",
    "COM1",
    "LPT1",
    "con",
    "..",
    ".",
    "",
    "a\\b/c:d*e?f\"g<h>i|j",
    "\xE2\x80\xAErtlo.exe",          // U+202E RTL override smuggle
    std::string(4000, 'Q'),
    "\xF0\x9F\x98\x80-emoji",
    "trailing.dots...",
    " spaces  inside ",
};

bool reservedDeviceName(const QString& name) {
    // Reserved DOS device names, with or without extension (case-insensitive).
    static const QStringList reserved = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
    const QString first = name.section('.', 0, 0).toUpper();
    return reserved.contains(first);
}

bool pathStructureEscapes(const QString& name) {
    if (name.isEmpty()) return true;                        // never empty
    if (name.size() > 240) return true;                     // Win32 path sanity
    if (name.contains('/') || name.contains('\\')) return true;
    if (name.contains(':')) return true;                    // drive letters / ADS
    const QStringList parts = name.split('.');
    for (const QString& p : parts)
        if (p == QLatin1String("..")) return true;          // no .. components
    return false;
}

// The fixed hostile matrix: 4 templates x every hostile basename. This is a
// CODE-DEFECT probe (input-independent) — run via the `probes` mode, not
// per-input, so the campaign oracle below stays mutation-driven.
// Returns 0 = clean, 42 = at least one violation (printed).
int probeMatrix() {
    int violations = 0;
    const QDate runDate(2026, 9, 20);
    const std::pair<const char*, const char*> templates[] = {
        {"{basename}.pdf", "token-values"},
        {"..\\..\\{basename}.pdf", "template-dots"},
        {"C:\\{basename}.pdf", "template-drive"},
        {"{basename}/{n}.pdf", "template-separator"},
    };
    for (const auto& tmplTag : templates) {
        const QString tmpl = QString::fromUtf8(tmplTag.first);
        for (const std::string& raw : kHostileBasenames) {
            QString out, err;
            if (!BatchPresetSchema::resolveNaming(tmpl, QString::fromStdString(raw),
                                                  "preset-id", 7, runDate,
                                                  &out, &err))
                continue;   // refused template — fine
            const char* kind = nullptr;
            if (pathStructureEscapes(out)) kind = "P1_RENDER_PATH_ESCAPE";
            else if (reservedDeviceName(out)) kind = "P1_RESERVED_DEVICE_NAME";
            if (kind) {
                printf("PROBE [%s] basename=<%s> rendered=<%s> kind=%s\n",
                       tmplTag.second, raw.c_str(), out.toStdString().c_str(), kind);
                ++violations;
            }
        }
    }
    printf("PROBE-MATRIX violations=%d\n", violations);
    return violations ? 42 : 0;
}

const char* runOne(const std::vector<uint8_t>& data) {
    const QByteArray bytes(reinterpret_cast<const char*>(data.data()),
                           (qsizetype)data.size());

    BatchPreset p;
    QString err;
    if (!BatchPresetCodec::parse(bytes, &p, &err))
        return "REJECT";   // the diagnostic text is the V1–V9 surface; decode refused

    // Accepted preset — P2: a preset-borne template must render contained
    // even for a benign basename (the template's own literals are the risk).
    if (!p.outputNaming.isEmpty()) {
        QString out, nerr;
        if (BatchPresetSchema::resolveNaming(p.outputNaming, "benign", p.id, 1,
                                             QDate(2026, 9, 20), &out, &nerr)) {
            const char* kind = pathStructureEscapes(out) ? "P2_PRESET_NAMING_PATH_ESCAPE"
                              : reservedDeviceName(out)  ? "P2_PRESET_NAMING_RESERVED_DEVICE"
                              : nullptr;
            if (kind) {
                static std::string verdict;
                verdict = std::string("FINDING ") + kind + " template=<"
                        + p.outputNaming.toStdString() + "> rendered=<"
                        + out.toStdString() + ">";
                return verdict.c_str();
            }
        }
    }

    // P3: canonical round-trip.
    const QByteArray ser = BatchPresetCodec::serialize(p);
    BatchPreset p2;
    QString err2;
    if (!BatchPresetCodec::parse(ser, &p2, &err2)
        || p2.steps.size() != p.steps.size())
        return "FINDING P3_ROUNDTRIP_FAIL";

    // P4: slug identity grammar.
    static const QRegularExpression idRx(QStringLiteral("^[a-z0-9-]{1,64}$"));
    const QString slug = BatchPresetSchema::idFromName(p.name);
    if (!idRx.match(slug).hasMatch())
        return "FINDING P4_IDFROMNAME_GRAMMAR_BROKEN";

    return "OK";
}

}  // namespace

int main(int argc, char** argv) {
    // `probes` mode: the input-independent hostile template x basename matrix.
    if (argc == 2 && std::string(argv[1]) == "probes") return probeMatrix();
    return sweep::driverMain(argc, argv, runOne);
}
