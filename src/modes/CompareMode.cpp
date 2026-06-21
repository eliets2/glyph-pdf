// SPDX-License-Identifier: Apache-2.0
#include "CompareMode.h"
#include "util/GpTheme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QColor>
#include <QTextBrowser>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>
#include <QDate>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextStream>
#include "ui/CompareWidget.h"

namespace gp {

CompareMode::CompareMode(QWidget* parent) : QWidget(parent) {
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0,0,0,0); col->setSpacing(0);

    // toolbar
    auto* tb = new QFrame;
    tb->setProperty("role","modeToolbar");
    tb->setFixedHeight(Theme::ToolbarH);
    auto* hrow = new QHBoxLayout(tb);
    hrow->setContentsMargins(10,0,10,0); hrow->setSpacing(6);
    auto mono = [](const QString& s){ auto* l = new QLabel(s); l->setProperty("mono",true); return l; };
    hrow->addWidget(mono(tr("COMPARE")));
    m_filesLabel = mono(tr("No files selected — use Compare Docs to open two PDFs"));
    hrow->addWidget(m_filesLabel);
    auto* prev = new QToolButton; prev->setText(tr("← PREV")); prev->setProperty("variant","ghost");
    prev->setToolTip(tr("Navigate to previous change"));
    hrow->addWidget(prev);
    auto* next = new QToolButton; next->setText(tr("NEXT →")); next->setProperty("variant","ghost");
    next->setToolTip(tr("Navigate to next change"));
    hrow->addWidget(next);
    m_statusLabel = mono(tr("CHANGE 0 OF 0"));
    hrow->addWidget(m_statusLabel);
    hrow->addStretch(1);

    auto* toggleDiff = new QToolButton;
    toggleDiff->setText(tr("Toggle Overlay"));
    toggleDiff->setCheckable(true);
    connect(toggleDiff, &QToolButton::toggled, [this](bool checked) {
        m_compareWidget->setShowPixelDiff(checked);
    });
    hrow->addWidget(toggleDiff);

    m_exportBtn = new QToolButton;
    m_exportBtn->setText(tr("Export Report"));
    m_exportBtn->setProperty("variant","ghost");
    m_exportBtn->setEnabled(false);
    m_exportBtn->setToolTip(tr("Export the comparison results to HTML or text"));
    connect(m_exportBtn, &QToolButton::clicked, this, &CompareMode::onExportReport);
    hrow->addWidget(m_exportBtn);

    // "Close Compare" button removed (AR-8 D3): navigating away from compare mode
    // is handled by the mode strip, so a second button here was inert dead UI.
    col->addWidget(tb);

    m_compareWidget = new CompareWidget(this);
    col->addWidget(m_compareWidget, 1);

    // Wire PREV/NEXT to CompareWidget navigation (captured after m_compareWidget exists)
    connect(prev, &QToolButton::clicked, m_compareWidget, &CompareWidget::prevChange);
    connect(next, &QToolButton::clicked, m_compareWidget, &CompareWidget::nextChange);

    // changes panel
    auto* changes = new QFrame;
    changes->setFixedHeight(160);
    auto* cl = new QVBoxLayout(changes); cl->setContentsMargins(0,0,0,0); cl->setSpacing(0);
    auto* ch = new QFrame; ch->setProperty("role","modeToolbar"); ch->setFixedHeight(26);
    auto* chr = new QHBoxLayout(ch); chr->setContentsMargins(12,0,12,0);
    auto* chl = new QLabel(CompareMode::tr("CHANGES")); chl->setProperty("mono",true); chr->addWidget(chl); chr->addStretch(1);
    cl->addWidget(ch);

    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({CompareMode::tr("#"), CompareMode::tr("Page"), CompareMode::tr("Description")});
    m_tree->setRootIsDecorated(false);
    cl->addWidget(m_tree, 1);
    col->addWidget(changes);

    connect(&m_watcher, &QFutureWatcher<DiffResult>::finished, this, &CompareMode::onDiffFinished);
}

void CompareMode::compareFiles(const QString& file1, const QString& file2) {
    m_file1 = file1;
    m_file2 = file2;
    m_compareWidget->loadDocuments(file1, file2);
    // Update the toolbar label to show the actual file names being compared.
    if (m_filesLabel) {
        m_filesLabel->setText(QFileInfo(file1).fileName() + "   ↔   " + QFileInfo(file2).fileName());
    }
    m_statusLabel->setText(tr("COMPARING..."));
    m_tree->clear();
    if (m_exportBtn) m_exportBtn->setEnabled(false);

    QFuture<DiffResult> future = QtConcurrent::run([file1, file2]() {
        DiffEngine engine;
        return engine.compare(file1, file2, 150);
    });
    m_watcher.setFuture(future);
}

