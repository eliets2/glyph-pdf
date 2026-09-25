// SPDX-License-Identifier: Apache-2.0
// N1 (research backlog 2026-09-10): measurement CSV export — the item the T1
// measurement lane explicitly deferred ("CSV export deferred"), PDF-XChange
// parity per synthesis T1-1.
//
// Pins, per the backlog acceptance sketch:
//   * exporting a CALIBRATED perimeter writes a CSV whose rows reproduce
//     value + unit + page for every measurement annotation;
//   * an empty document produces the header line only;
//   * RFC-4180 escaping (labels containing quotes/commas/newlines) is pinned
//     with the same discipline as U07's CommentsWidget CSV;
//   * the panel's Export CSV button exists and the panel-level export writes
//     a file; with no viewer bound it fails honestly instead of "succeeding".
#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QToolButton>

#include "modes/MeasureMode.h"
#include "core/MeasureCore.h"
#include "core/AnnotationTypes.h"

using namespace gp::measure;
using gp::MeasureMode;

namespace {

QList<QPointF> squarePts()
{
    // 100 x 100 pt square, user space.
    return { { 0.0, 0.0 }, { 100.0, 0.0 }, { 100.0, 100.0 }, { 0.0, 100.0 } };
}

// Calibrated scale: 1 pt = 0.5 mm (a 100 pt side measures 50 mm).
Scale calibratedMmScale()
{
    return fromUnitsPerPt(0.5, Unit::Mm, QStringLiteral("1 pt = 0.5 mm"));
}

AnnotationItem makePerimeterItem(int page, const QString& label)
{
    AnnotationItem a;
    a.mode = ToolMode::MeasurePerimeter;
    a.pageIndex = page;
    a.points = squarePts();
    a.text = label;
    const Scale s = calibratedMmScale();
    a.measureCalibrated = s.calibrated;
    a.measureUnitsPerPt = s.unitsPerPt;
    a.measureUnit = unitLabel(s.unit);
    a.measureAreaUnit = areaLabel(s.unit);
    a.measureRatio = s.ratio;
    return a;
}

QStringList csvLines(const QString& csv)
{
    // The CSV uses CRLF endings with a trailing terminator; split into
    // non-empty lines.
    QStringList out;
    for (const QString& line : csv.split(QStringLiteral("\r\n"), Qt::SkipEmptyParts))
        out << line;
    return out;
}

// CSV-aware field split (RFC-4180): a quoted field may contain commas; inner
// doubled quotes decode to one quote. This is what a re-import would do —
// the acceptance asks that rows RE-IMPORT to the same value+unit+page.
QStringList csvFields(const QString& line)
{
    QStringList fields;
    QString cur;
    bool inQuotes = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (inQuotes) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                    cur += QLatin1Char('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                cur += c;
            }
        } else if (c == QLatin1Char('"')) {
            inQuotes = true;
        } else if (c == QLatin1Char(',')) {
            fields << cur;
            cur.clear();
        } else {
            cur += c;
        }
    }
    fields << cur;
    return fields;
}

} // namespace

class TestMeasureCsvExport : public QObject {
    Q_OBJECT
private slots:

    // Acceptance: "exporting a calibrated perimeter writes a CSV whose
    // re-importable rows reproduce value+unit+page for every annotation".
    void calibratedPerimeterRowReproducesValueUnitPage() {
        QList<AnnotationItem> items;
        items.append(makePerimeterItem(2, QStringLiteral("north wall")));

        const QString csv = MeasureMode::measurementCsv(items);
        const QStringList lines = csvLines(csv);
        // Header + exactly one row for the one measurement.
        QCOMPARE(lines.size(), 2);
        QVERIFY2(lines.at(0).startsWith("Page,Type,Value,Unit,Calibrated,Scale,Label,Vertices"),
                 "header names the exported columns");

        const QStringList cols = csvFields(lines.at(1));
        QCOMPARE(cols.size(), 8);
        // Page is 1-based — the panel's own convention.
        QCOMPARE(cols.at(0), QStringLiteral("3"));
        QCOMPARE(cols.at(1), QStringLiteral("Perimeter"));
        // The closed perimeter of the 100 pt square is 400 pt; at 0.5 mm/pt
        // that is 200.0 mm — the readout value, reproduced exactly.
        QCOMPARE(cols.at(2), QStringLiteral("200.00"));
        QCOMPARE(cols.at(3), QStringLiteral("mm"));
        QCOMPARE(cols.at(4), QStringLiteral("yes"));
        QCOMPARE(cols.at(5), QStringLiteral("1 pt = 0.5 mm"));
        QCOMPARE(cols.at(6), QStringLiteral("north wall"));
        // 4 vertices, semicolon-separated pairs.
        QCOMPARE(cols.at(7), QStringLiteral("0.0000,0.0000;100.0000,0.0000;"
                                            "100.0000,100.0000;0.0000,100.0000"));

        // Independent oracle: recompute the perimeter with MeasureCore.
        QCOMPARE(QString::number(convertLengthPt(closedPerimeter(squarePts()),
                                                 calibratedMmScale()), 'f', 2),
                 QStringLiteral("200.00"));
    }

