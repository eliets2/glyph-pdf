// SPDX-License-Identifier: Apache-2.0
#include "PdfAValidationPanel.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"
#include "util/GpTheme.h"
#include "util/Badge.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QTextStream>
#include <QVBoxLayout>
#include <algorithm>
#include <QtConcurrent/QtConcurrent>
#include <podofo/podofo.h>

namespace gp {

// ---------------------------------------------------------------------------
// Helper: build a single violation row widget
// ---------------------------------------------------------------------------
// `panel` is the owning PdfAValidationPanel (used to reach the MainWindow's
// viewer for the JUMP action). `pageNumber` is 1-based (veraPDF convention) or
// -1 when the location is unknown — in that case the JUMP button is hidden so
// it is never a dead control.
static QWidget* issueRow(QWidget* panel, const QString& rule, const QString& descr,
                         bool err, int pageNumber) {
    auto* w = new QFrame;
    w->setStyleSheet(QString("background:#1a1b1e; border:1px solid #393b40; border-left:3px solid %1; padding:8px 10px; margin-bottom:6px;")
        .arg(err ? "#c8442b" : "#ff8c42"));
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    auto* dot = new QLabel(err ? "✕" : "▲");
    dot->setStyleSheet(QString("color:%1;font-weight:700;").arg(err ? "#c8442b" : "#ff8c42"));
    auto* lbl = new QLabel(QString("<b style='color:#dfe1e5;font-family:JetBrains Mono;font-size:10px;'>%1</b> · %2").arg(rule, descr));
    lbl->setTextFormat(Qt::RichText);
    lbl->setWordWrap(true);
    auto* jump = new QPushButton(QObject::tr("JUMP"));
    jump->setStyleSheet("font-family:JetBrains Mono; font-size:9.5px; color:#ff8c42; border:1px solid rgba(255,140,66,0.33); padding:1px 6px;");
    h->addWidget(dot);
    h->addWidget(lbl, 1);
    h->addWidget(jump);

    if (pageNumber >= 1) {
        jump->setAccessibleName(QObject::tr("Jump to page %1").arg(pageNumber));
        // Navigate the viewer to the violation's page. veraPDF page numbers are
        // 1-based; goToPage() is 0-based.
        const int targetPage = pageNumber - 1;
        QObject::connect(jump, &QPushButton::clicked, panel, [panel, targetPage]() {
            for (QWidget* p = panel; p; p = p->parentWidget()) {
                if (auto* mw = qobject_cast<MainWindow*>(p)) {
                    if (auto* viewer = mw->pdfViewer())
                        viewer->goToPage(targetPage);
                    break;
                }
            }
        });
    } else {
        // No page information for this violation — hide rather than leave a
        // button that does nothing.
        jump->hide();
    }
    return w;
}

// ---------------------------------------------------------------------------
// Constructor — builds the static skeleton; dynamic content via updateDisplay()
// ---------------------------------------------------------------------------
PdfAValidationPanel::PdfAValidationPanel(QWidget* parent) : QFrame(parent) {
    setObjectName("rightSidebar");
    setFixedWidth(Theme::RightPaneW);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0); outer->setSpacing(0);

    // Header bar
    auto* head = new QFrame; head->setProperty("role","modeToolbar"); head->setFixedHeight(32);
    auto* hr = new QHBoxLayout(head); hr->setContentsMargins(10,0,10,0);
    auto* t = new QLabel(tr("PDF/A · VALIDATION")); t->setStyleSheet("font-weight:600;letter-spacing:1.2px;");
    hr->addWidget(t); hr->addStretch(1);
    outer->addWidget(head);

    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto* body = new QWidget;
    auto* col = new QVBoxLayout(body);
    col->setContentsMargins(12,12,12,12); col->setSpacing(8);

    // Status label — shows validation result or "unavailable" message
    m_statusLabel = new QLabel(tr("No document loaded."));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setProperty("mono", true);
    col->addWidget(m_statusLabel);