void CompareMode::onDiffFinished() {
    m_lastResult = m_watcher.result();
    m_compareWidget->setDiffResult(m_lastResult);
    if (m_exportBtn) m_exportBtn->setEnabled(true);

    if (m_lastResult.isIdentical) {
        m_statusLabel->setText(tr("FILES ARE IDENTICAL"));
        return;
    }

    int totalChanges = 0;
    m_tree->clear();
    for (const auto& page : m_lastResult.pages) {
        const int textChanges = page.textAdded.size() + page.textRemoved.size()
                                + page.moves.size();
        const int changes = textChanges + (page.pixelDiffCount > 0 ? 1 : 0);
        if (changes > 0) {
            totalChanges += changes;
            QString desc = tr("+%1 words  -%2 words  ↔%3 moved  ~%4 px")
                               .arg(page.textAdded.size())
                               .arg(page.textRemoved.size())
                               .arg(page.moves.size())
                               .arg(page.pixelDiffCount);
            auto* item = new QTreeWidgetItem(m_tree,
                {QString::number(totalChanges),
                 QString("p.%1").arg(page.pageIndex + 1),
                 desc});
            // Colour code items that have moves
            if (!page.moves.isEmpty())
                item->setForeground(2, QColor("#d97c00"));  // orange for moves
        }
    }

    // Page-level reorder entries (whole pages that moved position).
    for (const auto& mv : m_lastResult.pageMoves) {
        ++totalChanges;
        auto* item = new QTreeWidgetItem(m_tree,
            {QString::number(totalChanges),
             QString("p.%1 \xE2\x86\x92 p.%2").arg(mv.fromPage + 1).arg(mv.toPage + 1),
             tr("Page %1 moved to position %2").arg(mv.fromPage + 1).arg(mv.toPage + 1)});
        item->setForeground(2, QColor("#7a4cc8"));  // purple for page moves
    }

    m_statusLabel->setText(tr("%1 CHANGES").arg(totalChanges));
}

// ── Report export ───────────────────────────────────────────────────────────────

void CompareMode::onExportReport() {
    QString selectedFilter;
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export Comparison Report"), QStringLiteral("compare-report.html"),
        tr("HTML Report (*.html);;Text Report (*.txt)"), &selectedFilter);
    if (path.isEmpty()) return;

    const bool asText = selectedFilter.contains(QStringLiteral("*.txt"))
                        || path.endsWith(QLatin1String(".txt"), Qt::CaseInsensitive);
    if (!asText && !path.endsWith(QLatin1String(".html"), Qt::CaseInsensitive))
        path += QStringLiteral(".html");

    const QString content = asText ? buildTextReport() : buildHtmlReport();

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Report"),
            tr("Could not write report to:\n%1").arg(path));
        return;
    }
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts << content;
    f.close();

    m_statusLabel->setText(tr("Report exported: %1").arg(QFileInfo(path).fileName()));
}

