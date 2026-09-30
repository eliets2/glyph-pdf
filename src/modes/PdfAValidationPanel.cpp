// SPDX-License-Identifier: Apache-2.0
#include "PdfAValidationPanel.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"
#include "util/GpTheme.h"
#include "util/Badge.h"

#include <QFileDialog>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSet>
#include <QStringList>
#include <QTextStream>
#include <QVBoxLayout>
#include <QVariant>
#include <algorithm>
#include <iterator>
#include <vector>
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
    m_statusLabel->setObjectName(QStringLiteral("pdfaStatusLabel"));
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
// ARC06: bounded async lifetime — a destroyed panel must not leave in-flight
// validation/reading-order workers delivering into it.
// ---------------------------------------------------------------------------
PdfAValidationPanel::~PdfAValidationPanel() {
    for (QFutureWatcherBase* w :
         { m_validationWatcher ? static_cast<QFutureWatcherBase*>(m_validationWatcher) : nullptr,
           m_readingOrderWatcher ? static_cast<QFutureWatcherBase*>(m_readingOrderWatcher) : nullptr }) {
        if (!w) continue;
        if (w->isRunning()) {
            w->cancel();
            w->waitForFinished();
        }
    }
}

// ---------------------------------------------------------------------------
// Public slot — set current document and trigger validation
// ---------------------------------------------------------------------------
// ARC06: this is the panel's ONLY production path setter. A document change
// cancels BOTH in-flight async workers first (a slow validation or
// reading-order run for the OLD document must never populate the NEW one),
// and the finished handlers additionally discard any result whose submitted
// identity no longer matches m_currentDocPath.
void PdfAValidationPanel::setDocument(const QString& path, PdfAConformance level) {
    m_currentDocPath = path;
    m_currentConformance = level;

    if (m_validationWatcher && m_validationWatcher->isRunning()) {
        m_validationWatcher->cancel();
        m_validationWatcher->waitForFinished();
    }
    if (m_readingOrderWatcher && m_readingOrderWatcher->isRunning()) {
        m_readingOrderWatcher->cancel();
        m_readingOrderWatcher->waitForFinished();
    }

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
    m_submittedValidationPath = path;
    m_validationWatcher->setFuture(QtConcurrent::run([path, level]() {
        return VeraPdfValidator::validate(path, level);
    }));
}