    // Issues heading (hidden until validation runs)
    m_issuesHeading = new QLabel;
    m_issuesHeading->setProperty("mono", true);
    m_issuesHeading->hide();
    col->addWidget(m_issuesHeading);

    // Dynamic issues list container
    m_issuesList = new QWidget;
    m_issuesLayout = new QVBoxLayout(m_issuesList);
    m_issuesLayout->setContentsMargins(0,0,0,0);
    m_issuesLayout->setSpacing(0);
    m_issuesList->hide();
    col->addWidget(m_issuesList);

    // Action buttons
    // AR-8 D3: "Fix Automatically" starts disabled and is conditionally enabled
    // by updateDisplay() when a real export callback is wired — keep it.
    m_fixBtn = new QPushButton(tr("Fix Automatically"));
    m_fixBtn->setEnabled(false);

    // AR-8 D3: "Convert to PDF/A-2B" button HIDDEN — it only showed a redirect
    // MessageBox ("not available in this version"), making it a dead placeholder.
    // The planned inline-convert feature is preserved; add it back here when wired.

    m_exportBtn = new QPushButton(tr("Export Report"));

    m_readingOrderBtn = new QPushButton(tr("Check Reading Order"));
    m_readingOrderBtn->setToolTip(tr("Check tagged-PDF structure order against visual layout (accessibility)"));

    col->addWidget(m_fixBtn);
    col->addWidget(m_exportBtn);
    col->addWidget(m_readingOrderBtn);
    col->addStretch(1);

    scroll->setWidget(body);
    outer->addWidget(scroll, 1);

    connect(m_exportBtn, &QPushButton::clicked, this, &PdfAValidationPanel::onExportReportClicked);
    connect(m_readingOrderBtn, &QPushButton::clicked, this, &PdfAValidationPanel::onCheckReadingOrder);
}

// ---------------------------------------------------------------------------
// Public slot — set current document and trigger validation
// ---------------------------------------------------------------------------
void PdfAValidationPanel::setDocument(const QString& path, PdfAConformance level) {
    m_currentDocPath = path;
    m_currentConformance = level;
    runValidation();
}

void PdfAValidationPanel::setExportPdfACallback(
    std::function<bool(const QString& outputPath, int conformanceLevel)> cb)
{
    m_exportPdfACallback = std::move(cb);
}

// ---------------------------------------------------------------------------
// Run veraPDF validation and refresh the display
// ---------------------------------------------------------------------------
void PdfAValidationPanel::runValidation() {
    if (m_currentDocPath.isEmpty()) {
        m_statusLabel->setText(tr("No document loaded."));
        m_issuesHeading->hide();
        m_issuesList->hide();
        return;
    }

    m_statusLabel->setText(tr("Validating…"));

    // AR-7 D2: veraPDF launches a Java subprocess and can block for several seconds.
    // Run it on a worker thread via QtConcurrent so the GUI stays responsive.
    if (!m_validationWatcher) {
        m_validationWatcher = new QFutureWatcher<PdfAValidationReport>(this);
        connect(m_validationWatcher, &QFutureWatcher<PdfAValidationReport>::finished,
                this, &PdfAValidationPanel::onValidationFinished);
    }

    // Cancel any in-flight validation (new document opened while old one ran).
    if (m_validationWatcher->isRunning()) {
        m_validationWatcher->cancel();
        m_validationWatcher->waitForFinished();
    }

    const QString path = m_currentDocPath;
    const PdfAConformance level = m_currentConformance;
    m_validationWatcher->setFuture(QtConcurrent::run([path, level]() {
        return VeraPdfValidator::validate(path, level);
    }));
}

void PdfAValidationPanel::onValidationFinished() {
    if (!m_validationWatcher || m_validationWatcher->isCanceled()) return;
    updateDisplay(m_validationWatcher->result());
}

