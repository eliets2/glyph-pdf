// SPDX-License-Identifier: Apache-2.0
#include "engines/A11yReportWriter.h"

#include "engines/SafeSave.h"
#include "engines/Standard14Text.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <podofo/podofo.h>

// Defensive: on Windows, windows.h (when reached transitively) defines
// `DrawText` as a macro for the Win32 API, colliding with PdfPainter::DrawText.
#ifdef DrawText
#undef DrawText
#endif

#include "engines/ConversionManager.h"

// ── CSV ──────────────────────────────────────────────────────────────────────
// ONE cell-escaping contract, DELEGATED (r3-api harmonization): csvCell is
// ConversionManager::csvCell — csvFormulaSafeCell (PGR-16/M3 formula-injection
// hardening, layer 1) + RFC-4180 quote-when-needed (layer 2) — the single
// composition shared with the conversion exporter and
// CommentsWidget::csvEscapeField, so the three exporters cannot drift at the
// composition layer either. Kept as a member so the writer's contract stays
// nameable and directly testable.
namespace gp {

QString A11yReportWriter::csvCell(const QString& raw) {
    return ConversionManager::csvCell(raw);
}

QString A11yReportWriter::severityLabel(gp::A11ySeverity s) {
    switch (s) {
        case gp::A11ySeverity::High:   return QStringLiteral("HIGH");
        case gp::A11ySeverity::Medium: return QStringLiteral("MEDIUM");
        case gp::A11ySeverity::Low:    return QStringLiteral("LOW");
    }
    return {};
}

QString A11yReportWriter::disclosureLine() {
    // The standing honesty contract of the checker (AccessibilityChecker.h /
    // the panel's disclosure box), carried into every artifact. Deliberately
    // free of the words "accessible"/"conformant" — the artifact never
    // words a conformance claim.
    return QObject::tr(
        "Disclosure: detection only — this checker never certifies PDF/UA. "
        "Zero findings are not a PDF/UA verdict (content tagging is not "
        "checked); per-object checks are bounded samples and truncation is "
        "disclosed via totals.");
}

QString A11yReportWriter::verdictLine(const gp::A11yReport& report) {
    if (!report.loadOk) {
        // A failed scan is exported AS failed — the checker produced NO
        // findings and none may be invented. Never greenwash into "clean".
        const QString reason =
            report.loadError.isEmpty() ? QObject::tr("unknown error")
                                       : report.loadError;
        return QObject::tr("Scan failed: %1 — no findings are available "
                           "(nothing was checked)")
            .arg(reason);
    }
    const int n = report.findings.size();
    if (n == 0) {
        // The panel's exact zero-findings wording — never "accessible".
        return QObject::tr("No gaps found by these checks. This is not a "
                           "PDF/UA verdict — content tagging is not checked.");
    }
    QString verdict = QObject::tr("%1 gap(s) found").arg(n);
    if (report.truncated())
        verdict += QObject::tr(" — report truncated");
    return verdict;
}

QString A11yReportWriter::csvPayload(const gp::A11yReport& report) {
    // Block 1 — the report facts (key/value), the machine-readable verdict:
    // scan status (a failed scan says so), the tagged state, counts by
    // severity, the bounded-sample totals and the honest verdict + standing
    // disclosure. Block 2 — the findings table. CRLF per RFC-4180.
    const QString nl = QStringLiteral("\r\n");
    QStringList lines;
    lines << csvCell(QStringLiteral("Format"))
          + QLatin1Char(',')
          + csvCell(QStringLiteral("glyphpdf-a11y-report/1"));
    lines << csvCell(QStringLiteral("Generated")) + QLatin1Char(',')
          + csvCell(QDateTime::currentDateTime().toString(Qt::ISODate));
    lines << csvCell(QStringLiteral("Document")) + QLatin1Char(',')
          + csvCell(report.path);
    lines << csvCell(QStringLiteral("Scan status")) + QLatin1Char(',')
          + csvCell(report.loadOk ? QStringLiteral("ok")
                                  : QStringLiteral("failed"));
    if (!report.loadOk)
        lines << csvCell(QStringLiteral("Load error")) + QLatin1Char(',')
              + csvCell(report.loadError);
    lines << csvCell(QStringLiteral("Tagged")) + QLatin1Char(',')
          + csvCell(report.tagged ? QStringLiteral("true")
                                  : QStringLiteral("false"));
    lines << csvCell(QStringLiteral("Finding count")) + QLatin1Char(',')
          + csvCell(QString::number(report.findings.size()));
    int high = 0, medium = 0, low = 0;
    for (const gp::A11yFinding& f : report.findings) {
        switch (f.severity) {
            case gp::A11ySeverity::High:   ++high;   break;
            case gp::A11ySeverity::Medium: ++medium; break;
            case gp::A11ySeverity::Low:    ++low;    break;
        }
    }
    lines << csvCell(QStringLiteral("High severity")) + QLatin1Char(',')
          + csvCell(QString::number(high));
    lines << csvCell(QStringLiteral("Medium severity")) + QLatin1Char(',')
          + csvCell(QString::number(medium));
    lines << csvCell(QStringLiteral("Low severity")) + QLatin1Char(',')
          + csvCell(QString::number(low));
    lines << csvCell(QStringLiteral("Images reported")) + QLatin1Char(',')
          + csvCell(QStringLiteral("%1 of %2")
                        .arg(report.imagesReported)
                        .arg(report.imagesTotal));
    lines << csvCell(QStringLiteral("Fields reported")) + QLatin1Char(',')
          + csvCell(QStringLiteral("%1 of %2")
                        .arg(report.fieldsReported)
                        .arg(report.fieldsTotal));
    lines << csvCell(QStringLiteral("Truncated")) + QLatin1Char(',')
          + csvCell(report.truncated() ? QStringLiteral("true")
                                       : QStringLiteral("false"));
    lines << csvCell(QStringLiteral("Verdict")) + QLatin1Char(',')
          + csvCell(verdictLine(report));
    lines << csvCell(QStringLiteral("Disclosure")) + QLatin1Char(',')
          + csvCell(disclosureLine());

    lines << QString();   // blank separator between the two blocks

    lines << QStringList{csvCell(QStringLiteral("Severity")),
                         csvCell(QStringLiteral("Check")),
                         csvCell(QStringLiteral("Page")),
                         csvCell(QStringLiteral("Target")),
                         csvCell(QStringLiteral("Where")),
                         csvCell(QStringLiteral("Detail"))}
              .join(QLatin1Char(','));
    // Findings in the checker's own order (document-level first); pages are
    // 1-based to match what the reviewer sees (the CommentsWidget CSV rule).
    for (const gp::A11yFinding& f : report.findings) {
        QStringList row;
        row << csvCell(severityLabel(f.severity));
        row << csvCell(f.checkId);
        row << csvCell(f.page >= 0 ? QString::number(f.page + 1) : QString());
        row << csvCell(f.targetId);
        row << csvCell(f.where);
        row << csvCell(f.whyNot);
        lines << row.join(QLatin1Char(','));
    }
    return lines.join(nl) + nl;
}

bool A11yReportWriter::writeCsv(const QString& outPath,
                                const gp::A11yReport& report,
                                QString* errorOut) {
    QFile file(outPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = QObject::tr("Cannot open %1 for writing")
                            .arg(QDir::toNativeSeparators(outPath));
        return false;
    }
    const QByteArray payload = csvPayload(report).toUtf8();
    if (file.write(payload) != payload.size()) {
        file.close();
        if (errorOut) *errorOut = QObject::tr("Short write on %1")
                                      .arg(QDir::toNativeSeparators(outPath));
        return false;
    }
    file.close();
    return true;
}

// ── PDF ──────────────────────────────────────────────────────────────────────

} // namespace gp