void PdfAValidationPanel::onValidationFinished() {
    if (!m_validationWatcher || m_validationWatcher->isCanceled()) return;
    // ARC06: identity tie — a result submitted for a document that is no
    // longer current is discarded (never displayed for the new identity).
    if (m_submittedValidationPath != m_currentDocPath) return;
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

// PARITY-SCORECARD-2026-09-30 §4 #3: one marked-content reference under /K —
// a bare integer (ISO 32000-2 §14.7.3.3) or an /MCR dict — collected in
// document (reading) order so the analysis can extend INTO the content the
// structure tags, not just between the tags.
struct McidRef {
    int page = -1;   // 0-based page the reference belongs to (inherited per §14.7.2)
    int mcid = -1;
};

// What the content-stream walk resolved for one MCID: its top-most text
// baseline (page space, y-up — same convention as a struct /BBox top edge)
// and a best-effort printable-ASCII text snippet.
struct MarkedSpanPos {
    bool   hasY = false;
    double topY = 0.0;
    QString text;
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
//
// PARITY-SCORECARD-2026-09-30 §4 #3: `mcidRefs` (when non-null) additionally
// collects the marked-content references — bare MCID integers and /MCR dicts —
// in the same reading order. §4 #19: `maxDepth` is the named depth cap
// (kReadingOrderMaxStructDepth by default); when a node exists BEYOND it,
// `depthTruncated` is set so the caller can disclose the truncation instead
// of silently stopping.
void collectStructElems(PoDoFo::PdfMemDocument& doc, const PoDoFo::PdfObject* node,
                        QList<StructElem>& out, int depth, int inheritedPage = -1,
                        QList<McidRef>* mcidRefs = nullptr,
                        int maxDepth = gp::kReadingOrderMaxStructDepth,
                        bool* depthTruncated = nullptr);

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
                        QList<StructElem>& out, int depth, int inheritedPage,
                        QList<McidRef>* mcidRefs, int maxDepth,
                        bool* depthTruncated) {
    if (!node) return;
    if (depth > maxDepth) {
        // §4 #19: a node exists beyond the cap — the walk is being cut short.
        // Disclose it; never stop silently.
        if (depthTruncated) *depthTruncated = true;
        return;
    }
    node = resolveObj(doc, node);
    if (!node) return;

    if (node->IsArray()) {
        for (const auto& child : node->GetArray())
            collectStructElems(doc, &child, out, depth + 1, inheritedPage,
                               mcidRefs, maxDepth, depthTruncated);
        return;
    }
    if (node->IsNumber()) {
        // PARITY-SCORECARD-2026-09-30 §4 #3: a bare integer under /K is a
        // marked-content id into the element's page content stream
        // (ISO 32000-2 §14.7.3.3) — collect it instead of skipping it, so the
        // text-level order of the tagged content can be checked.
        if (mcidRefs)
            mcidRefs->append({ inheritedPage, static_cast<int>(node->GetNumber()) });
        return;
    }
    if (!node->IsDictionary()) return;

    const PoDoFo::PdfDictionary& d = node->GetDictionary();
    // The marked-content-reference DICT form (/MCR — /Type optional per
    // ISO 32000-2 Tab. 363, so keyed on /MCID without /S) references marked
    // content living in another stream (/Stm, the CX-07 shape): collect its
    // /Pg + /MCID so provenance stays right even though the page-level walk
    // will not find the span.
    const PoDoFo::PdfObject* sObj = d.FindKey("S");
    const PoDoFo::PdfObject* mcrMcid = sObj ? nullptr : d.FindKey("MCID");
    if (mcrMcid && mcrMcid->IsNumber()) {
        if (mcidRefs) {
            const PoDoFo::PdfObject* pg = d.FindKey("Pg");
            const int mcrPage = pg ? pageIndexOf(doc, pg) : inheritedPage;
            mcidRefs->append({ mcrPage, static_cast<int>(mcrMcid->GetNumber()) });
        }
        return;
    }
    // /OBJR (object reference, e.g. a Widget annotation) carries no marked
    // content of its own — nothing to collect.
    const PoDoFo::PdfObject* typeObj = d.FindKey("Type");
    if (typeObj && typeObj->IsName() && typeObj->GetName() == PoDoFo::PdfName("OBJR"))
        return;
    // This element's effective page: explicit /Pg wins; otherwise inherit the
    // nearest ancestor's page (ISO 32000-2 §14.7.2).
    const PoDoFo::PdfObject* ownPg = d.FindKey("Pg");
    const int elemPage = ownPg ? pageIndexOf(doc, ownPg) : inheritedPage;
    if (sObj && sObj->IsName()) {
        StructElem e;
        e.type = QString::fromStdString(std::string(sObj->GetName().GetString()));
        e.page = elemPage;
        extractBBox(doc, d, e);
        out.append(e);
    }
    if (const PoDoFo::PdfObject* k = d.FindKey("K"))
        collectStructElems(doc, k, out, depth + 1, elemPage, mcidRefs,
                           maxDepth, depthTruncated);
}

// ── PARITY-SCORECARD-2026-09-30 §4 #3 ────────────────────────────────────────
// Marked-content position extraction. Mirrors the page-content walk the
// redaction engine uses (PoDoFoBackend.cpp: PdfContentStreamReader +
// VariantStack operand order — stack[0] is the LAST pushed operand — BDC/EMC
// /MCID tracking incl. the named /Properties form, and full Tm/Tlm/CTM
// tracking per PDF 9.4.2), reused here read-only: each struct-referenced
// MCID gets its top-most text baseline (page space, y-up) and a bounded
// printable-ASCII snippet. Form XObjects are deliberately NOT followed
// (PdfContentReaderFlags::DontFollowXObjectForms): their MCIDs live in the
// FORM's StructParents namespace (they surface as /MCR dicts with /Stm, the
// CX-07 shape) and mixing namespaces could mis-resolve page-level ids — such
// spans simply keep their structural order below.
//
// Bounded like every checker walk: only MCIDs referenced by the structure
// tree are visited, and the walk stops at kReadingOrderMaxMarkedContentSpans
// resolved spans (disclosed by the caller), never scans unboundedly.
struct WalkMat {
    // PDF affine identity [1 0 0 1 0 0] — d is the identity's 1, not 0.
    double a = 1.0, b = 0.0, c = 0.0, d = 1.0, e = 0.0, f = 0.0;
};

// result = m2 x m1 (apply m1 then m2), per PDF 8.3.4 — same convention as the
// redaction engine's concat.
WalkMat walkMatConcat(const WalkMat& m1, const WalkMat& m2) {
    WalkMat r;
    r.a = m1.a * m2.a + m1.b * m2.c;
    r.b = m1.a * m2.b + m1.b * m2.d;
    r.c = m1.c * m2.a + m1.d * m2.c;
    r.d = m1.c * m2.b + m1.d * m2.d;
    r.e = m1.e * m2.a + m1.f * m2.c + m2.e;
    r.f = m1.e * m2.b + m1.f * m2.d + m2.f;
    return r;
}

// Best-effort text snippet: content-stream strings are raw encoded bytes, so
// only fully printable-ASCII runs are quoted (hex/encoded runs stay
// "unlabeled" rather than guessing an encoding). Bounded length.
QString snippetFromStrings(const PoDoFo::PdfVariantStack& stack, bool isArray) {
    QByteArray acc;
    auto takeString = [&](const PoDoFo::PdfObject& o) {
        if (!o.IsString()) return;
        const std::string_view s = o.GetString().GetString();
        for (const char ch : s) {
            const unsigned char u = static_cast<unsigned char>(ch);
            if (u < 0x20 || u > 0x7E) return;   // not printable ASCII — no guess
        }
        acc.append(s.data(), static_cast<qsizetype>(s.size()));
    };
    if (isArray && stack.size() >= 1 && stack[0].IsArray()) {
        for (const auto& item : stack[0].GetArray())
            takeString(item);
    } else if (stack.size() >= 1) {
        takeString(stack[0]);
    }
    if (acc.isEmpty()) return {};
    QString snip = QString::fromLatin1(acc.left(32));
    if (acc.size() > 32) snip += QStringLiteral("…");
    return snip;
}

// One page's content stream: fill `pos` for every wanted MCID found. Returns
// the number of DISTINCT wanted spans resolved. `budget` is the remaining
// global span budget; when it runs out the walk stops early (returns negative
// to signal truncation).
qint64 markedSpanKey(int pageIdx, int mcid) {
    return static_cast<qint64>(pageIdx) * 1000000 + mcid;
}

// The MCIDs the structure tree references on ONE page — the only ids the
// page's content walk has reason to track.
QSet<int> wantedSetFor(int pageIdx, const QList<McidRef>& refs) {
    QSet<int> s;
    for (const McidRef& r : refs)
        if (r.page == pageIdx) s.insert(r.mcid);
    return s;
}

int collectMarkedSpanPositions(PoDoFo::PdfMemDocument& doc, int pageIdx,
                               const QSet<int>& wantedMcids,
                               QHash<qint64, MarkedSpanPos>& pos,
                               int& budget, bool& truncated) {
    PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pageIdx);

    PoDoFo::PdfContentReaderArgs args;
    // Form XObjects are deliberately NOT followed (their MCIDs live in the
    // form's StructParents namespace — see the block comment above).
    args.Flags = PoDoFo::PdfContentReaderFlags::SkipFollowFormXObjects;
    PoDoFo::PdfContentStreamReader reader(page, args);
    PoDoFo::PdfContent content;

    WalkMat tm;      // text matrix (Tm)
    WalkMat tlm;     // text line matrix (Tlm)
    WalkMat ctm;     // canvas CTM (q/Q/cm)
    std::vector<WalkMat> ctmStack;
    double leading = 0.0;
    int64_t currentMcid = -1;
    int resolved = 0;

    while (reader.TryReadNext(content)) {
        if (content.GetType() != PoDoFo::PdfContentType::Operator) continue;
        const std::string_view kw = content.GetKeyword();
        const auto& stack = content.GetStack();

        // ── canvas state ────────────────────────────────────────────────
        if (kw == "q") {
            ctmStack.push_back(ctm);
            continue;
        }
        if (kw == "Q") {
            if (!ctmStack.empty()) { ctm = ctmStack.back(); ctmStack.pop_back(); }
            continue;
        }
        if (kw == "cm" && stack.size() >= 6) {
            WalkMat m;
            if (stack[5].IsNumberOrReal()) m.a = stack[5].GetReal();
            if (stack[4].IsNumberOrReal()) m.b = stack[4].GetReal();
            if (stack[3].IsNumberOrReal()) m.c = stack[3].GetReal();
            if (stack[2].IsNumberOrReal()) m.d = stack[2].GetReal();
            if (stack[1].IsNumberOrReal()) m.e = stack[1].GetReal();
            if (stack[0].IsNumberOrReal()) m.f = stack[0].GetReal();
            ctm = walkMatConcat(m, ctm);
            continue;
        }

        // ── text position state (PDF 9.4.2) ─────────────────────────────
        if (kw == "BT") { tm = WalkMat{}; tlm = WalkMat{}; continue; }
        if (kw == "TL" && stack.size() >= 1 && stack[0].IsNumberOrReal()) {
            leading = stack[0].GetReal();
            continue;
        }
        if (kw == "Tm" && stack.size() >= 6) {
            WalkMat m;
            if (stack[5].IsNumberOrReal()) m.a = stack[5].GetReal();
            if (stack[4].IsNumberOrReal()) m.b = stack[4].GetReal();
            if (stack[3].IsNumberOrReal()) m.c = stack[3].GetReal();
            if (stack[2].IsNumberOrReal()) m.d = stack[2].GetReal();
            if (stack[1].IsNumberOrReal()) m.e = stack[1].GetReal();
            if (stack[0].IsNumberOrReal()) m.f = stack[0].GetReal();
            tm = m; tlm = m;
            continue;
        }
        if ((kw == "Td" || kw == "TD") && stack.size() >= 2) {
            double tx = 0.0, ty = 0.0;
            if (stack[1].IsNumberOrReal()) tx = stack[1].GetReal();
            if (stack[0].IsNumberOrReal()) ty = stack[0].GetReal();
            WalkMat tr; tr.e = tx; tr.f = ty;
            tlm = walkMatConcat(tr, tlm);
            tm = tlm;
            if (kw == "TD") leading = -ty;
            continue;
        }
        if (kw == "T*") {
            WalkMat tr; tr.f = -leading;
            tlm = walkMatConcat(tr, tlm);
            tm = tlm;
            continue;
        }

        // ── marked content ──────────────────────────────────────────────
        if (kw == "BDC" && stack.size() >= 2) {
            int64_t found = -1;
            auto inDict = [&](const PoDoFo::PdfObject& o) -> bool {
                if (o.IsDictionary()) {
                    const PoDoFo::PdfObject* m = o.GetDictionary().FindKey("MCID");
                    if (m && m->IsNumber()) { found = m->GetNumber(); return true; }
                }
                return false;
            };
            // stack[0]: the property dict (inline form) or the property NAME
            // (named form → resolve via the page's /Resources /Properties).
            if (!inDict(stack[0]) && stack[0].IsName()) {
                const PoDoFo::PdfObject* props = nullptr;
                try {
                    auto& res = page.GetResources();
                    props = res.GetDictionary().FindKey("Properties");
                } catch (const PoDoFo::PdfError&) { props = nullptr; }
                if (props && props->IsDictionary()) {
                    const PoDoFo::PdfObject* sub = props->GetDictionary().FindKey(
                        PoDoFo::PdfName(stack[0].GetName().GetString()));
                    if (sub) {
                        if (sub->IsReference())
                            sub = &doc.GetObjects().MustGetObject(sub->GetReference());
                        if (sub) inDict(*sub);
                    }
                }
            }
            // Defensive: the redaction engine reads the SAME pair from
            // stack[1] first — honour that form too (property pushed first).
            if (found < 0 && stack.size() >= 2) inDict(stack[1]);
            currentMcid = (found >= 0 && wantedMcids.contains(static_cast<int>(found)))
                              ? found : -1;
            continue;
        }
        if (kw == "EMC") { currentMcid = -1; continue; }

        // ── text showing: record the line origin for the active MCID ────
        const bool isTextOp = (kw == "Tj" || kw == "TJ" || kw == "'" || kw == "\"");
        if (!isTextOp) continue;
        if (currentMcid >= 0) {
            const qint64 key = markedSpanKey(pageIdx, static_cast<int>(currentMcid));
            MarkedSpanPos& p = pos[key];
            if (!p.hasY) {
                // Baseline line-origin (0,0) through Tm, then through the CTM.
                const double px = tm.e, py = tm.f;
                p.topY = ctm.b * px + ctm.d * py + ctm.f;
                p.hasY = true;
                ++resolved;
                if (--budget < 0) { truncated = true; return -1; }
            }
            if (p.text.isEmpty()) {
                // stack[0] holds the string for Tj / ' / " (last-pushed
                // operand); TJ keeps its strings in the array operand.
                p.text = snippetFromStrings(stack, kw == "TJ");
            }
        }
        // ' and " perform a T* before showing — advance the line matrices so
        // the NEXT line origin is right (operator side effects, PDF 9.4.1).
        if (kw == "'" || kw == "\"") {
            WalkMat tr; tr.f = -leading;
            tlm = walkMatConcat(tr, tlm);
            tm = tlm;
        }
    }
    return resolved;
}

} // namespace