// ---------------------------------------------------------------------------
// Update the panel UI from a PdfAValidationReport
// ---------------------------------------------------------------------------
void PdfAValidationPanel::updateDisplay(const PdfAValidationReport& report) {
    // Clear previous violation rows
    QLayoutItem* item;
    while ((item = m_issuesLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_issuesList->hide();
    m_issuesHeading->hide();

    if (!report.validatorAvailable) {
        m_statusLabel->setText(
            tr("PDF/A validation needs veraPDF, which isn't installed. "
               "Download it free from verapdf.org — all other features work normally."));
        m_exportBtn->setEnabled(false);
        m_fixBtn->setEnabled(false);
        if (m_fixBtnConn) disconnect(m_fixBtnConn);
        return;
    }

    if (!report.errorMessage.isEmpty()) {
        m_statusLabel->setText(tr("Validation error: %1").arg(report.errorMessage));
        m_exportBtn->setEnabled(false);
        m_fixBtn->setEnabled(false);
        if (m_fixBtnConn) disconnect(m_fixBtnConn);
        return;
    }

    m_exportBtn->setEnabled(true);

    if (report.isValid) {
        QString level = report.conformanceLevel.isEmpty()
            ? tr("PDF/A")
            : report.conformanceLevel;
        m_statusLabel->setText(tr("✓ Conforms to %1").arg(level));
        m_fixBtn->setEnabled(false);
        if (m_fixBtnConn) disconnect(m_fixBtnConn);
        return;
    }

    // Violations found
    int n = report.violations.size();
    m_statusLabel->setText(tr("✗ %1 violation(s) found").arg(n));

    if (n > 0) {
        m_issuesHeading->setText(tr("ISSUES · %1").arg(n));
        m_issuesHeading->show();

        for (const RuleViolation& v : report.violations) {
            QString label = v.pageNumber >= 0
                ? tr("%1 · Page %2").arg(v.description).arg(v.pageNumber)
                : v.description;
            bool isError = (v.severity == "error");
            m_issuesLayout->addWidget(issueRow(this, v.ruleId, label, isError, v.pageNumber));
        }
        m_issuesList->show();

        if (m_fixBtnConn) disconnect(m_fixBtnConn);

        if (m_exportPdfACallback) {
            m_fixBtn->setEnabled(true);
            m_fixBtn->setToolTip(tr(
                "Export a PDF/A-compliant copy of this document, "
                "fixing the violations listed above."));

            const QString docPath = m_currentDocPath;
            const int conformance = static_cast<int>(m_currentConformance);
            auto cb = m_exportPdfACallback;

            m_fixBtnConn = connect(m_fixBtn, &QPushButton::clicked, this,
                [this, docPath, conformance, cb]() {
                    QFileInfo fi(docPath);
                    const QString suggested =
                        fi.dir().filePath(fi.completeBaseName() + "_fixed_pdfa.pdf");
                    QString dest = QFileDialog::getSaveFileName(
                        this,
                        tr("Save Fixed PDF/A"),
                        suggested,
                        tr("PDF files (*.pdf);;All files (*)"));
                    if (dest.isEmpty()) return;

                    const bool ok = cb(dest, conformance);
                    if (ok) {
                        QMessageBox::information(this, tr("Fix Applied"),
                            tr("PDF/A-compliant copy saved to:\n%1\n\nRe-validating…").arg(dest));
                        setDocument(dest, static_cast<PdfAConformance>(conformance));
                    } else {
                        QMessageBox::warning(this, tr("Fix Failed"),
                            tr("Could not export a compliant copy.\n"
                               "Try File > Export As PDF/A for more options."));
                    }
                });
        } else {
            m_fixBtn->setEnabled(false);
            m_fixBtn->setToolTip(tr("No export callback configured."));
        }
    }
}

// ---------------------------------------------------------------------------
// Export Report — write violations to a JSON file chosen via QFileDialog
// ---------------------------------------------------------------------------
void PdfAValidationPanel::onExportReportClicked() {
    if (m_currentDocPath.isEmpty()) {
        QMessageBox::information(this, tr("Export Report"),
            tr("No document to export a report for."));
        return;
    }

    QString dest = QFileDialog::getSaveFileName(
        this,
        tr("Export Validation Report"),
        QString(),
        tr("JSON files (*.json);;Text files (*.txt);;All files (*)"));

    if (dest.isEmpty()) return;

    auto report = VeraPdfValidator::validate(m_currentDocPath, m_currentConformance);

    QFile file(dest);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Failed"),
            tr("Could not write to %1").arg(dest));
        return;
    }

    QTextStream out(&file);
    out << "{\n";
    out << "  \"conformanceLevel\": \"" << report.conformanceLevel << "\",\n";
    out << "  \"isValid\": " << (report.isValid ? "true" : "false") << ",\n";
    out << "  \"validatorAvailable\": " << (report.validatorAvailable ? "true" : "false") << ",\n";
    if (!report.errorMessage.isEmpty()) {
        QString escaped = report.errorMessage;
        escaped.replace("\\", "\\\\").replace("\"", "\\\"");
        out << "  \"errorMessage\": \"" << escaped << "\",\n";
    }
    out << "  \"violations\": [\n";
    const auto& viols = report.violations;
    for (int i = 0; i < viols.size(); ++i) {
        const RuleViolation& v = viols[i];
        QString desc = v.description;
        desc.replace("\\", "\\\\").replace("\"", "\\\"");
        out << "    {\n";
        out << "      \"ruleId\": \"" << v.ruleId << "\",\n";
        out << "      \"clause\": \"" << v.clause << "\",\n";
        out << "      \"description\": \"" << desc << "\",\n";
        out << "      \"pageNumber\": " << v.pageNumber << ",\n";
        out << "      \"severity\": \"" << v.severity << "\"\n";
        out << "    }" << (i + 1 < viols.size() ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";

    file.close();
    QMessageBox::information(this, tr("Report Exported"),
        tr("Validation report written to:\n%1").arg(dest));
}

// ---------------------------------------------------------------------------
// §9.14 Tagged-PDF reading-order check
// ---------------------------------------------------------------------------
namespace {

struct StructElem {
    QString type;
    int     page   = -1;
    double  topY   = 0.0;   // top edge from /BBox (if present), in PDF user space
    bool    hasBBox = false;
};

// ReadingOrderResult is declared in PdfAValidationPanel.h (shared with tests).
const PoDoFo::PdfObject* resolveObj(PoDoFo::PdfMemDocument& doc, const PoDoFo::PdfObject* o) {
    if (!o) return nullptr;
    if (o->IsReference()) return doc.GetObjects().GetObject(o->GetReference());
    return o;
}

double numberValue(const PoDoFo::PdfObject& o) {
    if (o.IsNumberOrReal()) return o.GetReal();
    return 0.0;
}

int pageIndexOf(PoDoFo::PdfMemDocument& doc, const PoDoFo::PdfObject* pg) {
    pg = resolveObj(doc, pg);
    if (!pg) return -1;
    auto& pages = doc.GetPages();
    for (unsigned i = 0; i < pages.GetCount(); ++i)
        if (&pages.GetPageAt(i).GetObject() == pg) return static_cast<int>(i);
    return -1;
}

// §9.14 P0: ISO 32000-2 §14.7.2 — a structure element's /Pg entry is
// INHERITED from its nearest ancestor when absent. Coding a missing /Pg as
// page -1 produced false "out of order" flags on correctly-tagged PDFs whose
// child elements rely on inheritance from the parent's /Pg.
void collectStructElems(PoDoFo::PdfMemDocument& doc, const PoDoFo::PdfObject* node,
                        QList<StructElem>& out, int depth, int inheritedPage = -1);

// Extract /BBox top-edge from a struct element's /A layout attribute(s).
void extractBBox(PoDoFo::PdfMemDocument& doc, const PoDoFo::PdfDictionary& d, StructElem& e) {
    auto tryDict = [&](const PoDoFo::PdfObject* a) -> bool {
        a = resolveObj(doc, a);
        if (!a || !a->IsDictionary()) return false;
        const PoDoFo::PdfObject* bb = a->GetDictionary().FindKey("BBox");
        if (bb && bb->IsArray() && bb->GetArray().size() >= 4) {
            const auto& arr = bb->GetArray();
            e.topY = std::max(numberValue(arr[1]), numberValue(arr[3]));
            e.hasBBox = true;
            return true;
        }
        return false;
    };
    const PoDoFo::PdfObject* aObj = d.FindKey("A");
    if (!aObj) return;
    aObj = resolveObj(doc, aObj);
    if (aObj && aObj->IsArray()) {
        for (const auto& el : aObj->GetArray())
            if (tryDict(&el)) break;
    } else {
        tryDict(aObj);
    }
}

// Depth-first walk of the structure tree, collecting structure elements in
// reading (document structure) order.
void collectStructElems(PoDoFo::PdfMemDocument& doc, const PoDoFo::PdfObject* node,
                        QList<StructElem>& out, int depth, int inheritedPage) {
    if (!node || depth > 60) return;
    node = resolveObj(doc, node);
    if (!node) return;

    if (node->IsArray()) {
        for (const auto& child : node->GetArray())
            collectStructElems(doc, &child, out, depth + 1, inheritedPage);
        return;
    }
    if (!node->IsDictionary()) return;  // e.g. a bare MCID integer — skip

    const PoDoFo::PdfDictionary& d = node->GetDictionary();
    // This element's effective page: explicit /Pg wins; otherwise inherit the
    // nearest ancestor's page (ISO 32000-2 §14.7.2).
    const PoDoFo::PdfObject* ownPg = d.FindKey("Pg");
    const int elemPage = ownPg ? pageIndexOf(doc, ownPg) : inheritedPage;
    const PoDoFo::PdfObject* sObj = d.FindKey("S");
    if (sObj && sObj->IsName()) {
        StructElem e;
        e.type = QString::fromStdString(std::string(sObj->GetName().GetString()));
        e.page = elemPage;
        extractBBox(doc, d, e);
        out.append(e);
    }
    if (const PoDoFo::PdfObject* k = d.FindKey("K"))
        collectStructElems(doc, k, out, depth + 1, elemPage);
}

} // namespace

ReadingOrderResult analyzeReadingOrder(const QString& path) {
    ReadingOrderResult r;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());

        const PoDoFo::PdfObject* root =
            doc.GetCatalog().GetDictionary().FindKey("StructTreeRoot");
        if (!root) return r;  // not tagged
        r.tagged = true;

        QList<StructElem> elems;
        collectStructElems(doc, root, elems, 0);
        r.elementCount = elems.size();
        if (elems.size() < 2) return r;  // nothing meaningful to reorder

        // Visual order: sort by (page, then top-of-page first). Elements without
        // a /BBox keep their structural order within the page via a tiny
        // struct-index nudge, so only BBox-bearing elements can be flagged.
        const int n = elems.size();
        QVector<int> structIdx(n);
        for (int i = 0; i < n; ++i) structIdx[i] = i;

        auto visualKey = [&](int i) -> double {
            const StructElem& e = elems[i];
            const int pg = (e.page < 0) ? 100000 : e.page;
            const double y = e.hasBBox ? e.topY : (1.0e6 - static_cast<double>(i));
            return static_cast<double>(pg) * 1.0e7 - y;  // lower sorts earlier
        };

        QVector<int> visualOrder = structIdx;
        std::stable_sort(visualOrder.begin(), visualOrder.end(),
                         [&](int a, int b) { return visualKey(a) < visualKey(b); });

        // visualPos[structIndex] = position in visual order
        QVector<int> visualPos(n, 0);
        for (int pos = 0; pos < n; ++pos) visualPos[visualOrder[pos]] = pos;

        // §9.14 P1: flag elements further than kReadingOrderSlotTolerance
        // positions from their visual position. The threshold is the NAMED,
        // documented constant in PdfAValidationPanel.h — a HEURISTIC per
        // PDF/UA practice (a low-noise triage aid); human review remains
        // authoritative. Boundary pinned by TestReadingOrderThreshold:
        // displacement of exactly 2 slots is not reported, exactly 3 is.
        for (int i = 0; i < n; ++i) {
            if (std::abs(i - visualPos[i]) > gp::kReadingOrderSlotTolerance) {
                const StructElem& e = elems[i];
                r.issues << QObject::tr("\"%1\" at structure position %2 maps to visual position %3%4")
                    .arg(e.type.isEmpty() ? QStringLiteral("Elem") : e.type)
                    .arg(i + 1)
                    .arg(visualPos[i] + 1)
                    .arg(e.page >= 0 ? QObject::tr(" (page %1)").arg(e.page + 1) : QString());
                r.issuePages << e.page; // §9.14 P0: parallel page list for jump-to-page
            }
        }
    } catch (const PoDoFo::PdfError& ex) {
        qWarning() << "analyzeReadingOrder error:" << ex.what();
    }
    return r;
}