namespace {

constexpr double kPageW = 595.0;   // A4 portrait
constexpr double kPageH = 842.0;
constexpr double kMargin = 56.0;
constexpr double kBodySize = 10.0;
constexpr double kLineStep = 14.0;

// The same standard-14 toolkit ReviewSummaryWriter draws through
// (engines/Standard14Text.h): sanitizeForStandard14 at the one draw
// boundary, char-counted wrapText.
using gp::sanitizeForStandard14;
using gp::wrapText;

void renderSummaryDocument(PoDoFo::PdfMemDocument& doc,
                           PoDoFo::PdfFont* regular,
                           PoDoFo::PdfFont* bold,
                           const QString& docTitle,
                           const gp::A11yReport& report) {
    PoDoFo::PdfPainter painter;
    auto& firstPage = doc.GetPages().CreatePage(
        PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
    painter.SetCanvas(firstPage);

    double y = kPageH - kMargin;

    // W1-04: substitution accounting for the honesty note — every draw goes
    // through drawLine (or the sanitized footers in the second pass).
    int substitutedTotal = 0;

    auto drawLine = [&](const QString& text, PoDoFo::PdfFont* font, double size) {
        const gp::WinAnsiSafeString safe = sanitizeForStandard14(text);
        substitutedTotal += safe.substituted;
        painter.TextState.SetFont(*font, size);
        painter.DrawText(safe.text.toUtf8().constData(), kMargin, y);
        y -= kLineStep;
    };
    auto ensureRoom = [&](double needed) {
        if (y - needed < kMargin) {
            painter.FinishDrawing();
            auto& p = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            painter.SetCanvas(p);
            y = kPageH - kMargin;
        }
    };

    // ── Header block ────────────────────────────────────────────────────
    drawLine(QObject::tr("Accessibility Check Summary"), bold, 18);
    y -= 4;
    drawLine(QObject::tr("Document: %1").arg(docTitle), regular, kBodySize);
    drawLine(QObject::tr("Generated: %1")
                 .arg(QDateTime::currentDateTime().toString(Qt::ISODate)),
             regular, kBodySize);

    // The honest verdict — a failed scan says so, zero findings are worded
    // as "no gaps found by these checks", never as conformance.
    drawLine(QObject::tr("Verdict: %1").arg(gp::A11yReportWriter::verdictLine(report)),
             bold, kBodySize);

    // Capability degradations, disclosed on the page:
    //   * the tagged state (what the document carries),
    //   * the bounded per-object samples with their totals,
    //   * the standing disclosure line.
    drawLine(QObject::tr("Tagged: %1")
                 .arg(report.tagged ? QObject::tr("yes (carries /StructTreeRoot)")
                                    : QObject::tr("no (no structure tree)")),
             regular, kBodySize);
    int high = 0, medium = 0, low = 0;
    for (const gp::A11yFinding& f : report.findings) {
        switch (f.severity) {
            case gp::A11ySeverity::High:   ++high;   break;
            case gp::A11ySeverity::Medium: ++medium; break;
            case gp::A11ySeverity::Low:    ++low;    break;
        }
    }
    drawLine(QObject::tr("Findings: %1 (%2 HIGH, %3 MEDIUM, %4 LOW)")
                 .arg(report.findings.size())
                 .arg(high)
                 .arg(medium)
                 .arg(low),
             regular, kBodySize);
    drawLine(QObject::tr("Images without /Alt reported: %1 of %2")
                 .arg(report.imagesReported)
                 .arg(report.imagesTotal),
             regular, kBodySize);
    drawLine(QObject::tr("Fields without /TU reported: %1 of %2")
                 .arg(report.fieldsReported)
                 .arg(report.fieldsTotal),
             regular, kBodySize);
    for (const QString& line : wrapText(gp::A11yReportWriter::disclosureLine(), 92)) {
        ensureRoom(kLineStep);
        if (!line.isEmpty()) drawLine(line, regular, kBodySize);
    }
    y -= 8;

    // ── Findings grouped by severity (HIGH → MEDIUM → LOW), in the
    //    checker's own order inside each band ─────────────────────────────
    const gp::A11ySeverity bands[3] = {gp::A11ySeverity::High,
                                       gp::A11ySeverity::Medium,
                                       gp::A11ySeverity::Low};
    int entryNo = 0;
    for (const gp::A11ySeverity band : bands) {
        bool bandHeadingDrawn = false;
        for (const gp::A11yFinding& f : report.findings) {
            if (f.severity != band) continue;
            if (!bandHeadingDrawn) {
                bandHeadingDrawn = true;
                ensureRoom(3 * kLineStep);
                y -= 4;
                drawLine(gp::A11yReportWriter::severityLabel(band), bold, 13);
            }
            ensureRoom(3 * kLineStep);
            ++entryNo;
            // `where` is the checker's human-readable target ("page 3 ·
            // image /Im0", "document") — carried verbatim.
            drawLine(QObject::tr("No. %1 [%2] %3")
                         .arg(entryNo)
                         .arg(f.checkId, f.where),
                     bold, kBodySize);
            for (const QString& line : wrapText(f.whyNot, 92)) {
                ensureRoom(kLineStep);
                if (!line.isEmpty()) drawLine(line, regular, kBodySize);
            }
            y -= 4;
        }
    }

    // W1-04 honesty note: content the printable font cannot carry was
    // substituted — disclose it in the artifact instead of silently
    // mangling (or aborting the whole export over one string).
    if (substitutedTotal > 0) {
        ensureRoom(kLineStep);
        drawLine(QObject::tr("Note: %1 character(s) could not be rendered in "
                             "the printable summary font and are shown as "
                             "\"?\".")
                     .arg(substitutedTotal),
                 regular, kBodySize);
    }

    painter.FinishDrawing();

    // ── Sheet footers ────────────────────────────────────────────────────
    // Real pagination furniture: every sheet names the artifact and its own
    // position ("Page i of N"), stamped in a second pass once the page
    // count is final (the ReviewSummaryWriter idiom).
    const int totalPages = static_cast<int>(doc.GetPages().GetCount());
    if (totalPages > 0) {
        QString footerLeft = QObject::tr("Accessibility Summary — %1")
                                 .arg(docTitle);
        if (footerLeft.length() > 70)
            footerLeft = footerLeft.left(67) + QStringLiteral("...");
        PoDoFo::PdfPainter footerPainter;
        for (int i = 0; i < totalPages; ++i) {
            auto& page = doc.GetPages().GetPageAt(i);
            footerPainter.SetCanvas(page);
            footerPainter.TextState.SetFont(*regular, 8);
            // W1-04: the footer names the document — same sanitizing rule
            // as the body, so a hostile/non-Latin title can neither abort
            // the export nor corrupt the footer stream.
            const gp::WinAnsiSafeString footerLeftSafe =
                sanitizeForStandard14(footerLeft);
            footerPainter.DrawText(footerLeftSafe.text.toUtf8().constData(),
                                   kMargin, kMargin / 2.0);
            const QString footerRight =
                QObject::tr("Page %1 of %2").arg(i + 1).arg(totalPages);
            footerPainter.DrawText(footerRight.toUtf8().constData(),
                                   kPageW - kMargin - 90, kMargin / 2.0);
            footerPainter.FinishDrawing();
        }
    }
}

} // namespace

namespace gp {

bool A11yReportWriter::writePdf(const QString& outPath,
                                const QString& docTitle,
                                const gp::A11yReport& report,
                                QString* errorOut) {
    // The destination is only ever replaced through the SafeSave transaction:
    // render into a unique OWNED candidate, then commit atomically. A failed
    // render or a refused commit leaves an existing destination byte-identical
    // — never overwritten outside the guard (the ReviewSummaryWriter contract).
    QString candidate;
    QString candidateErr;
    if (!gp::SafeSave::makeUniqueCandidate(&candidate, &candidateErr)) {
        if (errorOut) *errorOut = candidateErr;
        return false;
    }

    try {
        PoDoFo::PdfMemDocument doc;

        PoDoFo::PdfFont* regular = &doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        PoDoFo::PdfFont* bold = &doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::HelveticaBold);

        renderSummaryDocument(doc, regular, bold, docTitle, report);

        doc.Save(candidate.toUtf8().constData());
    } catch (const std::exception& e) {
        QFile::remove(candidate);   // the candidate is ours — clean it up
        if (errorOut) *errorOut = QString::fromLatin1(e.what());
        return false;
    } catch (...) {
        QFile::remove(candidate);
        if (errorOut)
            *errorOut = QObject::tr("Unknown error writing the accessibility "
                                    "summary.");
        return false;
    }

    // Validate the candidate: an empty artifact never reaches the destination.
    const QFileInfo candidateInfo(candidate);
    if (!candidateInfo.exists() || candidateInfo.size() <= 0) {
        QFile::remove(candidate);
        if (errorOut)
            *errorOut = QObject::tr("The accessibility summary could not be "
                                    "rendered.");
        return false;
    }

    QString commitErr;
    const bool committed =
        gp::SafeSave::commitFileToDestination(candidate, outPath, &commitErr);
    QFile::remove(candidate);   // the candidate is ours on EVERY outcome
    if (!committed) {
        if (errorOut) *errorOut = commitErr;
        return false;
    }
    return true;
}

} // namespace gp
