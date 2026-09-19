// SPDX-License-Identifier: Apache-2.0
#include "engines/ReviewSummaryWriter.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QSet>
#include <algorithm>
#include <exception>
#include <podofo/podofo.h>

#include "engines/SafeSave.h"

// Defensive: on Windows, windows.h (when reached transitively) defines
// `DrawText` as a macro for the Win32 API, colliding with PdfPainter::DrawText.
#ifdef DrawText
#undef DrawText
#endif

QString ReviewSummaryWriter::statusLabel(ReviewState s) {
    // The /State names of the PDF review model (ISO 32000-1 §12.5.6.4),
    // matching reviewStateToPdfName in PoDoFoBackend (Open == /State None).
    switch (s) {
    case ReviewState::Open:      return QStringLiteral("Open");
    case ReviewState::Accepted:  return QStringLiteral("Accepted");
    case ReviewState::Rejected:  return QStringLiteral("Rejected");
    case ReviewState::Completed: return QStringLiteral("Completed");
    case ReviewState::Cancelled: return QStringLiteral("Cancelled");
    default:                     return QStringLiteral("None");
    }
}

QList<ReviewSummaryWriter::RenderedEntry>
ReviewSummaryWriter::renderEntries(const QList<AnnotationItem>& comments) {
    QList<RenderedEntry> out;

    // Group by page, then order by author and creation date inside the page —
    // "grouped by page/author/status" per the research row, page first (the
    // reviewer reads the summary against the document).
    QMap<int, QList<const AnnotationItem*>> byPage;
    for (const auto& c : comments)
        byPage[c.pageIndex].append(&c);

    for (auto it = byPage.constBegin(); it != byPage.constEnd(); ++it) {
        QList<const AnnotationItem*> rows = it.value();
        std::stable_sort(rows.begin(), rows.end(),
                         [](const AnnotationItem* a, const AnnotationItem* b) {
                             if (a->author != b->author)
                                 return a->author < b->author;
                             return a->creationDate < b->creationDate;
                         });
        for (const AnnotationItem* c : rows) {
            RenderedEntry e;
            e.pageIndex = it.key();
            e.heading = QStringLiteral("[%1] %2 — %3")
                            .arg(statusLabel(c->reviewState),
                                 c->author.isEmpty() ? QStringLiteral("(no author)") : c->author,
                                 c->creationDate);
            e.body = c->text;
            out.append(e);
        }
    }
    return out;
}

QString ReviewSummaryWriter::proofSummaryLine(const QString& proofPackPath) {
    if (proofPackPath.isEmpty() || !QFileInfo::exists(proofPackPath))
        return QObject::tr("Redaction proof: not available "
                           "(no redaction-proof report beside the document)");

    QFile pack(proofPackPath);
    if (!pack.open(QIODevice::ReadOnly))
        return QObject::tr("Redaction proof: not available "
                           "(the redaction-proof report could not be read)");

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(pack.readAll(), &parseErr);
    pack.close();
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject())
        return QObject::tr("Redaction proof: not available "
                           "(the redaction-proof report is unreadable: %1)")
                   .arg(parseErr.errorString());

    const QJsonObject root = doc.object();
    // The pack schema is RedactionProof::Result::toJson's
    // ("glyphpdf-redaction-proof/1"): verdict / generated_at_utc / excisions.
    const QString verdict = root.value(QLatin1String("verdict")).toString();
    if (verdict.isEmpty())
        return QObject::tr("Redaction proof: not available "
                           "(unrecognized redaction-proof report format)");

    QString line = QStringLiteral("Redaction proof: %1 — %2 excision(s)")
                       .arg(verdict,
                            QString::number(
                                root.value(QLatin1String("excisions")).toArray().size()));
    const QString generatedAt =
        root.value(QLatin1String("generated_at_utc")).toString();
    if (!generatedAt.isEmpty())
        line += QStringLiteral(" — pack generated %1 (UTC)").arg(generatedAt);
    return line;
}

