// SPDX-License-Identifier: Apache-2.0
#include "engines/ReviewSummaryWriter.h"

#include <QDateTime>
#include <QHash>
#include <QMap>
#include <algorithm>
#include <exception>
#include <podofo/podofo.h>

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

} // namespace

bool ReviewSummaryWriter::write(const QString& outPath,
                                const QString& docTitle,
                                const QList<AnnotationItem>& comments,
                                QString* errorOut) {
    try {
        PoDoFo::PdfMemDocument doc;

        PoDoFo::PdfFont* regular = &doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        PoDoFo::PdfFont* bold = &doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::HelveticaBold);

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
        for (const auto& c : comments)
            byStatus[statusLabel(c.reviewState)] += 1;
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
        y -= 8;

        // ── Entries grouped by page ─────────────────────────────────────────
        const QList<RenderedEntry> entries = renderEntries(comments);
        int currentPage = -1;
        for (const auto& e : entries) {
            if (e.pageIndex != currentPage) {
                currentPage = e.pageIndex;
                ensureRoom(3 * kLineStep);
                y -= 4;
                drawLine(QObject::tr("Page %1").arg(currentPage + 1), bold, 13);
            }
            ensureRoom(3 * kLineStep);
            drawLine(e.heading, bold, kBodySize);
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
        doc.Save(outPath.toUtf8().constData());
        return true;
    } catch (const std::exception& e) {
        if (errorOut) *errorOut = QString::fromLatin1(e.what());
        return false;
    } catch (...) {
        if (errorOut) *errorOut = QObject::tr("Unknown error writing the review summary.");
        return false;
    }
}