void PdfAValidationPanel::onCheckReadingOrder() {
    if (m_currentDocPath.isEmpty()) {
        QMessageBox::information(this, tr("Reading Order"),
            tr("No document loaded."));
        return;
    }

    // §9.14: run the analysis off the GUI thread — same QFutureWatcher pattern
    // the sibling veraPDF validation in this file already uses. The PoDoFo
    // parse + structure-tree walk can take seconds on large/deeply-tagged
    // documents and used to freeze the whole UI.
    if (!m_readingOrderWatcher) {
        m_readingOrderWatcher = new QFutureWatcher<ReadingOrderResult>(this);
        connect(m_readingOrderWatcher, &QFutureWatcher<ReadingOrderResult>::finished,
                this, &PdfAValidationPanel::onReadingOrderFinished);
    }
    if (m_readingOrderWatcher->isRunning()) {
        m_readingOrderWatcher->cancel();
        m_readingOrderWatcher->waitForFinished();
    }
    m_readingOrderBtn->setEnabled(false);
    m_statusLabel->setText(tr("Analyzing reading order…"));
    const QString path = m_currentDocPath;
    m_readingOrderWatcher->setFuture(QtConcurrent::run([path]() {
        return analyzeReadingOrder(path);
    }));
}

void PdfAValidationPanel::onReadingOrderFinished() {
    if (!m_readingOrderWatcher || m_readingOrderWatcher->isCanceled()) return;
    m_readingOrderBtn->setEnabled(true);
    const ReadingOrderResult r = m_readingOrderWatcher->result();

    if (!r.tagged) {
        QMessageBox::information(this, tr("Reading Order"),
            tr("Document is not tagged. Accessibility reading order cannot be verified."));
        return;
    }

    // Clear previous rows (same pattern as updateDisplay).
    QLayoutItem* item;
    while ((item = m_issuesLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    if (r.issues.isEmpty()) {
        m_statusLabel->setText(tr("✓ %1 elements. Reading order: OK.").arg(r.elementCount));
        m_issuesHeading->hide();
        m_issuesList->hide();
        QMessageBox::information(this, tr("Reading Order"),
            tr("Tagged PDF detected. %1 elements. Reading order: OK.").arg(r.elementCount));
        return;
    }

    // §9.14 P0: report each mismatch as a JUMP-able row in the issues list —
    // same interaction as the veraPDF violation list — instead of a static
    // message box.
    m_statusLabel->setText(tr("✗ %1 elements. %2 reading-order issue(s).").arg(r.elementCount).arg(r.issues.size()));
    m_issuesHeading->setText(tr("READING ORDER · %1").arg(r.issues.size()));
    m_issuesHeading->show();

    const int shown = std::min(static_cast<int>(r.issues.size()), 20);
    for (int i = 0; i < shown; ++i) {
        const int page1 = (i < r.issuePages.size()) ? r.issuePages[i] + 1 : -1;
        m_issuesLayout->addWidget(
            issueRow(this, QStringLiteral("RO"), r.issues[i], /*err=*/true, page1));
    }
    if (r.issues.size() > shown) {
        auto* more = new QLabel(tr("… and %1 more.").arg(r.issues.size() - shown));
        more->setWordWrap(true);
        m_issuesLayout->addWidget(more);
    }
    m_issuesList->show();
}

} // namespace gp