namespace {

constexpr double kPageW = 595.0;   // A4 portrait
constexpr double kPageH = 842.0;
constexpr double kMargin = 56.0;
constexpr double kBodySize = 10.0;
constexpr double kLineStep = 14.0;

// Rough character wrap at ~92 body characters — a printable line, drawn by
// simple DrawText calls (the same standard-14 drawing the Bates/header-footer
// writers use).
QStringList wrapText(const QString& text, int maxChars) {
    QStringList lines;
    QString normalized = text;
    normalized.replace(QLatin1Char('\r'), QString());
    for (const QString& para : normalized.split(QLatin1Char('\n'))) {
        if (para.isEmpty()) { lines.append(QString()); continue; }
        int start = 0;
        while (start < para.length()) {
            int len = qMin(maxChars, para.length() - start);
            if (start + len < para.length()) {
                // Back off to the last space so words are not split.
                const int lastSpace = para.lastIndexOf(QLatin1Char(' '), start + len);
                if (lastSpace > start)
                    len = lastSpace - start;
            }
            lines.append(para.mid(start, len));
            start += len;
            while (start < para.length() && para.at(start) == QLatin1Char(' '))
                ++start;
        }
    }
    return lines;
}

// Renders the summary into the document already attached to the painter's
// canvas stream: header block, redaction-proof availability line, table of
// entries, numbered entries grouped by page. Footers are stamped later in a
// second pass, once the page count is final.
void renderSummaryDocument(PoDoFo::PdfMemDocument& doc,
                           PoDoFo::PdfFont* regular,
                           PoDoFo::PdfFont* bold,
                           const QString& docTitle,
                           const QList<AnnotationItem>& comments,
                           const ReviewSummaryWriter::PrintOptions& options) {
    PoDoFo::PdfPainter painter;
    auto& firstPage = doc.GetPages().CreatePage(
        PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
    painter.SetCanvas(firstPage);

    double y = kPageH - kMargin;

    auto drawLine = [&](const QString& text, PoDoFo::PdfFont* font, double size) {
        painter.TextState.SetFont(*font, size);
        painter.DrawText(text.toUtf8().constData(), kMargin, y);
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
    drawLine(QObject::tr("Review Summary"), bold, 18);
    y -= 4;
    drawLine(QObject::tr("Document: %1").arg(docTitle), regular, kBodySize);
    drawLine(QObject::tr("Generated: %1")
                 .arg(QDateTime::currentDateTime().toString(Qt::ISODate)),
             regular, kBodySize);

    // Per-status totals — the "loop completed" data U07 started.
    QHash<QString, int> byStatus;
    QSet<QString> authors;
    for (const auto& c : comments) {
        byStatus[ReviewSummaryWriter::statusLabel(c.reviewState)] += 1;
        if (!c.author.isEmpty())
            authors.insert(c.author);
    }
    QStringList totals;
    const QStringList order = {QStringLiteral("Open"), QStringLiteral("Accepted"),
                               QStringLiteral("Rejected"), QStringLiteral("Completed"),
                               QStringLiteral("Cancelled"), QStringLiteral("None")};
    for (const QString& s : order)
        if (byStatus.value(s, 0) > 0)
            totals.append(QStringLiteral("%1 %2").arg(byStatus.value(s)).arg(s));
    drawLine(QObject::tr("Comments: %1").arg(comments.size())
                 + (totals.isEmpty() ? QString() : QStringLiteral(" (") + totals.join(QStringLiteral(", ")) + QStringLiteral(")")),
             regular, kBodySize);
    // Markup count by author — who carried the review.
    drawLine(QObject::tr("Distinct authors: %1").arg(authors.size()),
             regular, kBodySize);
    // Feature honesty: the redaction-proof section is LISTED — carried with
    // its verdict and the pack's own generation time when available, stated
    // as not available when not — never silently omitted.
    drawLine(ReviewSummaryWriter::proofSummaryLine(options.proofPackPath),
             regular, kBodySize);
    y -= 8;

    const QList<ReviewSummaryWriter::RenderedEntry> entries =
        ReviewSummaryWriter::renderEntries(comments);

    // ── Table of entries ─────────────────────────────────────────────────
    // Page groups with entry counts and the grand total, so a multi-sheet
    // summary is navigable before the reviewer commits to reading it.
    {
        QMap<int, int> countByPage;
        for (const auto& e : entries)
            countByPage[e.pageIndex] += 1;
        ensureRoom(2 * kLineStep);
        drawLine(QObject::tr("Contents"), bold, 13);
        drawLine(QObject::tr("%1 entries on %2 page(s)")
                     .arg(entries.size()).arg(countByPage.size()),
                 regular, kBodySize);
        for (auto it = countByPage.constBegin(); it != countByPage.constEnd(); ++it) {
            const int n = it.value();
            drawLine(QObject::tr("Page %1 — %2").arg(it.key() + 1)
                         .arg(n == 1 ? QObject::tr("1 entry")
                                     : QObject::tr("%1 entries").arg(n)),
                     regular, kBodySize);
        }
        y -= 4;
    }

    // ── Entries grouped by page ─────────────────────────────────────────
    int currentPage = -1;
    int entryNo = 0;
    for (const auto& e : entries) {
        if (e.pageIndex != currentPage) {
            currentPage = e.pageIndex;
            ensureRoom(3 * kLineStep);
            y -= 4;
            drawLine(QObject::tr("Page %1").arg(currentPage + 1), bold, 13);
        }
        ensureRoom(3 * kLineStep);
        ++entryNo;
        drawLine(QObject::tr("No. %1  %2").arg(entryNo).arg(e.heading), bold, kBodySize);
        for (const QString& line : wrapText(e.body, 92)) {
            ensureRoom(kLineStep);
            if (!line.isEmpty())
                drawLine(line, regular, kBodySize);
            else
                y -= kLineStep / 2;
        }
        y -= 4;
    }

    if (comments.isEmpty()) {
        drawLine(QObject::tr("No comments in the selected scope."), regular, kBodySize);
    }

    painter.FinishDrawing();

    // ── Sheet footers ────────────────────────────────────────────────────
    // Real pagination furniture: every sheet names the document and its own
    // position ("Page i of N"), stamped in a second pass once the page count
    // is final.
    const int totalPages = static_cast<int>(doc.GetPages().GetCount());
    if (totalPages > 0) {
        QString footerLeft = QObject::tr("Review Summary — %1").arg(docTitle);
        if (footerLeft.length() > 70)
            footerLeft = footerLeft.left(67) + QStringLiteral("...");
        PoDoFo::PdfPainter footerPainter;
        for (int i = 0; i < totalPages; ++i) {
            auto& page = doc.GetPages().GetPageAt(i);
            footerPainter.SetCanvas(page);
            footerPainter.TextState.SetFont(*regular, 8);
            footerPainter.DrawText(footerLeft.toUtf8().constData(),
                                   kMargin, kMargin / 2.0);
            const QString footerRight = QObject::tr("Page %1 of %2").arg(i + 1).arg(totalPages);
            footerPainter.DrawText(footerRight.toUtf8().constData(),
                                   kPageW - kMargin - 90, kMargin / 2.0);
            footerPainter.FinishDrawing();
        }
    }
}

} // namespace

bool ReviewSummaryWriter::write(const QString& outPath,
                                const QString& docTitle,
                                const QList<AnnotationItem>& comments,
                                QString* errorOut) {
    return writePrintable(outPath, docTitle, comments, PrintOptions(), errorOut);
}

bool ReviewSummaryWriter::writePrintable(const QString& outPath,
                                         const QString& docTitle,
                                         const QList<AnnotationItem>& comments,
                                         const PrintOptions& options,
                                         QString* errorOut) {
    // The destination is only ever replaced through the SafeSave transaction:
    // render into a unique OWNED candidate, then commit atomically. A failed
    // render or a refused commit leaves an existing destination byte-identical
    // — never overwritten outside the guard.
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

        renderSummaryDocument(doc, regular, bold, docTitle, comments, options);

        doc.Save(candidate.toUtf8().constData());
    } catch (const std::exception& e) {
        QFile::remove(candidate);   // the candidate is ours — clean it up
        if (errorOut) *errorOut = QString::fromLatin1(e.what());
        return false;
    } catch (...) {
        QFile::remove(candidate);
        if (errorOut) *errorOut = QObject::tr("Unknown error writing the review summary.");
        return false;
    }

    // Validate the candidate: an empty artifact never reaches the destination.
    const QFileInfo candidateInfo(candidate);
    if (!candidateInfo.exists() || candidateInfo.size() <= 0) {
        QFile::remove(candidate);
        if (errorOut)
            *errorOut = QObject::tr("The review summary could not be rendered.");
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