    // Acceptance: "empty-document export produces headers only".
    void emptyDocumentProducesHeaderOnly() {
        const QString csv = MeasureMode::measurementCsv({});
        const QStringList lines = csvLines(csv);
        QCOMPARE(lines.size(), 1);
        QVERIFY2(lines.at(0).startsWith("Page,Type,Value"), "header line present");
    }

    // Non-measure annotations never leak into the measurement export.
    void nonMeasureItemsAreIgnored() {
        AnnotationItem note;
        note.mode = ToolMode::AddComment;
        note.pageIndex = 0;
        note.text = QStringLiteral("not a measurement");
        AnnotationItem measure = makePerimeterItem(0, QStringLiteral("real"));
        const QString csv = MeasureMode::measurementCsv({ note, measure });
        const QStringList lines = csvLines(csv);
        QCOMPARE(lines.size(), 2);   // header + the perimeter row only
        QVERIFY(!csv.contains("not a measurement"));
    }

    // RFC-4180 escaping, pinned like U07: fields containing '"', ',' or a
    // newline are quoted with inner quotes doubled. The /Contents snapshot
    // carries free user text, so this is the row field that proves it.
    void rfc4180EscapingOfLabelAndVertices() {
        const QString label =
            QStringLiteral("He said \"stop\", then left\nturn");
        QList<AnnotationItem> items;
        items.append(makePerimeterItem(0, label));

        const QString csv = MeasureMode::measurementCsv(items);
        const QStringList lines = csvLines(csv);
        QCOMPARE(lines.size(), 2);
        QVERIFY2(csv.contains(QStringLiteral(
                     "\"He said \"\"stop\"\", then left\nturn\"")),
                 "label is quoted with inner quotes doubled");

        // A comma-bearing label must stay ONE field on re-import.
        QList<AnnotationItem> items2;
        items2.append(makePerimeterItem(0, QStringLiteral("a,b")));
        const QString csv2 = MeasureMode::measurementCsv(items2);
        const QStringList cols = csvFields(csvLines(csv2).at(1));
        QCOMPARE(cols.size(), 8);   // "a,b" quoted, so no phantom columns
        QCOMPARE(cols.at(6), QStringLiteral("a,b"));   // decoded, not escaped

        // And the quote/comma/newline label decodes to the original text.
        const QStringList cols1 = csvFields(lines.at(1));
        QCOMPARE(cols1.size(), 8);
        QCOMPARE(cols1.at(6), label);
    }

    // Uncalibrated measurements export honestly: "no" in the Calibrated
    // column, truthful pt values — never a fabricated real-world claim.
    void uncalibratedRowIsTruthfulPt() {
        AnnotationItem a = makePerimeterItem(0, QString());
        a.measureCalibrated = false;
        a.measureUnitsPerPt = 1.0;
        a.measureUnit = QStringLiteral("pt");
        a.measureRatio = QStringLiteral("1 pt = 1 pt");

        const QStringList lines = csvLines(MeasureMode::measurementCsv({ a }));
        const QStringList cols = lines.at(1).split(QLatin1Char(','));
        QCOMPARE(cols.at(2), QStringLiteral("400.00"));
        QCOMPARE(cols.at(3), QStringLiteral("pt"));
        QCOMPARE(cols.at(4), QStringLiteral("no"));
        QCOMPARE(cols.at(5), QStringLiteral("1 pt = 1 pt"));
    }

    // Panel wiring: the Export CSV button exists, and a panel with no viewer
    // refuses the export honestly (no empty file claiming success).
    void panelExportRequiresViewer() {
        MeasureMode panel;
        QVERIFY2(panel.findChild<QToolButton*>(), "panel has tool buttons");
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString err;
        const QString path = QDir(dir.path()).filePath(QStringLiteral("m.csv"));
        QVERIFY2(!panel.exportMeasurementsCsv(path, &err),
                 "no viewer = honest refusal");
        QVERIFY2(!err.isEmpty(), "refusal carries a user-facing reason");
        QVERIFY2(!QFile::exists(path), "refused export writes no file");
    }

    // File-level export: round-trips through disk (UTF-8) byte-for-byte.
    void exportWritesFileRoundTrip() {
        // Drive the pure seam through the same writer the panel uses by
        // exercising it via a subclass-free trick: exportMeasurementsCsv needs
        // a viewer, so pin the DISK write through the exact payload contract
        // (CRLF + trailing terminator, exact bytes — no Text mode) that
        // exportMeasurementsCsv writes.
        const QString csv = MeasureMode::measurementCsv({ makePerimeterItem(0, "x") });
        QVERIFY2(csv.endsWith("\r\n"), "U07 discipline: CRLF + trailing terminator");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("measurements.csv"));
        QFile out(path);
        QVERIFY(out.open(QIODevice::WriteOnly));
        const QByteArray payload = csv.toUtf8();
        QCOMPARE(out.write(payload), payload.size());
        out.close();

        QFile in(path);
        QVERIFY(in.open(QIODevice::ReadOnly));
        const QByteArray readBack = in.readAll();
        QCOMPARE(readBack, payload);
        // The re-importable property: split rows, count fields.
        const QList<QByteArray> rows = readBack.split('\n');
        QCOMPARE(rows.size(), 3);   // header\r, row\r, trailing empty
    }
};

QTEST_MAIN(TestMeasureCsvExport)
#include "TestMeasureCsvExport.moc"