ReadingOrderResult analyzeReadingOrder(const QString& path, int maxStructDepth) {
    ReadingOrderResult r;
    // §4 #19 fail-safe: a nonsensical cap falls back to the named default —
    // the override can only ever RAISE the bound, never lower it below the
    // shipped default (clamped again at the settings seam).
    if (maxStructDepth < gp::kReadingOrderMaxStructDepth
        || maxStructDepth > gp::kReadingOrderStructDepthLimit)
        maxStructDepth = gp::kReadingOrderMaxStructDepth;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());

        const PoDoFo::PdfObject* root =
            doc.GetCatalog().GetDictionary().FindKey("StructTreeRoot");
        if (!root) return r;  // not tagged
        r.tagged = true;

        QList<StructElem> elems;
        QList<McidRef> mcidRefs;
        bool depthTruncated = false;
        collectStructElems(doc, root, elems, 0, -1, &mcidRefs, maxStructDepth,
                           &depthTruncated);
        if (depthTruncated) {
            r.depthTruncated = true;
            r.depthLimit = maxStructDepth;
        }
        r.elementCount = elems.size();

        // ── Element-level analysis (§9.14, unchanged) ───────────────────
        if (elems.size() >= 2) {
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
        }

        // ── MCID-level analysis (PARITY-SCORECARD-2026-09-30 §4 #3) ─────
        // The same displacement rule, applied to the MARKED-CONTENT the
        // structure references: each struct-referenced MCID gets its text
        // position from the page content stream (bounded walk), so text-level
        // inversions — interleaved paragraphs, out-of-order lines inside a
        // correctly-tagged structure — are visible with page+MCID provenance.
        if (mcidRefs.size() >= 2) {
            // Distinct pages first — one content-stream walk per page.
            QSet<int> pages;
            for (const McidRef& ref : mcidRefs)
                if (ref.page >= 0) pages.insert(ref.page);

            QHash<qint64, MarkedSpanPos> pos;
            int budget = gp::kReadingOrderMaxMarkedContentSpans;
            bool truncated = false;
            for (int pageIdx : pages) {
                try {
                    if (collectMarkedSpanPositions(doc, pageIdx, wantedSetFor(pageIdx, mcidRefs),
                                                   pos, budget, truncated) < 0)
                        break;   // budget exhausted — walk stopped early
                } catch (const PoDoFo::PdfError&) {
                    // A broken page contributes no positions — its spans keep
                    // structural order below; never fatal.
                }
            }
            r.markedContentTruncated = truncated;
            r.markedSpansAnalyzed = static_cast<int>(pos.size());

            const int m = mcidRefs.size();
            auto visualKeyOf = [&](int i) -> double {
                const McidRef& ref = mcidRefs[i];
                const MarkedSpanPos* p = pos.contains(markedSpanKey(ref.page, ref.mcid))
                                             ? &pos[markedSpanKey(ref.page, ref.mcid)]
                                             : nullptr;
                // Same convention as the element level: spans without a
                // resolved position keep their structural order via the
                // struct-index nudge (only positioned spans can be flagged).
                const double y = (p && p->hasY) ? p->topY
                                                : (1.0e6 - static_cast<double>(i));
                const int pg = (ref.page < 0) ? 100000 : ref.page;
                return static_cast<double>(pg) * 1.0e7 - y;
            };
            QVector<int> order(m);
            for (int i = 0; i < m; ++i) order[i] = i;
            std::stable_sort(order.begin(), order.end(),
                             [&](int a, int b) { return visualKeyOf(a) < visualKeyOf(b); });
            QVector<int> mPos(m, 0);
            for (int p2 = 0; p2 < m; ++p2) mPos[order[p2]] = p2;

            for (int i = 0; i < m; ++i) {
                if (std::abs(i - mPos[i]) > gp::kReadingOrderSlotTolerance) {
                    const McidRef& ref = mcidRefs[i];
                    const MarkedSpanPos* p = pos.contains(markedSpanKey(ref.page, ref.mcid))
                                                 ? &pos[markedSpanKey(ref.page, ref.mcid)]
                                                 : nullptr;
                    const QString snippet =
                        (p && !p->text.isEmpty())
                            ? QObject::tr("\"%1\"").arg(p->text)
                            : QObject::tr("unlabeled");
                    r.issues << QObject::tr("MCID %1 (%2) at structure position %3 maps to visual position %4%5")
                                    .arg(ref.mcid)
                                    .arg(snippet)
                                    .arg(i + 1)
                                    .arg(mPos[i] + 1)
                                    .arg(ref.page >= 0 ? QObject::tr(" (page %1)").arg(ref.page + 1)
                                                       : QString());
                    r.issuePages << ref.page;
                }
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
    m_submittedReadingOrderPath = path;

    // §4 #19: optional settings override for the struct-walk depth cap.
    // Anything outside [60, 500] (or unset / non-numeric) falls back to the
    // fail-safe default — the override can only RAISE the bound.
    int maxStructDepth = gp::kReadingOrderMaxStructDepth;
    const QVariant overrideVal =
        QSettings().value(QStringLiteral("accessibility/readingOrderMaxDepth"));
    if (overrideVal.isValid()) {
        bool ok = false;
        const int requested = overrideVal.toInt(&ok);
        if (ok)
            maxStructDepth = std::clamp(requested,
                                        gp::kReadingOrderMaxStructDepth,
                                        gp::kReadingOrderStructDepthLimit);
    }

    m_readingOrderWatcher->setFuture(QtConcurrent::run([path, maxStructDepth]() {
        return analyzeReadingOrder(path, maxStructDepth);
    }));
}

void PdfAValidationPanel::onReadingOrderFinished() {
    if (!m_readingOrderWatcher || m_readingOrderWatcher->isCanceled()) return;
    // ARC06: identity tie — a reading-order run submitted for a document that
    // is no longer current is discarded (setDocument cancels it; this also
    // covers a result that raced the cancellation).
    if (m_submittedReadingOrderPath != m_currentDocPath) return;
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

    // Bounded-sample disclosures (PARITY-SCORECARD-2026-09-30 §4 #3 and #19):
    // a walk that stopped at a cap is never silent, and a truncated analysis
    // never reads as "OK".
    if (r.depthTruncated) {
        m_issuesLayout->addWidget(issueRow(this, QStringLiteral("RO"),
            tr("Struct tree deeper than %1 — analysis truncated; results may be incomplete.")
                .arg(r.depthLimit),
            /*err=*/false, /*pageNumber=*/-1));
    }
    if (r.markedContentTruncated) {
        m_issuesLayout->addWidget(issueRow(this, QStringLiteral("RO"),
            tr("Marked-content sample truncated at %1 spans — results may be incomplete.")
                .arg(r.markedSpansAnalyzed),
            /*err=*/false, /*pageNumber=*/-1));
    }

    if (r.issues.isEmpty() && !r.depthTruncated && !r.markedContentTruncated) {
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
    if (r.issues.isEmpty()) {
        // Truncated with no findings: honest partial result, never "OK".
        m_statusLabel->setText(
            tr("%1 elements. Analysis truncated at a bound — results may be incomplete.")
                .arg(r.elementCount));
    } else {
        m_statusLabel->setText(tr("✗ %1 elements. %2 reading-order issue(s).").arg(r.elementCount).arg(r.issues.size()));
    }
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