QString CompareMode::buildHtmlReport() const {
    auto esc = [](const QString& s) { return s.toHtmlEscaped(); };

    int totalAdded = 0, totalRemoved = 0, totalMoved = 0, changedPages = 0;
    for (const auto& page : m_lastResult.pages) {
        totalAdded   += page.textAdded.size();
        totalRemoved += page.textRemoved.size();
        totalMoved   += page.moves.size();
        if (!page.textAdded.isEmpty() || !page.textRemoved.isEmpty()
            || !page.moves.isEmpty() || page.pixelDiffCount > 0)
            ++changedPages;
    }
    const int totalChanges = totalAdded + totalRemoved + totalMoved;

    QString html;
    QTextStream o(&html);
    o << "<!DOCTYPE html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">\n";
    o << "<title>GlyphPDF Comparison Report</title>\n";
    o << "<style>\n"
         "body{font-family:'Segoe UI',Arial,sans-serif;margin:24px;color:#1a1b1e;}\n"
         "h1{font-size:20px;} h2{font-size:15px;margin-top:24px;}\n"
         ".summary{background:#f4f5f7;border:1px solid #d8dade;border-radius:6px;padding:12px 16px;}\n"
         ".summary td{padding:2px 16px 2px 0;}\n"
         "table.diff{border-collapse:collapse;width:100%;margin-top:8px;}\n"
         "table.diff th,table.diff td{border:1px solid #d8dade;padding:4px 8px;"
         "vertical-align:top;font-size:13px;width:50%;}\n"
         "table.diff th{background:#eceef1;text-align:left;}\n"
         ".removed{color:#c8442b;background:#fbecea;}\n"
         ".added{color:#1f8a44;background:#e9f6ee;}\n"
         ".unchanged{color:#71747a;}\n"
         ".moved{color:#d97c00;}\n"
         "</style></head>\n<body>\n";

    o << "<h1>GlyphPDF Comparison Report</h1>\n";
    o << "<table class=\"summary\">\n";
    o << "<tr><td><b>Original</b></td><td>" << esc(m_file1) << "</td></tr>\n";
    o << "<tr><td><b>Revised</b></td><td>"  << esc(m_file2) << "</td></tr>\n";
    o << "<tr><td><b>Date</b></td><td>" << esc(QDate::currentDate().toString(Qt::ISODate)) << "</td></tr>\n";
    o << "<tr><td><b>Pages</b></td><td>" << m_lastResult.pageCount1 << " \xE2\x86\x92 "
      << m_lastResult.pageCount2 << "</td></tr>\n";
    o << "<tr><td><b>Total changes</b></td><td>" << totalChanges
      << " (" << totalAdded << " added, " << totalRemoved << " removed, "
      << totalMoved << " moved across " << changedPages << " pages)</td></tr>\n";
    o << "</table>\n";

    if (m_lastResult.isIdentical) {
        o << "<p class=\"unchanged\">The documents are identical.</p>\n";
        o << "</body></html>\n";
        return html;
    }

    if (!m_lastResult.pageMoves.isEmpty()) {
        o << "<h2>Page moves</h2>\n<ul>\n";
        for (const auto& mv : m_lastResult.pageMoves)
            o << "<li class=\"moved\">Page " << (mv.fromPage + 1) << " moved to position "
              << (mv.toPage + 1) << " <span class=\"unchanged\">" << esc(mv.excerpt) << "</span></li>\n";
        o << "</ul>\n";
    }

    for (const auto& page : m_lastResult.pages) {
        const bool changed = !page.textAdded.isEmpty() || !page.textRemoved.isEmpty()
                             || !page.moves.isEmpty() || page.pixelDiffCount > 0;
        if (!changed) continue;

        o << "<h2>Page " << (page.pageIndex + 1) << "</h2>\n";
        o << "<table class=\"diff\"><tr><th>Removed</th><th>Added</th></tr>\n";
        o << "<tr><td class=\"removed\">"
          << (page.textRemoved.isEmpty() ? QStringLiteral("<span class=\"unchanged\">\xE2\x80\x94</span>")
                                         : esc(page.textRemoved.join(QLatin1Char(' '))))
          << "</td><td class=\"added\">"
          << (page.textAdded.isEmpty() ? QStringLiteral("<span class=\"unchanged\">\xE2\x80\x94</span>")
                                       : esc(page.textAdded.join(QLatin1Char(' '))))
          << "</td></tr>\n</table>\n";

        if (!page.moves.isEmpty()) {
            o << "<p class=\"moved\">Moved: ";
            QStringList mv;
            for (const auto& m : page.moves)
                mv << esc(m.token) + QStringLiteral(" (%1\xE2\x86\x92%2)").arg(m.fromIndex).arg(m.toIndex);
            o << mv.join(QStringLiteral(", ")) << "</p>\n";
        }
        if (page.pixelDiffCount > 0)
            o << "<p class=\"unchanged\">~" << page.pixelDiffCount << " pixels differ visually.</p>\n";
    }

    o << "</body></html>\n";
    return html;
}

QString CompareMode::buildTextReport() const {
    QString out;
    QTextStream o(&out);

    int totalAdded = 0, totalRemoved = 0, totalMoved = 0;
    for (const auto& page : m_lastResult.pages) {
        totalAdded   += page.textAdded.size();
        totalRemoved += page.textRemoved.size();
        totalMoved   += page.moves.size();
    }

    o << "GlyphPDF Comparison Report\n";
    o << "==========================\n";
    o << "Original: " << m_file1 << "\n";
    o << "Revised:  " << m_file2 << "\n";
    o << "Date:     " << QDate::currentDate().toString(Qt::ISODate) << "\n";
    o << "Pages:    " << m_lastResult.pageCount1 << " -> " << m_lastResult.pageCount2 << "\n";
    o << "Changes:  " << (totalAdded + totalRemoved + totalMoved)
      << " (" << totalAdded << " added, " << totalRemoved << " removed, "
      << totalMoved << " moved)\n\n";

    if (m_lastResult.isIdentical) {
        o << "The documents are identical.\n";
        return out;
    }

    if (!m_lastResult.pageMoves.isEmpty()) {
        o << "Page moves:\n";
        for (const auto& mv : m_lastResult.pageMoves)
            o << "  Page " << (mv.fromPage + 1) << " moved to position " << (mv.toPage + 1) << "\n";
        o << "\n";
    }

    for (const auto& page : m_lastResult.pages) {
        const bool changed = !page.textAdded.isEmpty() || !page.textRemoved.isEmpty()
                             || !page.moves.isEmpty() || page.pixelDiffCount > 0;
        if (!changed) continue;

        o << "--- Page " << (page.pageIndex + 1) << " ---\n";
        for (const QString& w : page.textRemoved)
            o << "- " << w << "\n";
        for (const QString& w : page.textAdded)
            o << "+ " << w << "\n";
        for (const auto& m : page.moves)
            o << "~ " << m.token << " (" << m.fromIndex << " -> " << m.toIndex << ")\n";
        if (page.pixelDiffCount > 0)
            o << "  (~" << page.pixelDiffCount << " pixels differ visually)\n";
        o << "\n";
    }
    return out;
}

} // namespace gp
