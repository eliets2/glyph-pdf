// SPDX-License-Identifier: Apache-2.0
// SWEEP-W3 UX audit (2026-09-20) — end-to-end real-flow completion + honesty.
//
// Mission (docs/audit/SWEEP-W3-UX-2026-09-20.md): can a FIRST-TIME user
// complete each task? Every dead-end, silent state, and dishonest message is
// a finding. Evidence = per-step transcript (qInfo "UXAUDIT:" lines) +
// landed-state assertions on real artifacts. Drives the PRODUCTION routes
// (welcome card → signal → host → dialog → load → task) with the R16
// route-test pattern: AA_DontUseNativeDialogs + deadline-budgeted modal
// drivers, no sleeps.
//
// Flows in THIS file (batch A):
//   F1  open → annotate → save, incl. the Save-As overwrite guard
//   F2  welcome → Merge (multi-file) + Batch run + Preset Pipeline
//       (create preset → run via preset → re-run identical)
//   F3  redaction: mark → apply → proof report → export, incl. what the user
//       sees on a partial failure (sanitization failure)
//   F8  find & replace: regex + replace-all count honesty (WP-R07 contract)
#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QSettings>
#include <QStandardPaths>
#include <QPdfDocument>
#include <QTimer>
#include <QDir>
#include <QImage>
#include <QPdfWriter>
#include <QPainter>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QAbstractButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QProgressDialog>

#include <memory>
#include <functional>

#include <podofo/podofo.h>

#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "core/AnnotationTypes.h"
#include "engines/DocumentSession.h"   // F7b: setReadOnly on the real session
#include "shell/Ribbon.h"
#include "shell/StatusBar.h"
#include "ui/WelcomeWidget.h"
#include "ui/PdfViewerWidget.h"
#include "ui/FindReplaceDialog.h"
#include "modes/BatchMode.h"
#include "modes/RedactMode.h"
#include "modes/OCRMode.h"
#include "modes/AccessibilityPanel.h"
#include "ui/SignatureDialog.h"
#include "ui/SigningRequestDialog.h"
#include "ui/SigningProgressPanel.h"
#include "ui/RecipientPickerDialog.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "engines/PdfEditorEngine.h"
#include "engines/SignatureManager.h"
#include "core/SigningRequestModel.h"
#include "core/Capability.h"


using gp::MainWindow;

#ifdef DrawText
#undef DrawText
#endif
// Win32 minwindef.h defines GetObject as GetObjectW — PoDoFo's
// PdfIndirectObjectList::GetObject collides. Drop the macro for this TU.
#ifdef GetObject
#undef GetObject
#endif

namespace {

// ── transcript ───────────────────────────────────────────────────────────────
void step(const QString &msg) { qInfo().noquote() << "UXAUDIT:" << msg; }

// ── slot-scoped modal drivers ────────────────────────────────────────────────
// Every modal driver below is a self-rescheduling QTimer::singleShot(0) chain
// with a deadline budget of 60-120s. A fast slot returns long before its
// drivers' budgets expire, so the chains used to SURVIVE into the next test
// function and consumed ITS modals — the full-harness failures of flow2a/2b/3
// at the merged tip (uxflows.txt 2026-09-23): flow1's stale 'No'-clicker
// answered flow2a's "Open Merged PDF" completion modal before flow2a's
// capturer could read it; flow2a's stale capturer closed flow2b's preset-name
// dialog before flow2b's driveModalDialog matched it ('presetDialogSeen'
// false); flow2b's 300s defensive capturer closed flow3's apply dialog (the
// apply never ran, 'QFileInfo::exists(redactedOut)' false). Each driver now
// captures the driver epoch at creation, and init() bumps the epoch before
// every test function: a chain whose slot has ended exits at its next tick.
int g_driverEpoch = 0;   // GUI-thread only (all drivers run on the event loop)

// ── fixtures ─────────────────────────────────────────────────────────────────
void makePdf(const QString &path, const QString &marker)
{
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    QPainter p(&w);
    p.drawText(100, 100, marker);
    p.end();
    QVERIFY(QFileInfo::exists(path));
}

// A PDF whose text sits at a KNOWN PoDoFo coordinate (TestRedactApplyMarks
// idiom) so a regex redaction mark can be verified against content streams.
void makePdfWithSecret(const QString &path, const QString &secret)
{
    try {
        PoDoFo::PdfMemDocument doc;
        auto &page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        auto &font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        painter.TextState.SetFont(font, 12.0);
        painter.DrawText(secret.toUtf8().constData(), 50, 700);
        painter.DrawText("harmless filler", 50, 650);
        painter.FinishDrawing();
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception &e) {
        qWarning() << "makePdfWithSecret failed:" << e.what();
    }
    QVERIFY(QFileInfo::exists(path));
}

bool pdfTextContains(const QString &path, const QByteArray &needle)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        for (int pi = 0; pi < static_cast<int>(doc.GetPages().GetCount()); ++pi) {
            auto &page = doc.GetPages().GetPageAt(pi);
            auto *co = page.GetContents();
            if (!co) continue;
            PoDoFo::charbuff buf;
            co->CopyTo(buf);
            if (QByteArray(buf.data(), static_cast<int>(buf.size())).contains(needle))
                return true;
        }
    } catch (const std::exception &) {
        return false;
    }
    return false;
}

// Reads the fixture's tiling-pattern stream (page /Resources /Pattern /P1)
// directly — the secret there is deliberately invisible to pdfTextContains
// (which reads PAGE contents only): that unreachability is exactly what the
// G2 guard refuses over.
bool patternStreamContains(const QString &path, const QByteArray &needle)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        auto &page = doc.GetPages().GetPageAt(0);
        const auto *patterns = page.GetResources().GetObject().GetDictionary()
                                   .FindKey(PoDoFo::PdfName("Pattern"));
        if (!patterns) return false;
        if (patterns->IsReference())
            patterns = &doc.GetObjects().MustGetObject(patterns->GetReference());
        if (!patterns || !patterns->IsDictionary()) return false;
        for (const auto &kv : patterns->GetDictionary()) {
            const PoDoFo::PdfObject *pat = &kv.second;
            if (pat->IsReference())
                pat = &doc.GetObjects().MustGetObject(pat->GetReference());
            if (!pat || !pat->HasStream()) continue;
            PoDoFo::charbuff buf;
            pat->GetStream()->CopyTo(buf);
            if (QByteArray(buf.data(), static_cast<int>(buf.size())).contains(needle))
                return true;
        }
    } catch (const std::exception &) {
        return false;
    }
    return false;
}

// ── Deterministic modal drivers (TestWelcomeRoutes pattern) ──────────────────

void pickFilesInSequenceStep(const QStringList &paths, QElapsedTimer deadline, int budgetMs,
                             int epoch)
{
    if (epoch != g_driverEpoch) return;             // slot ended — stop driving
    if (paths.isEmpty()) return;
    if (deadline.elapsed() >= budgetMs) {
        qWarning() << "UXAUDIT: pickFilesInSequence budget exhausted for" << paths.first();
        return;
    }
    QTimer::singleShot(0, [paths, deadline, budgetMs, epoch] {
        if (epoch != g_driverEpoch) return;         // slot ended — stop driving
        if (auto *dlg = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
            const QString path = paths.first();
            const QStringList selected = dlg->selectedFiles();
            const bool delivered = selected.contains(path)
                || (!selected.isEmpty()
                    && QFileInfo(selected.last()).canonicalFilePath()
                           == QFileInfo(path).canonicalFilePath());
            if (!delivered) {
                dlg->selectFile(path);
                pickFilesInSequenceStep(paths, deadline, budgetMs, epoch);
                return;
            }
            QMetaObject::invokeMethod(dlg, "accept");
            if (paths.size() > 1)
                pickFilesInSequenceStep(paths.mid(1), deadline, budgetMs, epoch);
            return;
        }
        pickFilesInSequenceStep(paths, deadline, budgetMs, epoch);
    });
}

// Drives a MULTI-SELECT open dialog (matched by window title) by writing the
// quoted absolute paths into the dialog's file-name edit - the standard
// multi-file syntax the non-native dialog parses on accept - then accepting.
void pickMultiFilesByTitleStep(const QString &title, const QStringList &paths,
                               QElapsedTimer deadline, int budgetMs, int epoch)
{
    if (epoch != g_driverEpoch) return;             // slot ended — stop driving
    if (deadline.elapsed() >= budgetMs) {
        qWarning() << "UXAUDIT: pickMultiFilesByTitle budget exhausted for" << title;
        return;
    }
    QTimer::singleShot(0, [title, paths, deadline, budgetMs, epoch] {
        if (epoch != g_driverEpoch) return;         // slot ended — stop driving
        if (auto *dlg = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
            if (dlg->windowTitle() == title && dlg->acceptMode() == QFileDialog::AcceptOpen
                && dlg->selectedFiles().size() < paths.size()) {
                QStringList quoted;
                for (const QString &p : paths)
                    quoted << QLatin1Char('"') + p + QLatin1Char('"');
                if (auto *edit = dlg->findChild<QLineEdit *>(QStringLiteral("fileNameEdit")))
                    edit->setText(quoted.join(QLatin1Char(' ')));
                QMetaObject::invokeMethod(dlg, "accept");
                return;
            }
        }
        pickMultiFilesByTitleStep(title, paths, deadline, budgetMs, epoch);
    });
}

void pickMultiFilesByTitle(const QString &title, const QStringList &paths, int budgetMs = 60000)
{
    QElapsedTimer deadline;
    deadline.start();
    pickMultiFilesByTitleStep(title, paths, deadline, budgetMs, g_driverEpoch);
}

void pickFilesInSequence(const QStringList &paths, int budgetMs = 60000)
{
    QElapsedTimer deadline;
    deadline.start();
    pickFilesInSequenceStep(paths, deadline, budgetMs, g_driverEpoch);
}

// Captures the TEXT of the next non-file modal, then dismisses it. The
// captured text is the honesty evidence: what the user was actually told.
// `skipObjectName`: a task dialog that a parallel driver owns (skipped, and
// NOT closed).
void captureModalTextStep(QString *text, QString *title, bool *seen,
                          const QString &skipObjectName,
                          QElapsedTimer deadline, int budgetMs, int epoch)
{
    if (epoch != g_driverEpoch) return;             // slot ended — stop driving
    if (deadline.elapsed() >= budgetMs) {
        qWarning() << "UXAUDIT: captureModalText budget exhausted";
        return;
    }
    QTimer::singleShot(0, [text, title, seen, skipObjectName, deadline, budgetMs, epoch] {
        if (epoch != g_driverEpoch) return;         // slot ended — stop driving
        if (QWidget *w = QApplication::activeModalWidget()) {
            // Never touch progress dialogs: closing one CANCELS the worker.
            if (qobject_cast<QProgressDialog *>(w)) {
                step(QStringLiteral("(transit: progress dialog '%1')")
                         .arg(w->windowTitle()));
                captureModalTextStep(text, title, seen, skipObjectName, deadline, budgetMs, epoch);
                return;
            }
            if (auto *box = qobject_cast<QMessageBox *>(w)) {
                if (text) *text = box->text() + QLatin1Char('\n') + box->informativeText();
                if (title) *title = box->windowTitle();
                if (seen) *seen = true;
                step(QStringLiteral("modal seen: title='%1' text='%2'")
                         .arg(box->windowTitle(),
                              (box->text() + QLatin1Char(' ') + box->informativeText())
                                  .left(300)));
                box->close();
                return;
            }
            if (!skipObjectName.isEmpty()
                && (w->objectName() == skipObjectName || w->findChild<QWidget *>(skipObjectName))) {
                captureModalTextStep(text, title, seen, skipObjectName, deadline, budgetMs, epoch);
                return;
            }
            if (qobject_cast<QFileDialog *>(w)) {
                // A file dialog a pick driver owns - never close it.
                captureModalTextStep(text, title, seen, skipObjectName, deadline, budgetMs, epoch);
                return;
            }
            if (auto *dlg = qobject_cast<QDialog *>(w)) {
                if (title) *title = dlg->windowTitle();
                // Task dialogs (ErrorDialog & co) carry their message in
                // child labels, not in QMessageBox::text() — capture them so
                // the transcript records WHAT the user was actually told.
                QString body;
                const auto labels = dlg->findChildren<QLabel *>();
                for (QLabel *l : labels)
                    if (!l->text().isEmpty())
                        body += l->text() + QLatin1Char(' ');
                body = body.trimmed();
                if (text) *text = body;
                if (seen) *seen = true;
                step(QStringLiteral("task dialog seen: title='%1' text='%2'")
                         .arg(dlg->windowTitle(), body.left(300)));
                dlg->close();
                return;
            }
        }
        captureModalTextStep(text, title, seen, skipObjectName, deadline, budgetMs, epoch);
    });
}

void captureModalText(QString *text, QString *title = nullptr, bool *seen = nullptr,
                      const QString &skipObjectName = QString(), int budgetMs = 60000)
{
    QElapsedTimer deadline;
    deadline.start();
    captureModalTextStep(text, title, seen, skipObjectName, deadline, budgetMs, g_driverEpoch);
}

void clickPromptButtonStep(const QString &text, bool *clicked,
                           QElapsedTimer deadline, int budgetMs, int epoch)
{
    if (epoch != g_driverEpoch) return;             // slot ended — stop driving
    if (deadline.elapsed() >= budgetMs) {
        // Budget out silently: no such prompt ever appeared (callers treat
        // clicked==false as "the guard never fired").
        return;
    }
    QTimer::singleShot(0, [text, clicked, deadline, budgetMs, epoch] {
        if (epoch != g_driverEpoch) return;         // slot ended — stop driving
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            const auto buttons = box->findChildren<QAbstractButton *>();
            for (QAbstractButton *b : buttons) {
                if (b->text().remove(QLatin1Char('&')) == text) {
                    step(QStringLiteral("clicked prompt button '%1' on '%2' (question: '%3')")
                             .arg(text, box->windowTitle(), box->text().left(150)));
                    if (clicked) *clicked = true;
                    b->click();
                    return;
                }
            }
        }
        clickPromptButtonStep(text, clicked, deadline, budgetMs, epoch);
    });
}

// Clicks the named button on the next QMessageBox prompt; reports whether the
// prompt appeared at all. budgetMs must outlive the file dialog driver that
// triggers the prompt (drive the prompt for the WHOLE flow window).
void clickPromptButton(const QString &text, bool *clicked, int budgetMs = 120000)
{
    QElapsedTimer deadline;
    deadline.start();
    clickPromptButtonStep(text, clicked, deadline, budgetMs, g_driverEpoch);
}

// Waits for a dialog to become the active modal, matched either by its own
// objectName or by containing a child with that objectName, then runs the
// mutator INSIDE its modal loop (never closes it — the mutator decides).
template <typename Prep>
void driveModalDialogStep(const QString &matchName, Prep prep, bool *seen,
                          QElapsedTimer deadline, int budgetMs, int epoch)
{
    if (epoch != g_driverEpoch) return;             // slot ended — stop driving
    if (deadline.elapsed() >= budgetMs) {
        qWarning() << "UXAUDIT: driveModalDialog('%s') budget exhausted"
                   << qPrintable(matchName);
        return;
    }
    QTimer::singleShot(0, [matchName, prep, seen, deadline, budgetMs, epoch] {
        if (epoch != g_driverEpoch) return;         // slot ended — stop driving
        QWidget *w = QApplication::activeModalWidget();
        const bool match = w
            && (w->objectName() == matchName
                || w->windowTitle() == matchName
                || w->findChild<QWidget *>(matchName));
        if (match) {
            step(QStringLiteral("driving dialog matched by '%1' (title='%2')")
                     .arg(matchName, w->windowTitle()));
            prep(w);
            if (seen) *seen = true;
            return;
        }
        driveModalDialogStep(matchName, prep, seen, deadline, budgetMs, epoch);
    });
}

template <typename Prep>
void driveModalDialog(const QString &matchName, Prep prep, bool *seen = nullptr,
                      int budgetMs = 90000)
{
    QElapsedTimer deadline;
    deadline.start();
    driveModalDialogStep(matchName, prep, seen, deadline, budgetMs, g_driverEpoch);
}

// Rejects the next QFileDialog that becomes active (after a declined
// overwrite the save dialog STAYS OPEN for another choice - correct UX; the
// driver must dismiss it or the nested modal loop never exits).
void rejectFileDialogStep(QElapsedTimer deadline, int budgetMs, int epoch)
{
    if (epoch != g_driverEpoch) return;             // slot ended — stop driving
    if (deadline.elapsed() >= budgetMs) {
        qWarning() << "UXAUDIT: rejectFileDialog budget exhausted";
        return;
    }
    QTimer::singleShot(0, [deadline, budgetMs, epoch] {
        if (epoch != g_driverEpoch) return;         // slot ended — stop driving
        if (auto *dlg = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
            step("decline path: the save dialog stayed open for another choice - dismissing");
            dlg->reject();
            return;
        }
        rejectFileDialogStep(deadline, budgetMs, epoch);
    });
}

void rejectFileDialogWhenVisible(int budgetMs = 120000)
{
    QElapsedTimer deadline;
    deadline.start();
    rejectFileDialogStep(deadline, budgetMs, g_driverEpoch);
}

// Captures the text of the next QMessageBox whose title contains
// titleNeedle, then clicks buttonText (the natural user choice - close() on
// a custom-button box leaves exec() unresolved).
void capturePromptAndClickStep(const QString &titleNeedle, const QString &buttonText,
                               QString *text, QString *title,
                               QElapsedTimer deadline, int budgetMs, int epoch)
{
    if (epoch != g_driverEpoch) return;             // slot ended — stop driving
    if (deadline.elapsed() >= budgetMs) {
        qWarning() << "UXAUDIT: capturePromptAndClick budget exhausted for" << titleNeedle;
        return;
    }
    QTimer::singleShot(0, [titleNeedle, buttonText, text, title, deadline, budgetMs, epoch] {
        if (epoch != g_driverEpoch) return;         // slot ended — stop driving
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            if (box->windowTitle().contains(titleNeedle)) {
                if (text) *text = box->text() + box->informativeText();
                if (title) *title = box->windowTitle();
                step(QStringLiteral("modal seen: title='%1' text='%2'")
                         .arg(box->windowTitle(),
                              (box->text() + box->informativeText()).left(300)));
                const auto buttons = box->findChildren<QAbstractButton *>();
                for (QAbstractButton *b : buttons) {
                    if (b->text().remove(QLatin1Char('&')) == buttonText) {
                        b->click();
                        return;
                    }
                }
                box->close();
                return;
            }
        }
        capturePromptAndClickStep(titleNeedle, buttonText, text, title, deadline, budgetMs, epoch);
    });
}

void capturePromptAndClick(const QString &titleNeedle, const QString &buttonText,
                           QString *text, QString *title, int budgetMs = 120000)
{
    QElapsedTimer deadline;
    deadline.start();
    capturePromptAndClickStep(titleNeedle, buttonText, text, title, deadline, budgetMs,
                              g_driverEpoch);
}
// Pattern-text fixture for the F3b refusal pin: the page paints its PUBLIC
// text as an ordinary content-stream op AND carries a tiling pattern whose
// stream holds a second, secret text. The excision canvas walk cannot reach
// pattern streams, so the merged redaction-gaps lane (G2, audit
// REDACTION-RESEARCH-2026-09-21 §2.4) made the engine REFUSE the whole run
// with a named reason instead of painting a black box over live data.
// (Mirror of TestRedactTransaction::makePatternSecretPdf.)
bool makePatternTextPdf(const QString &path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        auto &page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        auto &font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        auto &pattern = doc.GetObjects().CreateDictionaryObject();
        pattern.GetDictionary().AddKey(PoDoFo::PdfName("Type"), PoDoFo::PdfName("Pattern"));
        pattern.GetDictionary().AddKey(PoDoFo::PdfName("PatternType"), PoDoFo::PdfObject(int64_t(1)));
        pattern.GetDictionary().AddKey(PoDoFo::PdfName("PaintType"), PoDoFo::PdfObject(int64_t(1)));
        pattern.GetDictionary().AddKey(PoDoFo::PdfName("TilingType"), PoDoFo::PdfObject(int64_t(1)));
        PoDoFo::PdfArray bbox;
        bbox.Add(0.0); bbox.Add(0.0); bbox.Add(100.0); bbox.Add(100.0);
        pattern.GetDictionary().AddKey(PoDoFo::PdfName("BBox"), bbox);
        pattern.GetDictionary().AddKey(PoDoFo::PdfName("XStep"), PoDoFo::PdfObject(80.0));
        pattern.GetDictionary().AddKey(PoDoFo::PdfName("YStep"), PoDoFo::PdfObject(80.0));
        auto &fontMap = doc.GetObjects().CreateDictionaryObject();
        fontMap.GetDictionary().AddKey(PoDoFo::PdfName("F1"),
                                       font.GetObject().GetIndirectReference());
        pattern.GetDictionary().AddKey(PoDoFo::PdfName("Resources"),
                                       fontMap.GetIndirectReference());
        const char *patternContent = "BT /F1 14 Tf 10 30 Td (PatternSecretOmega) Tj ET\n";
        pattern.GetOrCreateStream().SetData(PoDoFo::bufferview(
            patternContent, std::strlen(patternContent)));
        auto &patternMap = doc.GetObjects().CreateDictionaryObject();
        patternMap.GetDictionary().AddKey(PoDoFo::PdfName("P1"),
                                          pattern.GetIndirectReference());
        auto &pageRes = doc.GetObjects().CreateDictionaryObject();
        pageRes.GetDictionary().AddKey(PoDoFo::PdfName("Font"),
                                       fontMap.GetIndirectReference());
        pageRes.GetDictionary().AddKey(PoDoFo::PdfName("Pattern"),
                                       patternMap.GetIndirectReference());
        page.GetObject().GetDictionary().AddKey(PoDoFo::PdfName("Resources"),
                                                pageRes.GetIndirectReference());
        auto &content = doc.GetObjects().CreateDictionaryObject();
        const char *pageContent =
            "BT /F1 12 Tf 50 700 Td (PUBLIC_KEEP_TEXT) Tj ET\n"
            "/Pattern cs /P1 scn 0 0 595 842 re f\n";
        content.GetOrCreateStream().SetData(PoDoFo::bufferview(
            pageContent, std::strlen(pageContent)));
        page.GetObject().GetDictionary().AddKey(PoDoFo::PdfName("Contents"),
                                                content.GetIndirectReference());
        doc.Save(path.toUtf8().constData());
        return QFileInfo::exists(path);
    } catch (const std::exception &e) {
        qWarning() << "makePatternTextPdf failed:" << e.what();
        return false;
    }
}

QPushButton *buttonByText(QWidget *w, const QString &text)
{
    const auto bs = w->findChildren<QPushButton *>();
    for (QPushButton *b : bs)
        if (b->text().remove(QLatin1Char('&')) == text) return b;
    return nullptr;
}

} // namespace

class TestSweepW3UxFlows : public QObject {
    Q_OBJECT

    std::unique_ptr<MainWindow> m_win;

    QPushButton *card(const QString &task) const
    {
        return m_win->findChild<QPushButton *>(QStringLiteral("welcomeCard-") + task);
    }
    gp::Ribbon *ribbon() const { return m_win->findChild<gp::Ribbon *>(); }

    void runCardRoute(const QString &task, const QString &fixture)
    {
        QPushButton *c = card(task);
        QVERIFY2(c, qPrintable(QStringLiteral("welcome card '%1' must exist").arg(task)));
        QVERIFY2(c->isEnabled(), qPrintable(QStringLiteral("card '%1' must be available").arg(task)));
        pickFilesInSequence({ fixture });
        step(QStringLiteral("F-route: clicking welcome card '%1'").arg(task));
        c->click();
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestSweepW3UxFlows"));
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    }

    void init()
    {
        // Slot-scope the modal drivers: any chain still polling from the
        // PREVIOUS test function dies here (see g_driverEpoch above).
        ++g_driverEpoch;
        m_win = std::make_unique<MainWindow>(Bootstrapper::createContext());
        m_win->show();
        QVERIFY(m_win->pdfViewer());
        QVERIFY(m_win->findChild<WelcomeWidget *>());
    }

    void cleanup() { m_win.reset(); }

    // ── F1: open → annotate → save, incl. the Save-As overwrite guard ───────
    void flow1_openAnnotateSave_overwriteGuard()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("report.pdf");
        makePdf(a, "F1ANNOTATE");
        step(QStringLiteral("F1 start: fixture %1").arg(a));

        // 1. Open via the welcome route.
        runCardRoute("open", a);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        step("F1 step1: open landed — page count 1");

        // 2. Arm the highlight tool the way a user does after the annotate
        //    intent: the production tool dispatch → Comment tab + armed tool
        //    (the R16 annotate-route contract, exercised directly here).
        m_win->onToolActivated(QStringLiteral("highlight"));
        QTRY_VERIFY_WITH_TIMEOUT(m_win->pdfViewer()->toolMode() == ToolMode::Highlight, 5000);
        step("F1 step2: highlight tool armed via production dispatch");

        // 3. Place a highlight by dragging on the canvas (the real gesture).
        auto *viewer = m_win->pdfViewer();
        viewer->setGeometry(0, 0, 900, 700);
        QTest::mousePress(viewer, Qt::LeftButton, Qt::NoModifier, QPoint(120, 120));
        QTest::mouseMove(viewer, QPoint(320, 180));
        QTest::mouseRelease(viewer, Qt::LeftButton, Qt::NoModifier, QPoint(320, 180));
        QTest::qWait(200);
        const int placedByDrag = viewer->annotations().size();
        if (placedByDrag == 0) {
            // Offscreen drag limitation (harness, not a product finding):
            // stage through the production annotation model so the SAVE half
            // of the flow still gets exercised end to end.
            AnnotationItem mark;
            mark.mode = ToolMode::Highlight;
            mark.pageIndex = 0;
            mark.rect = QRectF(90, 90, 220, 60);
            viewer->setAnnotations({ mark });
            step("F1 step3: DRAG DID NOT PLACE (offscreen harness limitation) — "
                 "staged via the viewer annotation model");
        } else {
            step(QStringLiteral("F1 step3: drag placed %1 annotation(s)").arg(placedByDrag));
        }
        QCOMPARE(viewer->annotations().size(), 1);

        // 4. Save in place via the production save tool.
        m_win->onToolActivated(QStringLiteral("save"));
        QTest::qWait(500);
        step("F4 step4: save-in-place dispatched");
        {
            PdfViewerWidget probe;
            QVERIFY2(probe.loadDocument(a), "F1: saved artifact must reopen");
            QVERIFY2(!probe.annotations().isEmpty(),
                     "F1: the highlight must survive the save (embedded or sidecar)");
            step(QStringLiteral("F1 step4 verified: reopen shows %1 annotation(s)")
                     .arg(probe.annotations().size()));
        }

        // 5. Save As onto an EXISTING file — the overwrite guard must fire
        //    (Qt non-native save dialog carries the standard replace prompt).
        const QString existing = dir.filePath("existing.pdf");
        makePdf(existing, "EXISTING TARGET");
        const qint64 sizeBefore = QFileInfo(existing).size();
        bool yesSeen = false;
        clickPromptButton(QStringLiteral("Yes"), &yesSeen);
        pickFilesInSequence({ existing });
        m_win->onToolActivated(QStringLiteral("saveAs"));
        QTest::qWait(2500);
        QTest::qWait(2500);
        if (yesSeen) {
            PdfViewerWidget probe;
            QVERIFY2(probe.loadDocument(existing),
                     "F1: Save As after confirming overwrite must produce a loadable PDF");
            step(QStringLiteral("F1 step5 verified: guard prompt answered Yes; target "
                               "rewritten (size %1 → %2)")
                     .arg(sizeBefore).arg(QFileInfo(existing).size()));
        } else {
            const bool rewritten = QFileInfo(existing).size() != sizeBefore;
            if (rewritten)
                step("F1 step5 FINDING-CANDIDATE: no overwrite prompt seen and the "
                     "target was rewritten silently (recorded for the report)");
            else
                step("F1 step5: no prompt seen and no write — save-as declined "
                     "silently (recorded for the report)");
        }

        // 6. Decline path: answering No must leave the target untouched.
        const QString declined = dir.filePath("declined.pdf");
        makePdf(declined, "DECLINED TARGET");
        const qint64 declinedSize = QFileInfo(declined).size();
        bool noSeen = false;
        clickPromptButton(QStringLiteral("No"), &noSeen);
        rejectFileDialogWhenVisible();
        pickFilesInSequence({ declined });
        m_win->onToolActivated(QStringLiteral("saveAs"));
        QTest::qWait(2500);
        QTest::qWait(2500);
        if (noSeen) {
            QTest::qWait(500);
            QCOMPARE(QFileInfo(declined).size(), declinedSize);
            PdfViewerWidget probeDeclined;
            QVERIFY(probeDeclined.loadDocument(declined));
            step("F1 step6 verified: declining the overwrite leaves the file untouched");
        } else {
            step(QStringLiteral("F1 step6: decline prompt never appeared (noSeen=false); "
                               "target size now %1 (was %2)")
                     .arg(QFileInfo(declined).size()).arg(declinedSize));
        }
    }

    // ── F2a: welcome → Merge route completes on a REAL artifact ─────────────
    // The welcome merge card collects inputs through ONE multi-select dialog
    // (its multi-select list is not drivable offscreen - the harness records
    // the single-input route; the true MULTI-file merge is driven through the
    // batch surface in flow2b, where no dialog sits between user and engine).
    void flow2a_welcomeMerge_route()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("part1.pdf");
        makePdf(a, "MERGEPARTONE");
        const QString out = dir.filePath("combined.pdf");
        step("F2a start: welcome merge card → Select PDFs to Merge → Save Merged PDF");

        QPushButton *c = card("merge");
        QVERIFY(c && c->isEnabled());
        static QString modalText, modalTitle;
        pickFilesInSequence({ a, out });
        captureModalText(&modalText, &modalTitle);
        c->click();

        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(out), 40000);
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(out).size() > 0, 40000);
        PdfiumBackend reader;
        QVERIFY(reader.loadDocument(out));
        const bool hasA = reader.extractText(0).contains(QStringLiteral("MERGEPARTONE"));
        QVERIFY2(hasA, "F2a: merged artifact must carry part one");
        // The merge worker can finish before the QTRY existence checks above
        // ever spin the event loop (fast machine, tiny fixture): the completion
        // modal lambda is then still queued when the honesty assertion runs and
        // the capture reads empty. Wait for the capture itself — the modal the
        // route owes the user is part of what F2a verifies.
        QTRY_VERIFY_WITH_TIMEOUT(!modalText.isEmpty(), 10000);
        step(QStringLiteral("F2a verified: artifact has %1 page(s) and carries the input "
                           "content; completion modal title='%2' text='%3'")
                 .arg(reader.pageCount()).arg(modalTitle, modalText.left(220)));
        // Honesty: the completion feedback must NAME the output or the merge.
        QVERIFY2(modalText.contains(QStringLiteral("combined"), Qt::CaseInsensitive)
                     || modalTitle.contains(QStringLiteral("combined"), Qt::CaseInsensitive)
                     || modalText.contains(QStringLiteral("merged"), Qt::CaseInsensitive)
                     || modalText.contains(QStringLiteral("Merge complete"), Qt::CaseInsensitive),
                 "F2a honesty: merge completion feedback must name the output file or the merge");
    }

    // ── F2b: Batch run + Preset Pipeline (create → run → re-run identical) ──
    void flow2b_batchAndPresetPipeline()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString src = dir.filePath("big.pdf");
        makePdf(src, "BATCHSOURCE");
        const QString outDir = dir.filePath("out");
        QVERIFY(QDir().mkpath(outDir));

        // Batch is a standalone task: welcome link → batch screen.
        m_win->startWelcomeTask(QStringLiteral("batch"));
        auto *bm = m_win->findChild<gp::BatchMode *>();
        QVERIFY2(bm, "F2b: the batch task must expose the BatchMode surface");
        step("F2b step1: batch screen reachable from the welcome (standalone link)");

        // Create a preset from a configured Compress run.
        bm->addFilesForTest({ src });
        bm->setOperationForTest(1 /* Compress / Optimize */);
        auto *presetOut = bm->findChild<QLineEdit *>(QStringLiteral("batchPresetOutDir"));
        QVERIFY(presetOut);
        presetOut->setText(outDir);
        const auto sliders = bm->findChildren<QSlider *>();
        if (!sliders.isEmpty())
            sliders.first()->setValue(50);

        // Save as preset — drive the real modal name dialog.
        bool presetDialogSeen = false;
        driveModalDialog(QStringLiteral("presetNameEdit"), [](QWidget *w) {
            if (auto *edit = w->findChild<QLineEdit *>(QStringLiteral("presetNameEdit")))
                edit->setText(QStringLiteral("uxflowpreset"));
            if (auto *bb = w->findChild<QDialogButtonBox *>())
                if (bb->button(QDialogButtonBox::Ok))
                    bb->button(QDialogButtonBox::Ok)->click();
        }, &presetDialogSeen);
        // Defensive: any UNEXPECTED modal (a preset-save refusal, a run
        // refusal) is recorded and dismissed instead of hanging the audit.
        static QString unexpectedText, unexpectedTitle;  // static: the poller may outlive the slot
        captureModalText(&unexpectedText, &unexpectedTitle,
                         nullptr, QStringLiteral("presetNameEdit"), 300000);
        // FINDING F2b-D1 (recorded, not fixed - outside this lane's surfaces):
        // on a CLEAN profile the first-ever "Save as Preset" FAILS with
        // ".../presets/<name>.glyphpreset.json: cannot open for writing -
        // The system cannot find the path specified", because
        // BatchPresetStore::save (src/core/BatchPreset.cpp:~857) never
        // mkpaths its root dir. The harness pre-creates the directory here so
        // the REST of the pipeline (run / re-run / multi-file merge) can be
        // audited; the first-run breaker itself is documented in the report.
        const QString presetDir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation) + QStringLiteral("/presets");
        if (QDir(presetDir).exists())
            for (const QFileInfo &stale
                 : QDir(presetDir).entryInfoList({ "*.glyphpreset.json" }, QDir::Files))
                QFile::remove(stale.absoluteFilePath());   // isolated test org - safe
        if (!QDir(presetDir).exists()) {
            QVERIFY(QDir().mkpath(presetDir));
            step("F2b: harness pre-created the missing preset store dir "
                 "(FINDING F2b-D1: the product never creates it - first-ever "
                 "Save-as-Preset fails with an opaque write error)");
        }

        auto *saveBtn = bm->findChild<QPushButton *>(QStringLiteral("batchPresetSaveBtn"));
        QVERIFY(saveBtn);
        saveBtn->click();
        QTRY_VERIFY_WITH_TIMEOUT(presetDialogSeen, 20000);
        QTest::qWait(300);
        step("F2b step2: preset save attempted through the real dialog");

        // The op combo must have switched to Preset Pipeline showing it.
        auto *picker = bm->findChild<QComboBox *>(QStringLiteral("batchPresetPicker"));
        QVERIFY2(picker, "F2b: preset picker must exist");
        if (picker->count() == 0)
            step(QStringLiteral("F2b REFUSAL RECORDED: preset picker empty; unexpected modal "
                               "title='%1' text='%2'")
                     .arg(unexpectedTitle, unexpectedText.left(200)));
        QCOMPARE(picker->count(), 1);
        auto *stepsLabel = bm->findChild<QLabel *>(QStringLiteral("batchPresetStepsLabel"));
        step(QStringLiteral("F2b step3: preset picker shows '%1' (%2 saved); steps label: '%3'")
                 .arg(picker->currentText()).arg(picker->count())
                 .arg(stepsLabel ? stepsLabel->text() : QStringLiteral("<none>")));

        // Run via the preset on a fresh copy of the source.
        const QString src2 = dir.filePath("big_run.pdf");
        QVERIFY(QFile::copy(src, src2));
        // The file list ACCUMULATES (Add Files appends - the Clear All button
        // is how a user resets it; QMetaObject drives the same slot).
        QMetaObject::invokeMethod(bm, "onClearFiles", Qt::DirectConnection);
        bm->addFilesForTest({ src2 });
        // batchFinished is emitted by onBatchFinished AFTER its G12 drain,
        // i.e. with the counters FINAL. Waiting on !isBatchRunning() raced
        // the queued resultReadyAt accounting (the worker can finish before
        // the slot reaches the wait; isRunning() flips false while the
        // accounting events are still pending) — the standalone failure at
        // the re-run assert read success=0 fail=0 while the engine log showed
        // the file WAS processed. The spy counts all three runs below.
        QSignalSpy finishedSpy(bm, &gp::BatchMode::batchFinished);
        bm->onRunBatch();
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 1, 60000);
        const QString log1 = bm->findChildren<QTextEdit *>().first()->toPlainText();
        step(QStringLiteral("F2b step4: preset run finished — success=%1 fail=%2 skip=%3; log: %4")
                 .arg(bm->successCount()).arg(bm->failCount()).arg(bm->skipCount())
                 .arg(log1.left(300)));
        QCOMPARE(bm->successCount(), 1);
        QCOMPARE(bm->failCount(), 0);

        // Re-run IDENTICAL: same preset, fresh copy of the same source.
        const QString src3 = dir.filePath("big_rerun.pdf");
        QVERIFY(QFile::copy(src, src3));
        QMetaObject::invokeMethod(bm, "onClearFiles", Qt::DirectConnection);
        bm->addFilesForTest({ src3 });
        bm->onRunBatch();
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 2, 60000);
        const QString log2 = bm->findChildren<QTextEdit *>().first()->toPlainText();
        step(QStringLiteral("F2b step5: identical re-run - success=%1 fail=%2; log: %3")
                 .arg(bm->successCount()).arg(bm->failCount()).arg(log2.left(200)));
        QCOMPARE(bm->successCount(), 1);
        step("F2b step5: identical re-run finished green");

        // Both runs must have produced artifacts in the chosen out dir.
        const QFileInfoList produced = QDir(outDir).entryInfoList({ "*.pdf" }, QDir::Files);
        QVERIFY2(produced.size() >= 1,
                 "F2b: preset runs must land outputs in the chosen out dir");
        step(QStringLiteral("F2b verified: %1 artifact(s) in the preset out dir; both runs green")
                 .arg(produced.size()));

        // ── Real MULTI-file merge through the batch surface (OpMerge = 4) ──
        const QString m1 = dir.filePath("mpart1.pdf");
        const QString m2 = dir.filePath("mpart2.pdf");
        makePdf(m1, "MERGEPARTONE");
        makePdf(m2, "MERGEPARTTWO");
        bm->setOperationForTest(4 /* Merge PDFs */);
        QMetaObject::invokeMethod(bm, "onClearFiles", Qt::DirectConnection);
        bm->addFilesForTest({ m1, m2 });
        const int successBefore = bm->successCount();   // counter is cumulative
        bm->onRunBatch();
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.size(), 3, 60000);
        QCOMPARE(bm->successCount(), successBefore + 1);
        QCOMPARE(bm->failCount(), 0);
        const QString mergedOut = dir.filePath("mpart1_merged.pdf");
        QVERIFY2(QFileInfo::exists(mergedOut),
                 "F2b: batch merge must write <first-input>_merged.pdf next to the input");
        PdfiumBackend mergedReader;
        QVERIFY(mergedReader.loadDocument(mergedOut));
        QVERIFY2(mergedReader.pageCount() >= 2,
                 "F2b: the merged artifact must carry both inputs");
        const bool both = mergedReader.extractText(0).contains(QStringLiteral("MERGEPARTONE"))
            && mergedReader.extractText(mergedReader.pageCount() - 1)
                   .contains(QStringLiteral("MERGEPARTTWO"));
        QVERIFY2(both, "F2b: both inputs' content must be in the merged artifact");
        // F2a-F1 pin (SWEEP-W3-UX): the merge completion feedback must NAME
        // the output. The batch surface has no completion modal — its summary
        // (status label + log) is the completion feedback, so it must carry
        // the merged file's name, not a generic "1 of 1 succeeded".
        QString mergeStatus;
        for (QLabel *l : bm->findChildren<QLabel *>())
            if (l->text().contains(QStringLiteral("BATCH COMPLETE")))
                mergeStatus = l->text();
        QVERIFY2(mergeStatus.contains(QStringLiteral("mpart1_merged.pdf"), Qt::CaseInsensitive)
                     || mergeStatus.contains(QStringLiteral("merged into"), Qt::CaseInsensitive),
                 QStringLiteral("F2a-F1 pin: the merge completion summary must name the "
                                "output file (status was '%1')").arg(mergeStatus)
                     .toUtf8().constData());
        // The merge run cleared and refilled the log — read it live.
        const QString mergeLog =
            bm->findChildren<QTextEdit *>().first()->toPlainText();
        QVERIFY2(mergeLog.contains(QStringLiteral("Merged output"), Qt::CaseInsensitive)
                     && mergeLog.contains(QStringLiteral("mpart1_merged.pdf"),
                                          Qt::CaseInsensitive),
                 "F2a-F1 pin: the batch log must name the merged output file");
        step(QStringLiteral("F2b verified: 2-file batch merge → %1 page(s), both parts present; "
                           "output at %2; completion named it: status='%3'")
                 .arg(mergedReader.pageCount()).arg(mergedOut, mergeStatus.left(120)));
    }

    // ── F3: redaction mark → apply → proof report → export + partial failure ─
    void flow3_redaction_fullLoop_andPartialFailure()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString src = dir.filePath("secret-doc.pdf");
        makePdfWithSecret(src, QStringLiteral("TOPSECRETDATA"));
        step("F3 start: fixture with TOPSECRETDATA at a known position");

        // Open + enter the redact screen through production dispatch.
        runCardRoute("open", src);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        m_win->activateScreen(QStringLiteral("redact"));
        QTest::qWait(300);
        step("F3 step1: redact screen activated");

        auto *redact = m_win->findChild<gp::RedactMode *>();
        QVERIFY2(redact, "F3: RedactMode must be reachable");
        redact->activateCustomRegex(QStringLiteral("TOPSECRETDATA"));

        // Mark All Occurrences — the real marking path (the panel pill's slot).
        QMetaObject::invokeMethod(redact, "onMarkAllOccurrences", Qt::DirectConnection);
        QTest::qWait(400);
        auto *viewer = m_win->pdfViewer();
        int marks = 0;
        const auto annos = viewer->annotations();
        for (const auto &an : annos)
            if (an.mode == ToolMode::Redact) ++marks;
        QVERIFY2(marks > 0, "F3: Mark All Occurrences must place redaction marks");
        step(QStringLiteral("F3 step2: %1 redaction mark(s) placed").arg(marks));

        // Apply All Redactions → the apply dialog (modal). Drive it: set
        // destinations, enable sanitize + proof, accept. The completion modal
        // text is captured (skipping the apply dialog itself).
        const QString redactedOut = dir.filePath("secret-doc_redacted.pdf");
        const QString sanitizedOut = dir.filePath("secret-doc_sanitized.pdf");
        static QString completionText, completionTitle;  // static: poller may outlive the slot
        driveModalDialog(QStringLiteral("redactApplyDialog"),
                         [redactedOut, sanitizedOut](QWidget *w) {
                             if (auto *d = w->findChild<QLineEdit *>(
                                     QStringLiteral("redactApplyDestinationEdit")))
                                 d->setText(redactedOut);
                             if (auto *s = w->findChild<QLineEdit *>(
                                     QStringLiteral("redactApplySanitizedDestinationEdit")))
                                 s->setText(sanitizedOut);
                             if (auto *chk = w->findChild<QCheckBox *>(
                                     QStringLiteral("redactApplySanitizeCheck")))
                                 chk->setChecked(true);
                             if (auto *proof = w->findChild<QCheckBox *>(
                                     QStringLiteral("redactApplyProofCheck")))
                                 proof->setChecked(true);
                             step("F3 step3: apply dialog configured (sanitize + proof) "
                                  "and accepted");
                             if (auto *ok = w->findChild<QPushButton *>(
                                     QStringLiteral("redactApplyOkButton")))
                                 ok->click();
                         });
        captureModalText(&completionText, &completionTitle,
                         nullptr, QStringLiteral("redactApplyDialog"), 90000);
        QMetaObject::invokeMethod(redact, "onApplyRedactions", Qt::DirectConnection);

        // Landed states.
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(redactedOut), 60000);
        QTest::qWait(2000);   // let the queued completion surface + driver settle
        QVERIFY2(!pdfTextContains(redactedOut, "TOPSECRETDATA"),
                 "F3: the secret must be excised from the redacted output");
        step("F3 step4 verified: redacted output exists; the secret is GONE from all streams");
        const QString statusText = m_win->findChild<gp::StatusBar *>()
            ? m_win->findChild<gp::StatusBar *>()->currentMessage() : QString();
        step(QStringLiteral("F3 completion surface: modal title='%1' text='%2'; statusbar='%3'")
                 .arg(completionTitle, completionText.left(400), statusText.left(150)));
        // Honesty (soft-recorded): SOME surface must tell the user the outcome.
        const bool modalTold = completionTitle.contains(QStringLiteral("Redaction"),
                                                         Qt::CaseInsensitive)
            || completionText.contains(QStringLiteral("redact"), Qt::CaseInsensitive);
        const bool statusTold = statusText.contains(QStringLiteral("edact"), Qt::CaseInsensitive)
            || statusText.contains(QStringLiteral("saved"), Qt::CaseInsensitive);
        if (!modalTold && !statusTold)
            step("F3 FINDING CANDIDATE (F3-D?): no completion modal was captured and the "
                 "status bar does not state the outcome - verify against the transcript");
        QVERIFY2(modalTold || statusTold,
                 "F3 honesty: the user must be told the redaction outcome "
                 "(modal or status bar)");
        QVERIFY2(completionText.isEmpty()
                     || completionText.contains(QStringLiteral("proof"), Qt::CaseInsensitive)
                     || completionText.contains(QStringLiteral("PASSED"), Qt::CaseInsensitive)
                     || completionText.contains(QStringLiteral("sanitiz"), Qt::CaseInsensitive),
                 "F3 honesty: proof/sanitization verdict must surface to the user");
        if (QFileInfo::exists(sanitizedOut))
            step("F3 step5: sanitized copy landed as requested");
        else
            step("F3 step5: sanitized copy MISSING — if the modal claimed full success "
                 "that is a DISHONESTY finding (transcript above decides)");

        // ── Partial-failure honesty: a sanitization target that cannot be
        // written (unusable path) must produce a LABELED partial dialog, never
        // a generic success banner.
        const QString src2 = dir.filePath("secret2.pdf");
        makePdfWithSecret(src2, QStringLiteral("TOPSECRETDATA"));
        m_win->openDocument(src2);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        m_win->activateScreen(QStringLiteral("redact"));
        QTest::qWait(300);
        auto *redact2 = m_win->findChild<gp::RedactMode *>();
        QVERIFY(redact2);
        redact2->activateCustomRegex(QStringLiteral("TOPSECRETDATA"));
        QMetaObject::invokeMethod(redact2, "onMarkAllOccurrences", Qt::DirectConnection);
        QTest::qWait(400);
        marks = 0;
        for (const auto &an : m_win->pdfViewer()->annotations())
            if (an.mode == ToolMode::Redact) ++marks;
        QVERIFY2(marks > 0, "F3: doc-2 marking must place redaction marks");

        const QString redactedOut2 = dir.filePath("secret2_redacted.pdf");
        const QString badSanitized = dir.filePath("no-such-subdir") + "/x.pdf";
        static QString partialText, partialTitle;  // static: poller may outlive the slot
        partialText.clear(); partialTitle.clear();
        driveModalDialog(QStringLiteral("redactApplyDialog"),
                         [redactedOut2, badSanitized](QWidget *w) {
                             if (auto *d = w->findChild<QLineEdit *>(
                                     QStringLiteral("redactApplyDestinationEdit")))
                                 d->setText(redactedOut2);
                             if (auto *s = w->findChild<QLineEdit *>(
                                     QStringLiteral("redactApplySanitizedDestinationEdit")))
                                 s->setText(badSanitized);
                             if (auto *chk = w->findChild<QCheckBox *>(
                                     QStringLiteral("redactApplySanitizeCheck")))
                                 chk->setChecked(true);
                             step("F3 step6: apply configured with an IMPOSSIBLE "
                                  "sanitize target");
                             if (auto *ok = w->findChild<QPushButton *>(
                                     QStringLiteral("redactApplyOkButton")))
                                 ok->click();
                         });
        // The partial dialog carries CUSTOM buttons (Retry Sanitize / Keep
        // Redacted File / Discard Output) - close() leaves exec() in an
        // unresolved state, so drive the NATURAL user choice instead.
        capturePromptAndClick(QStringLiteral("Partially Complete"),
                              QStringLiteral("Keep Redacted File"),
                              &partialText, &partialTitle);
        QMetaObject::invokeMethod(redact2, "onApplyRedactions", Qt::DirectConnection);

        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(redactedOut2), 60000);
        QTest::qWait(2000);
        step(QStringLiteral("F3 partial-failure modal: title='%1' text='%2'")
                 .arg(partialTitle, partialText.left(400)));
        if (!QFileInfo::exists(badSanitized)) {
            // The sanitize cannot have landed; the surface must not claim a
            // full success. Either a labeled partial dialog appeared, or an
            // explicit sanitized-failure verdict is in the text.
            const bool claimsSanitized = partialText.contains(
                QStringLiteral("sanitized copy has been saved"), Qt::CaseInsensitive)
                || partialText.contains(QStringLiteral("sanitized copy:"),
                                        Qt::CaseInsensitive);
            QVERIFY2(!claimsSanitized,
                     "F3 DISHONESTY: the sanitized copy was NOT written but the "
                     "surface claimed it was saved");
            step("F3 step6 verified: the failure is not reported as success "
                 "(verdict in the transcript above)");
        } else {
            step("F3 step6: the impossible target unexpectedly succeeded — harness note");
        }
    }

    // ── F3b: the G2 pattern-text refusal, through the REAL UI route ─────────
    // A page whose resource tree carries a tiling pattern whose stream holds
    // glyph-carrying text cannot be excised (the canvas walk reaches only
    // Do-referenced Form XObjects and images). The merged redaction-gaps lane
    // (G2, audit REDACTION-RESEARCH-2026-09-21 §2.4) made the engine REFUSE
    // the whole run with a named reason instead of painting a black box over
    // live data. This slot pins the USER-VISIBLE half of that contract: the
    // refusal must surface as a labeled "Redaction Failed" disclosure that
    // names the pattern reason, with NO output written and the source file
    // untouched. The refusal is page-level — the mark covers only the public
    // text, and the engine still refuses because the page's resource tree
    // carries the text-bearing pattern.
    void flow3b_redaction_patternText_refusalDisclosure()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString src = dir.filePath("pattern-secret.pdf");
        QVERIFY2(makePatternTextPdf(src), "F3b: pattern fixture creation failed");
        QVERIFY2(patternStreamContains(src, "PatternSecretOmega"),
                 "F3b: the fixture must carry the secret in its pattern stream");
        step("F3b start: pattern-text fixture (public text + pattern-stream secret)");

        runCardRoute("open", src);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        m_win->activateScreen(QStringLiteral("redact"));
        QTest::qWait(300);
        step("F3b step1: redact screen activated");

        auto *redact = m_win->findChild<gp::RedactMode *>();
        QVERIFY2(redact, "F3b: RedactMode must be reachable");
        redact->activateCustomRegex(QStringLiteral("PUBLIC_KEEP_TEXT"));
        QMetaObject::invokeMethod(redact, "onMarkAllOccurrences", Qt::DirectConnection);
        QTest::qWait(400);
        int marks = 0;
        for (const auto &an : m_win->pdfViewer()->annotations())
            if (an.mode == ToolMode::Redact) ++marks;
        QVERIFY2(marks > 0,
                 "F3b: Mark All Occurrences must place marks on the public text");
        step(QStringLiteral("F3b step2: %1 redaction mark(s) placed over the public text")
                 .arg(marks));

        const QString redactedOut = dir.filePath("pattern-secret_redacted.pdf");
        static QString refusalText, refusalTitle;  // static: the poller may outlive the slot
        refusalText.clear(); refusalTitle.clear();
        driveModalDialog(QStringLiteral("redactApplyDialog"),
                         [redactedOut](QWidget *w) {
                             if (auto *d = w->findChild<QLineEdit *>(
                                     QStringLiteral("redactApplyDestinationEdit")))
                                 d->setText(redactedOut);
                             step("F3b step3: apply dialog configured and accepted");
                             if (auto *ok = w->findChild<QPushButton *>(
                                     QStringLiteral("redactApplyOkButton")))
                                 ok->click();
                         });
        capturePromptAndClick(QStringLiteral("Redaction Failed"), QStringLiteral("OK"),
                              &refusalText, &refusalTitle);
        QMetaObject::invokeMethod(redact, "onApplyRedactions", Qt::DirectConnection);

        QTRY_VERIFY_WITH_TIMEOUT(!refusalTitle.isEmpty(), 60000);
        step(QStringLiteral("F3b refusal disclosure: title='%1' text='%2'")
                 .arg(refusalTitle, refusalText.left(400)));
        // The disclosure must be LABELED and NAMED — a generic failure banner
        // would hide the one thing the user must understand: the pattern text
        // survives, and that is why the page was refused.
        QCOMPARE(refusalTitle, QStringLiteral("Redaction Failed"));
        QVERIFY2(refusalText.contains(QStringLiteral("pattern"), Qt::CaseInsensitive),
                 "F3b: the refusal must name the pattern reason");
        QVERIFY2(refusalText.contains(QStringLiteral("refused"), Qt::CaseInsensitive)
                     || refusalText.contains(QStringLiteral("black box"),
                                             Qt::CaseInsensitive),
                 "F3b: the refusal must say the page was refused (no silent black box)");
        QVERIFY2(refusalText.contains(QStringLiteral("not modified"), Qt::CaseInsensitive),
                 "F3b: the refusal must state the original was not modified");

        // Landed states: an honest refusal writes NO output and leaves the
        // source byte-truthful (both the secret AND the public text intact).
        QTest::qWait(500);
        QVERIFY2(!QFileInfo::exists(redactedOut),
                 "F3b: an honest refusal must write NO redacted output");
        QVERIFY2(patternStreamContains(src, "PatternSecretOmega"),
                 "F3b: the source must be untouched — the pattern secret survives");
        QVERIFY2(pdfTextContains(src, "PUBLIC_KEEP_TEXT"),
                 "F3b: the source must be untouched — the public text intact");
        step("F3b verified: labeled refusal naming the pattern reason, no output "
             "written, source untouched");
    }

    // ── F8: find & replace regex + replace-all count honesty (WP-R07) ───────
    void flow8_findReplace_regex_countHonesty()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString src = dir.filePath("three-pages.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            static const struct { int page; const char *word; double x; double y; } words[] = {
                { 0, "Alpha", 50, 700 }, { 0, "beta", 150, 700 }, { 0, "alphabet", 250, 700 },
                { 1, "ALPHA", 50, 700 }, { 1, "appears", 110, 700 }, { 1, "here", 180, 700 },
                { 2, "delta", 50, 700 }, { 2, "alpha", 100, 700 }, { 2, "end", 150, 700 },
            };
            for (int i = 0; i < 3; ++i)
                doc.GetPages().CreatePage(
                    PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            for (const auto &w : words) {
                auto &page = doc.GetPages().GetPageAt(w.page);
                PoDoFo::PdfPainter painter;
                painter.SetCanvas(page);
                auto &font = doc.GetFonts().GetStandard14Font(
                    PoDoFo::PdfStandard14FontType::Helvetica);
                painter.TextState.SetFont(font, 12.0);
                painter.DrawText(w.word, w.x, w.y);
                painter.FinishDrawing();
            }
            doc.Save(src.toUtf8().constData());
        }
        QVERIFY(QFileInfo::exists(src));
        step("F8 start: 3-page fixture; regex [Aa]lpha[a-z]* → Alpha + alphabet + alpha = 3");

        runCardRoute("open", src);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 3, 20000);

        // Production entry (the host method EditController dispatches to).
        m_win->showFindReplaceDialog();
        auto *fr = m_win->findChild<FindReplaceDialog *>(
            QStringLiteral("findReplaceDialog"));
        QVERIFY2(fr, "F8: the find & replace dialog must open");
        step("F8 step1: dialog open via production entry");

        auto *search = fr->findChild<QLineEdit *>(QStringLiteral("frSearch"));
        auto *replace = fr->findChild<QLineEdit *>(QStringLiteral("frReplace"));
        auto *summary = fr->findChild<QLabel *>(QStringLiteral("frMatchSummary"));
        auto *countBtn = fr->findChild<QPushButton *>(QStringLiteral("frCount"));
        auto *replaceAllBtn = fr->findChild<QPushButton *>(QStringLiteral("frReplaceAll"));
        auto *regexChk = fr->findChild<QCheckBox *>(QStringLiteral("frRegex"));
        auto *details = fr->findChild<QPlainTextEdit *>(QStringLiteral("frDetails"));
        QVERIFY(search && replace && summary && countBtn && replaceAllBtn && regexChk);

        regexChk->setChecked(true);
        search->setText(QStringLiteral("[Aa]lpha[a-z]*"));
        replace->setText(QStringLiteral("Zeta"));
        QTest::qWait(800);   // recount debounce
        countBtn->click();
        QTest::qWait(500);
        const QString countText = summary->text();
        step(QStringLiteral("F8 step2: COUNT reported: '%1'").arg(countText));
        QVERIFY2(countText.contains(QStringLiteral("match"), Qt::CaseInsensitive),
                 "F8: the count surface must state the match count");
        QVERIFY2(countText.contains(QLatin1Char('3')), "F8: the regex count must be 3");

        replaceAllBtn->click();
        QTest::qWait(800);
        const QString outcome = details ? details->toPlainText() : QString();
        step(QStringLiteral("F8 step3: REPLACE ALL reported: '%1'").arg(outcome.left(300)));
        QVERIFY2(!outcome.isEmpty(), "F8: replace-all must report an outcome");
        QVERIFY2(outcome.contains(QStringLiteral("replac"), Qt::CaseInsensitive),
                 "F8: the outcome must state how many replacements were applied");

        // Independent honesty check: the saved artifact must agree with the
        // report — all 3 Alpha-family words replaced → no "lpha" glyph run
        // readable in the production text extractor.
        m_win->onToolActivated(QStringLiteral("save"));
        QTest::qWait(1200);
        const QString saved = m_win->pdfViewer()->filePath();
        QCOMPARE(saved, src);
        PdfiumBackend reader;
        QVERIFY(reader.loadDocument(saved));
        int remaining = 0;
        for (int p = 0; p < reader.pageCount(); ++p) {
            const QString text = reader.extractText(p);
            remaining += text.count(QStringLiteral("lpha"));
        }
        step(QStringLiteral("F8 step4: surviving 'lpha' glyph runs after save: %1 "
                           "(outcome disclosed: '%2')")
                 .arg(remaining).arg(outcome.left(120)));
        QVERIFY2(remaining == 0,
                 "F8 honesty: 'replace all' reported done must not leave matched "
                 "text readable in the artifact");
        QVERIFY2(outcome.contains(QStringLiteral("Geometry"), Qt::CaseInsensitive)
                     || outcome.contains(QStringLiteral("replac"), Qt::CaseInsensitive),
                 "F8: the report must carry the replacement accounting");
        step("F8 verified: count-before-replace + replace accounting are honest (WP-R07)");
    }

    // ── F4: sign P12 → certify L2 → verify; prepare-signing-request honesty ──
    void flow4_signCertifyVerify_andPrepRequest()
    {
        // Fixtures: p12 + recipient certs ship with the suite.
        #ifdef SOURCE_DIR
        const QString fixtures = QStringLiteral(SOURCE_DIR "/tests/fixtures/signing");
#else
        const QString fixtures = QStringLiteral("tests/fixtures/signing");
#endif
        const QString p12 = fixtures + QStringLiteral("/test_signer.p12");
        if (!QFileInfo::exists(p12)) {
            QSKIP("signing fixtures not present in this environment");
        }
        const QString p12Pass = QStringLiteral("test");
        const QString kReason = QStringLiteral("ux-audit sign");

        // ── 4a. P12 sign through the production Sign dispatch ──────────────
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString doc1 = dir.filePath("to-sign.pdf");
        makePdf(doc1, "F4SIGNME");
        runCardRoute("open", doc1);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);

        const QString signedOut = dir.filePath("to-sign_signed.pdf");
        // Dialog first (modal), then the save dialog it chains into.
        driveModalDialog(QStringLiteral("signatureCertPathEdit"), [&](QWidget *w) {
            if (auto *cert = w->findChild<QLineEdit *>(QStringLiteral("signatureCertPathEdit")))
                cert->setText(p12);
            if (auto *pwd = w->findChild<QLineEdit *>(QStringLiteral("signaturePasswordEdit")))
                pwd->setText(p12Pass);
            step(QStringLiteral("F4a sign dialog: title='%1'; credentials filled")
                     .arg(w->windowTitle()));
            if (auto *bb = w->findChild<QDialogButtonBox *>())
                if (auto *ok = bb->button(QDialogButtonBox::Ok))
                    ok->click();
        });
        pickFilesInSequence({ signedOut });
        // The sign flow ends with an honest completion question ("Signing
        // complete. Would you like to open the signed file?") — answer No; the
        // audit opens the artifact itself. Budget spans the whole sign window.
        bool f4aOpenPromptSeen = false;
        clickPromptButton(QStringLiteral("No"), &f4aOpenPromptSeen);
        step("F4a: dispatching production Sign");
        m_win->onToolActivated(QStringLiteral("sign"));
        // Landed state via the independent engine read path.
        auto signingConcrete = std::dynamic_pointer_cast<SignatureManager>(
            m_win->appContext()->signing);
        if (!signingConcrete)
            QSKIP("signing engine is not the concrete SignatureManager in this build");
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(signedOut), 60000);
        QTRY_VERIFY_WITH_TIMEOUT(
            !signingConcrete->validateSignatures(signedOut).isEmpty(), 60000);
        step(QStringLiteral("F4a verified: %1 signature(s) on the saved artifact; "
                           "completion prompt seen=%2 (dismissed with No)")
                 .arg(signingConcrete->validateSignatures(signedOut).size())
                 .arg(f4aOpenPromptSeen));

        // Verify surface (what the user is told): Validate All Signatures.
        static QString verifyText, verifyTitle;  // static: poller may outlive the slot
        captureModalText(&verifyText, &verifyTitle);
        m_win->openDocument(signedOut);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        m_win->onToolActivated(QStringLiteral("validateSig"));
        QTest::qWait(2500);
        step(QStringLiteral("F4a verify surface: title='%1' text='%2'")
                 .arg(verifyTitle, verifyText.left(300)));
        if (verifyText.isEmpty() && verifyTitle.isEmpty())
            step("F4a verify surface: NO modal appeared within budget - the outcome "
                 "may surface outside a modal (recorded for the report)");
        else
            QVERIFY2(verifyText.contains(QStringLiteral("signature"), Qt::CaseInsensitive)
                         || verifyTitle.contains(QStringLiteral("signature"), Qt::CaseInsensitive),
                     "F4a honesty: the verify surface must report the signature state");

        // ── 4b. Certify honesty on an ALREADY-SIGNED doc ────────────────────
        // Certify must be refused with the disclosed reason (certify must be
        // the FIRST signature): the purpose combo is visible but disabled.
        driveModalDialog(QStringLiteral("Certify Document"), [](QWidget *w) {
            if (auto *purpose = w->findChild<QComboBox *>(
                    QStringLiteral("signaturePurposeCombo"))) {
                step(QStringLiteral("F4b certify-on-signed: purpose combo enabled=%1 "
                                   "items=%2")
                         .arg(purpose->isEnabled()).arg(purpose->count()));
            }
            if (auto *unavail = w->findChild<QLabel *>(
                    QStringLiteral("certifyUnavailableLabel")))
                step(QStringLiteral("F4b disclosed reason: '%1'")
                         .arg(unavail->text().left(200)));
            // Close without accepting — this leg audits the state, not a run.
            if (auto *bb = w->findChild<QDialogButtonBox *>())
                if (auto *cancel = bb->button(QDialogButtonBox::Cancel))
                    cancel->click();
        }, nullptr, 20000);
        m_win->onToolActivated(QStringLiteral("certify"));
        QTest::qWait(2000);
        step("F4b done: certify-on-signed state recorded (see transcript)");

        // ── 4c. Certify level 2 on a FRESH doc → /DocMDP /P == 2 ────────────
        const QString doc2 = dir.filePath("to-certify.pdf");
        makePdf(doc2, "F4CERTIFYME");
        m_win->openDocument(doc2);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        const QString certifiedOut = dir.filePath("to-certified.pdf");
        bool certifyDialogSeen = false;
        driveModalDialog(QStringLiteral("Certify Document"), [&](QWidget *w) {
            if (auto *purpose = w->findChild<QComboBox *>(
                    QStringLiteral("signaturePurposeCombo"))) {
                const int i = purpose->findText(QStringLiteral("Certify"));
                QVERIFY2(i >= 0, "F4c: Certify purpose must be offered on an unsigned doc");
                purpose->setCurrentIndex(i);
            }
            if (auto *level = w->findChild<QComboBox *>(
                    QStringLiteral("signatureLevelCombo")))
                level->setCurrentIndex(1);   // level 2
            if (auto *cert = w->findChild<QLineEdit *>(
                    QStringLiteral("signatureCertPathEdit")))
                cert->setText(p12);
            if (auto *pwd = w->findChild<QLineEdit *>(
                    QStringLiteral("signaturePasswordEdit")))
                pwd->setText(p12Pass);
            step("F4c: Certify purpose + level 2 + credentials set; accepting");
            if (auto *bb = w->findChild<QDialogButtonBox *>())
                if (auto *ok = bb->button(QDialogButtonBox::Ok))
                    ok->click();
        }, &certifyDialogSeen, 30000);
        pickFilesInSequence({ certifiedOut });
        bool f4cOpenPromptSeen = false;
        clickPromptButton(QStringLiteral("No"), &f4cOpenPromptSeen);
        m_win->onToolActivated(QStringLiteral("certify"));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(certifiedOut), 60000);
        // Independent read: /DocMDP /P must be 2. The /P lives in the
        // certification signature's /Reference (Sig /Reference[0]
        // /TransformMethod /DocMDP → /TransformParams /P) with catalog /Perms
        // /DocMDP pointing at the signature — the TestCertifySelector idiom.
        const int docMdpP = [] (const QString &path) {
            try {
                PoDoFo::PdfMemDocument doc;
                doc.Load(path.toUtf8().constData());
                const PoDoFo::PdfObject *perms =
                    doc.GetCatalog().GetDictionary().FindKey("Perms");
                const PoDoFo::PdfObject *sigObj = perms
                    ? perms->GetDictionary().FindKey("DocMDP") : nullptr;
                const PoDoFo::PdfObject *mdpDict =
                    doc.GetCatalog().GetDictionary().FindKey("MDP");
                if (mdpDict && mdpDict->GetDictionary().FindKey("P"))
                    return static_cast<int>(
                        mdpDict->GetDictionary().FindKey("P")->GetNumber());
                if (!sigObj) return -1;
                const PoDoFo::PdfObject *reference =
                    sigObj->GetDictionary().FindKey("Reference");
                if (!reference || !reference->IsArray()
                    || reference->GetArray().size() == 0)
                    return -1;
                const PoDoFo::PdfObject *transformParams =
                    reference->GetArray()[0].GetDictionary().FindKey("TransformParams");
                if (!transformParams) return -1;
                const PoDoFo::PdfObject *p =
                    transformParams->GetDictionary().FindKey("P");
                if (!p) return -1;
                return static_cast<int>(p->GetNumber());
            } catch (const std::exception &) {
                return -1;
            }
        }(certifiedOut);
        QCOMPARE(docMdpP, 2);
        step(QStringLiteral("F4c verified: certified artifact carries /DocMDP /P=%1; "
                           "certify dialog seen=%2; completion prompt seen=%3")
                 .arg(docMdpP).arg(certifyDialogSeen).arg(f4cOpenPromptSeen));

        // ── 4d. prepare-signing-request with 2 signers → fill step 1 → step 2 ─
        const QString doc3 = dir.filePath("request.pdf");
        makePdf(doc3, "F4REQUEST");
        m_win->openDocument(doc3);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);

        bool prepDialogSeen = false;
        driveModalDialog(QStringLiteral("Prepare Signing Request"), [&](QWidget *w) {
            auto *addBtn = buttonByText(w, QStringLiteral("Add Signer"));
            if (!addBtn) return;
            addBtn->click();
            addBtn->click();
            step("F4d: two signers added through the real Add Signer control");
            if (auto *bb = w->findChild<QDialogButtonBox *>())
                if (auto *ok = bb->button(QDialogButtonBox::Ok)) {
                    ok->click();   // "Save Request"
                    step("F4d: Save Request accepted");
                }
        }, &prepDialogSeen, 30000);
        m_win->onToolActivated(QStringLiteral("prepareSigningReq"));
        QTRY_VERIFY_WITH_TIMEOUT(prepDialogSeen, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(
            QFileInfo::exists(SigningRequestModel::sidecarPathFor(doc3)), 10000);
        step("F4d verified: sidecar written next to the document");

        // The modeless progress panel appears; fill step 1 then step 2 through
        // the real Sign-as-this-signer + SignatureDialog chain.
        auto *panel = m_win->findChild<SigningProgressPanel *>();
        QVERIFY2(panel, "F4d: the signing progress panel must appear");
        QTRY_VERIFY_WITH_TIMEOUT(!panel->windowTitle().isEmpty(), 5000);
        // The fill-step path discloses status/failure through OK-only dialogs.
        // RECORD what the product tells the user + dismiss by CLICKING (a
        // close() on a one-button QMessageBox leaves its exec() unresolved and
        // blocks the audit). 'No' answers the changed-document re-confirm
        // question if the gate ever fires. Each step's dialogs get their own
        // driver (a clicker disarms after its single click).
        int f4dStepsSigned = 0;
        for (int signer = 0; signer < 2; ++signer) {
            bool f4dOkSeen = false, f4dNoSeen = false;
            clickPromptButton(QStringLiteral("OK"), &f4dOkSeen, 60000);
            clickPromptButton(QStringLiteral("No"), &f4dNoSeen, 60000);
            bool sigDialogSeen = false;
            driveModalDialog(QStringLiteral("signatureCertPathEdit"), [&](QWidget *w) {
                if (auto *cert = w->findChild<QLineEdit *>(
                        QStringLiteral("signatureCertPathEdit")))
                    cert->setText(p12);
                if (auto *pwd = w->findChild<QLineEdit *>(
                        QStringLiteral("signaturePasswordEdit")))
                    pwd->setText(p12Pass);
                step(QStringLiteral("F4d step %1: signature dialog filled")
                         .arg(signer + 1));
                if (auto *bb = w->findChild<QDialogButtonBox *>())
                    if (auto *ok = bb->button(QDialogButtonBox::Ok))
                        ok->click();
            }, &sigDialogSeen, 60000);
            auto *signBtn = buttonByText(panel, QStringLiteral("Sign as this signer"));
            if (!signBtn || !signBtn->isEnabled()) {
                step(QStringLiteral("F4d step %1: the panel offers no enabled "
                                    "'Sign as this signer' — workflow state recorded")
                         .arg(signer + 1));
                break;
            }
            QTest::qWait(200);
            signBtn->click();
            QTRY_VERIFY_WITH_TIMEOUT(sigDialogSeen, 60000);
            // The step completes asynchronously. This audit RECORDS the landed
            // panel state on a bounded, narrated budget instead of asserting
            // completability — the fill-step commit defect is recorded as
            // F4d-D1 and gated by QEXPECT_FAIL below.
            QListWidget *stateList = nullptr;
            const auto lists = panel->findChildren<QListWidget *>();
            if (!lists.isEmpty()) stateList = lists.first();
            bool stepSigned = false;
            if (stateList) {
                QElapsedTimer stClock;
                stClock.start();
                while (stClock.elapsed() < 45000) {
                    if (stateList->count() >= 2 && stateList->item(signer)
                        && stateList->item(signer)->text().contains(
                               QStringLiteral("SIGNED"))) {
                        stepSigned = true;
                        break;
                    }
                    QTest::qWait(3000);
                }
                step(QStringLiteral("F4d step %1 panel state after %2s: '%3'")
                         .arg(signer + 1).arg(stClock.elapsed() / 1000)
                         .arg(stateList->count() > signer && stateList->item(signer)
                                  ? stateList->item(signer)->text().left(120)
                                  : QStringLiteral("<no entry>")));
            } else {
                QTest::qWait(4000);
                step("F4d: panel list not found (state read via engine below)");
            }
            if (stepSigned)
                ++f4dStepsSigned;
            else
                step(QStringLiteral("F4d step %1: signer did not reach SIGNED "
                                    "(the step's own disclosure is recorded above — "
                                    "see finding F4d-D1)").arg(signer + 1));
        }
        // Completion honesty: the panel's status text must claim completion
        // only because both engine-attested signatures exist.
        const int engineSigs = signingConcrete->validateSignatures(doc3).size();
        QString titleText;
        const auto labels = panel->findChildren<QLabel *>();
        for (QLabel *l : labels) {
            const QString t = l->text();
            if (t.contains(QStringLiteral("Signer"), Qt::CaseInsensitive)
                || t.contains(QStringLiteral("complete"), Qt::CaseInsensitive)) {
                titleText = t;
                break;
            }
        }
        step(QStringLiteral("F4d completion surface: '%1'; engine signatures on doc: %2; "
                           "steps signed: %3")
                 .arg(titleText.left(250)).arg(engineSigs).arg(f4dStepsSigned));
        // F4d-D1 was FIXED in the ux-defects fix lane: the background fill-step
        // commits are background-safe again. Two pins had to go for the in-place
        // replacement to succeed — the viewer's QPdfDocument (the shell's SafeSave
        // handle coordinator now marshals park/restore to the GUI thread for
        // worker commits) and the editing engine's file-backed resident
        // (SendForSigningController releases it per step, the runA11yFix/
        // FormsController precedent). This slot is gated as a PERMANENT PASS:
        // the driven signer steps must produce engine-attested signatures.
        QVERIFY2(engineSigs >= 2,
                 "F4d completability: the driven signer steps must produce "
                 "engine-attested signatures");
        if (engineSigs == 2)
            QVERIFY2(titleText.contains(QStringLiteral("complete"), Qt::CaseInsensitive)
                         || titleText.contains(QStringLiteral("Signer"), Qt::CaseInsensitive),
                     "F4d honesty: the panel must state the workflow state");
    }

    // ── F5: OCR — scan page, image page, reject/re-Ocr ───────────────────────
    void flow5_ocr_imagePage_reject_reOcr_andTextPage()
    {
        // Models must sit next to the test binary (junction created by the
        // run script) for the capability probe to enable the route.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString imgPdf = dir.filePath("scan.pdf");
        {
            // Scanner-like scale: A4 @ 96dpi, ~21pt text drawn 1:1. (A raster
            // stretched across the full page defeats the detector's scale
            // priors — the first harness pass saw an honest "0 text blocks
            // detected" on exactly that.)
            QImage img(794, 1123, QImage::Format_RGB32);
            img.fill(Qt::white);
            QPainter p(&img);
            p.setPen(Qt::black);
            QFont f = QStringLiteral("Arial");
            f.setPixelSize(28);
            f.setBold(true);
            p.setFont(f);
            p.drawText(60, 200, QStringLiteral("OCRME 42"));
            p.end();
            QPdfWriter w(imgPdf);
            w.setResolution(96);
            w.setPageSize(QPageSize(QPageSize::A4));
            QPainter pw(&w);
            pw.drawImage(QRect(0, 0, w.width(), w.height()), img);
            pw.end();
        }
        QVERIFY(QFileInfo::exists(imgPdf));
        // F5-F2 pin (the audit's 3-blocks-clean-scan probe): run with the
        // SHIPPED DEFAULTS — clear the persisted preprocessing prefs so this
        // run sees exactly what a FIRST-TIME user sees. The defaults must be
        // honest: recognition on a clean scan must work out of the box (the
        // shipped deskew+binarize+denoise chain used to ZERO it).
        QSettings().remove(QStringLiteral("ocr/preprocessDeskew"));
        QSettings().remove(QStringLiteral("ocr/preprocessBinarize"));
        QSettings().remove(QStringLiteral("ocr/preprocessDenoise"));
        QSettings().remove(QStringLiteral("ocr/orientDetect"));
        step("F5 start: image-only fixture OCRME 42 (FIRST-RUN default prefs — F5-F2 pin)");

        auto *caps = m_win->appContext()->capabilities.get();
        const bool ocrPossible = caps
            && (caps->available(gp::CapId::OcrTesseract)
                || caps->available(gp::CapId::OcrRapidModels));
        if (!ocrPossible) {
            step("F5: no OCR engine probed available — the honest disabled card "
                 "state is the finding-free expectation (skipping live OCR)");
            QSKIP("no OCR engine available next to the test binary");
        }
        runCardRoute("ocr", imgPdf);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        auto *ocr = m_win->findChild<gp::OCRMode *>();
        QVERIFY2(ocr, "F5: the OCR verify screen must be reachable from the route");
        step("F5 step1: OCR verify screen open via the welcome route");

        auto *runBtn = ocr->findChild<QAbstractButton *>(QStringLiteral("ocrBtnRun"));
        auto *acceptBtn = ocr->findChild<QAbstractButton *>(QStringLiteral("ocrBtnAccept"));
        auto *rejectBtn = ocr->findChild<QAbstractButton *>(QStringLiteral("ocrBtnReject"));
        auto *textEdit = ocr->findChild<QPlainTextEdit *>(QStringLiteral("ocrTextEdit"));
        QVERIFY(runBtn && acceptBtn && rejectBtn && textEdit);
        QAbstractButton *runPushButton = runBtn;
        QAbstractButton *acceptPushButton = acceptBtn;
        QAbstractButton *rejectPushButton = rejectBtn;

        // F5-F2: the recognition probe below only pins the DEFAULTS if the
        // screen's own checkboxes MATCH the pipeline. Record what the shipped
        // defaults show; asserted permanently after the probe passes (so a
        // pre-fix failure demonstrates the recognition defect itself).
        auto defaultChkOn = [ocr](const char *name) {
            QAbstractButton *b = ocr->findChild<QAbstractButton *>(QString::fromLatin1(name));
            return b && b->isChecked();
        };
        const bool deskewShownOn   = defaultChkOn("ocrChkDeskew");
        const bool binarizeShownOn = defaultChkOn("ocrChkBinarize");
        const bool denoiseShownOn  = defaultChkOn("ocrChkDenoise");
        step(QStringLiteral("F5-F2 shipped defaults as shown on screen: "
                            "deskew=%1 binarize=%2 denoise=%3")
                 .arg(deskewShownOn).arg(binarizeShownOn).arg(denoiseShownOn));

        // Narrated OCR wait: engine init (Tesseract language seed + up to 3
        // ONNX sessions) is one-time and disk/CPU-bound and can take minutes
        // on a cold/contended machine. Poll the text pane + Run state and
        // TRANSCRIPT the wait every 15s, so the evidence shows engine progress
        // instead of a silent hang. Returns whether recognized text arrived.
        auto narratedOcrWait = [&](int budgetMs, const QString &tag) {
            QElapsedTimer clock;
            clock.start();
            for (;;) {
                if (!textEdit->toPlainText().trimmed().isEmpty())
                    return true;
                if (clock.elapsed() >= budgetMs) {
                    step(QStringLiteral("F5 %1: budget %2s exhausted; Run enabled=%3 "
                                        "textLen=%4")
                             .arg(tag).arg(clock.elapsed() / 1000)
                             .arg(runPushButton->isEnabled())
                             .arg(textEdit->toPlainText().size()));
                    return false;
                }
                // Fast ticks early: failure disclosures are 7s statusBar
                // transients — a 15s first tick would miss them.
                QTest::qWait(clock.elapsed() < 30000 ? 3000 : 15000);
                step(QStringLiteral("F5 %1: waiting… %2s; Run enabled=%3 textLen=%4 "
                                    "statusBar='%5'")
                         .arg(tag).arg(clock.elapsed() / 1000)
                         .arg(runPushButton->isEnabled())
                         .arg(textEdit->toPlainText().size())
                         .arg(m_win->statusBar()->currentMessage().left(120)));
            }
        };

        // Run on the IMAGE page.
        runPushButton->click();
        // F5-F1 pin: while the run is in flight — cold engine init on first
        // use takes minutes — the OCR screen ITSELF must carry the lifecycle
        // message, not just a status-bar transient.
        QLabel *lifecycleLbl = ocr->findChild<QLabel *>(QStringLiteral("ocrLifecycleLabel"));
        QVERIFY2(lifecycleLbl, "F5-F1: ocrLifecycleLabel missing from the OCR screen");
        QVERIFY2(lifecycleLbl->isVisible() && !lifecycleLbl->text().isEmpty(),
                 qPrintable(QStringLiteral("F5-F1: the OCR screen must surface the "
                              "lifecycle message while a run is in flight "
                              "(visible=%1 text='%2')")
                                .arg(lifecycleLbl->isVisible())
                                .arg(lifecycleLbl->text().left(120))));
        step(QStringLiteral("F5-F1 live lifecycle surface during run1: '%1'")
                 .arg(lifecycleLbl->text().left(160)));
        QVERIFY2(narratedOcrWait(250000, QStringLiteral("run1(image page)")),
                 "F5: OCR produced no recognized text within the narrated 250s "
                 "budget (Run re-enabled with text pane empty = the panel showed "
                 "an honest failure state; see transcript above)");
        const QString recognized = textEdit->toPlainText();
        step(QStringLiteral("F5 step2 verified: recognized text: '%1'")
                 .arg(recognized.left(120)));
        QVERIFY2(recognized.contains(QStringLiteral("42")) || recognized.contains(QStringLiteral("OCR")),
                 "F5: the recognized text should carry the fixture's content");
        // F5-F2 pin (permanent): with the SHIPPED defaults the clean scan must
        // be RECOGNIZED — the audit observed "OCR Complete. 0 text blocks
        // detected." on exactly this fixture when the default destructive
        // chain ran. Honesty guard: the checkboxes must show what the pipeline
        // will do — deskew/binarize/denoise OFF out of the box.
        QVERIFY2(!deskewShownOn && !binarizeShownOn && !denoiseShownOn,
                 qPrintable(QStringLiteral("F5-F2: the shipped preprocessing defaults "
                              "must be honest — destructive deskew/binarize/denoise "
                              "OFF out of the box (screen showed deskew=%1 binarize=%2 "
                              "denoise=%3)")
                                .arg(deskewShownOn).arg(binarizeShownOn)
                                .arg(denoiseShownOn)));

        // Reject → the user must be told; state must be retryable.
        // Accept first (so reject has review state afterwards): Accept exports
        // the searchable COPY through a Save dialog — drive it to a temp path
        // (the same non-native dialog driver the other flows use).
        const QString ocrExport = dir.filePath("searchable-copy.pdf");
        pickFilesInSequence({ ocrExport });
        acceptPushButton->click();   // accept first so reject has state to discard
        QTest::qWait(300);
        rejectPushButton->click();
        QTest::qWait(500);
        step(QStringLiteral("F5 step3: accept (save dialog driven to %1) then "
                            "reject clicked — status text captured in transcript "
                            "below").arg(ocrExport));
        // Re-Ocr guard: after a reject, Run must be available again (a
        // dead Run button here would be a dead-end finding).
        QVERIFY2(runPushButton->isEnabled(),
                 "F5: Run must be re-armed after a reject (no dead end)");
        runPushButton->click();
        QVERIFY2(narratedOcrWait(45000, QStringLiteral("run2(after reject)")),
                 "F5: re-Ocr after reject produced no text within the narrated "
                 "60s budget (engines are warm after run1)");
        step("F5 step4 verified: re-Ocr after reject completes again");

        // ── Text page: run OCR on a page that ALREADY has text. What does the
        // user see? (the single-page skip-disclosure probe)
        const QString textPdf = dir.filePath("digital.pdf");
        makePdf(textPdf, "DIGITAL TEXT PAGE");
        m_win->openDocument(textPdf);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        QTest::qWait(500);
        // The pane still carries run2's words, so clear it to make the wait
        // meaningful for THIS run.
        textEdit->clear();
        runPushButton->click();
        narratedOcrWait(90000, QStringLiteral("run3(already-digital page)"));
        // A text-dense page takes the recognizer longer than run1's 3 words;
        // whatever the outcome, the run must reach a TERMINAL (retryable)
        // state that tells the user where they stand.
        QElapsedTimer termClock;
        termClock.start();
        while (!runPushButton->isEnabled() && termClock.elapsed() < 90000)
            QTest::qWait(3000);
        step(QStringLiteral("F5 step5 (text page): OCR on an already-digital page → "
                           "text pane now: '%1' (empty = a 'no text recognized' state; "
                           "content = OCR ran with NO already-text disclosure); "
                           "terminal after %2s (Run enabled=%3)")
                 .arg(textEdit->toPlainText().left(80))
                 .arg(termClock.elapsed() / 1000).arg(runPushButton->isEnabled()));
        // Either way the run must COMPLETE honestly (retryable idle or words).
        QVERIFY2(runPushButton->isEnabled(),
                 "F5: the run must end in a state that tells the user where they stand");
    }

    // ── F6: cert-encrypt 2 recipients → open-with-owner sanity ───────────────
    void flow6_certEncrypt_twoRecipients()
    {
        #ifdef SOURCE_DIR
        const QString fixtures = QStringLiteral(SOURCE_DIR "/tests/fixtures/signing");
#else
        const QString fixtures = QStringLiteral("tests/fixtures/signing");
#endif
        const QString crt1 = fixtures + QStringLiteral("/signer.crt");
        const QString crt2 = fixtures + QStringLiteral("/weak.crt");
        if (!QFileInfo::exists(crt1) || !QFileInfo::exists(crt2))
            QSKIP("certificate fixtures not present");
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString doc = dir.filePath("sensitive.pdf");
        makePdf(doc, "F6SENSITIVE");
        const QString encOut = dir.filePath("sensitive_enc.pdf");

        runCardRoute("open", doc);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);

        bool pickerSeen = false;
        driveModalDialog(QStringLiteral("recipientList"), [&](QWidget *w) {
            auto *picker = qobject_cast<RecipientPickerDialog *>(w);
            if (!picker) return;
            const bool added = picker->addRecipientPaths({ crt1, crt2 });
            step(QStringLiteral("F6: recipient certs accepted=%1 count=%2; disclosure: '%3'")
                     .arg(added).arg(picker->recipientCount())
                     .arg(picker->findChild<QLabel *>(QStringLiteral("recipientDisclosureLabel"))
                              ? picker->findChild<QLabel *>(
                                    QStringLiteral("recipientDisclosureLabel"))->text().left(160)
                              : QStringLiteral("<none>")));
            QVERIFY2(picker->recipientCount() == 2, "F6: both recipients must be accepted");
            if (auto *bb = w->findChild<QDialogButtonBox *>(
                    QStringLiteral("recipientButtonBox")))
                if (auto *ok = bb->button(QDialogButtonBox::Ok))
                    ok->click();
        }, &pickerSeen, 30000);
        pickFilesInSequence({ encOut });
        m_win->onToolActivated(QStringLiteral("certEncrypt"));
        QTRY_VERIFY_WITH_TIMEOUT(pickerSeen, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(encOut), 60000);

        // Landed state: the output really is a PubSec-encrypted document.
        // GetEncrypt() is the WRONG probe for certificate encryption: PoDoFo
        // only instantiates the STANDARD security handler, so it stays null
        // even on a genuinely PubSec-encrypted file (proven by TestEncryption's
        // roundtrip). Probe the trailer /Encrypt dict + Filter + Recipients
        // (the Adobe public-key contract a conformant reader follows).
        bool encrypted = false;
        QString encProbeDetail;
        // PoDoFo 1.1: PdfName::GetString() yields std::string_view.
        auto pdfNameStr = [](const PoDoFo::PdfObject *o) -> QString {
            if (!o || !o->IsName()) return QStringLiteral("<none>");
            const std::string_view sv = o->GetName().GetString();
            return QString::fromUtf8(sv.data(), static_cast<int>(sv.size()));
        };
        try {
            PoDoFo::PdfMemDocument d2;
            d2.Load(encOut.toUtf8().constData());
            const auto &trailer = d2.GetTrailer();
            if (!trailer.GetDictionary().HasKey("Encrypt")) {
                encProbeDetail = QStringLiteral("no /Encrypt in trailer — PLAINTEXT output");
            } else {
                const PoDoFo::PdfObject *encRef =
                    trailer.GetDictionary().GetKey("Encrypt");
                if (!encRef || !encRef->IsReference()) {
                    encProbeDetail = QStringLiteral("/Encrypt is not an indirect reference");
                } else {
                    auto &encObj =
                        d2.GetObjects().MustGetObject(encRef->GetReference());
                    const auto &ed = encObj.GetDictionary();
                    const bool pubSecFilter =
                        ed.HasKey("Filter") && pdfNameStr(ed.GetKey("Filter"))
                               == QLatin1String("PubSec");
                    const bool hasRecipients = ed.HasKey("Recipients");
                    encrypted = pubSecFilter && hasRecipients;
                    encProbeDetail = QStringLiteral(
                         "/Encrypt present: Filter=%1 PubSec=%2 Recipients=%3")
                         .arg(pdfNameStr(ed.GetKey("Filter")))
                         .arg(pubSecFilter).arg(hasRecipients);
                }
            }
        } catch (const std::exception &e) {
            encProbeDetail = QStringLiteral("parser could not open output: %1").arg(e.what());
        }
        step(QStringLiteral("F6 verified: encrypted output written; probe: %1")
                 .arg(encProbeDetail));
        QVERIFY2(encrypted,
                 "F6: the output must carry a PubSec /Encrypt dictionary with "
                 "recipient envelopes");

        // Open-with-owner probe: opening the encrypted output must not lie —
        // capture exactly what the user is told.
        static QString openMsg, openTitle;  // static: poller may outlive the slot
        captureModalText(&openMsg, &openTitle, nullptr, QString(), 30000);
        m_win->openDocument(encOut);
        QTest::qWait(4000);
        step(QStringLiteral("F6 open-with-owner: modal title='%1' text='%2'; "
                           "viewer page count now %3")
                 .arg(openTitle, openMsg.left(250))
                 .arg(m_win->pdfViewer()->pageCount()));
        // F6-F1 (FIXED, permanent pin): the artifact is VALID — this viewer
        // just cannot decrypt it — so the open-failure disclosure must name
        // the certificate-encrypted state. The regression this guards against
        // is the generic "Could not open the PDF document" — the same wording
        // a corrupt file gets, which dead-ends a user who JUST encrypted the
        // file with zero mention of certificates.
        QVERIFY2(openMsg.contains(QStringLiteral("certificate-encrypted"),
                                  Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("F6-F1: the open-failure disclosure for "
                              "the app's own PubSec-encrypted output must name the "
                              "certificate-encrypted state (modal said: '%1')")
                                .arg(openMsg.left(200))));
        QVERIFY2(m_win->pdfViewer()->pageCount() == 0,
                 "F6-F1: the PubSec-encrypted document must not render in the "
                 "viewer (it cannot be decrypted here)");
    }

    // ── F7: accessibility scan → fix /Lang + /Alt → rescan; honesty box ──────
    void flow7_accessibility_scan_fix_rescan()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString doc = dir.filePath("a11y-doc.pdf");
        // No /Lang, no /Title, one image without /Alt.
        {
            QImage img(40, 20, QImage::Format_RGB32);
            img.fill(QColor(180, 30, 30));
            const QString pngPath = dir.filePath("fig.png");
            QVERIFY(img.save(pngPath, "PNG"));
            PoDoFo::PdfMemDocument pdf;
            auto &page = pdf.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto &font = pdf.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);
            painter.DrawText("A11y audit page", 50, 750);
            try {
                std::unique_ptr<PoDoFo::PdfImage> pdfImg = pdf.CreateImage();
                pdfImg->Load(pngPath.toUtf8().constData());
                painter.DrawImage(*pdfImg, 60.0, 600.0, 200.0, 100.0);
            } catch (const std::exception &e) {
                qWarning() << "image embed failed (alt check may be vacuous):" << e.what();
            }
            painter.FinishDrawing();
            pdf.Save(doc.toUtf8().constData());
        }
        QVERIFY(QFileInfo::exists(doc));
        step("F7 start: fixture without /Lang, without /Title, with an image w/o /Alt");

        runCardRoute("open", doc);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);

        // The accessibility checker is a TASK SCREEN, not a welcome card: the
        // real first-run route is the task nav ("accessibility"), which
        // creates + hosts the panel on first entry (GpMainWindow lazy seam).
        m_win->activateScreen(QStringLiteral("accessibility"));
        auto *panel = m_win->findChild<gp::AccessibilityPanel *>();
        QVERIFY2(panel, "F7: the accessibility panel must be hosted in the window");
        auto *status = panel->findChild<QLabel *>(QStringLiteral("a11yStatusLabel"));
        auto *disclosure = panel->findChild<QLabel *>(QStringLiteral("a11yDisclosureLabel"));
        QVERIFY(status && disclosure);

        // Honesty box: must bound what the report means (never overclaim).
        // Contract update (PR-review §3.5): bf85bfbd (T2-4 P2 accessibility
        // panel) legitimately EXTENDED the P1 disclosure — "Detection only"
        // became "Detection and tagging", the "never certifies" sentence is
        // retained verbatim. Assert the new wording, keep the never-certifies
        // check (the honest bound is what F7 pins, not the old phrase).
        const QString disc = disclosure->text();
        step(QStringLiteral("F7 disclosure box: '%1'").arg(disc));
        QVERIFY2(disc.contains(QStringLiteral("Detection and tagging"), Qt::CaseInsensitive)
                     && disc.contains(QStringLiteral("never certifies"), Qt::CaseInsensitive),
                 "F7 honesty: the disclosure must say detection-and-tagging, never a certification");

        // Explicit scan.
        auto *runBtn = panel->findChild<QPushButton *>(QStringLiteral("a11yRunButton"));
        QVERIFY(runBtn);
        runBtn->click();
        QTRY_VERIFY_WITH_TIMEOUT(
            status->text().contains(QStringLiteral("gap"), Qt::CaseInsensitive)
                || status->text().contains(QStringLiteral("No gaps"), Qt::CaseInsensitive),
            60000);
        step(QStringLiteral("F7 scan verdict: '%1'").arg(status->text()));

        // Fix loop: apply the cheap fixes one round at a time (each Apply
        // re-scans). Stop when no enabled FIX remains or no gaps remain.
        // Any MODAL during the loop is unexpected (the editors are inline):
        // record + dismiss it BY CLICKING (close() does not resolve a
        // one-button QMessageBox exec loop), so the evidence lands in the
        // transcript instead of a blocked event loop.
        bool f7OkSeen = false, f7YesSeen = false, f7NoSeen = false;
        clickPromptButton(QStringLiteral("OK"), &f7OkSeen);
        clickPromptButton(QStringLiteral("Yes"), &f7YesSeen);
        clickPromptButton(QStringLiteral("No"), &f7NoSeen);
        for (int round = 0; round < 4; ++round) {
            const auto fixButtons = panel->findChildren<QPushButton *>();
            QPushButton *target = nullptr;
            int idx = -1;
            for (QPushButton *b : fixButtons) {
                const QString n = b->objectName();
                if (n.startsWith(QStringLiteral("a11yFixButton_")) && b->isEnabled()) {
                    const int cand = n.mid(QStringLiteral("a11yFixButton_").size()).toInt();
                    if (idx < 0 || cand < idx) {
                        idx = cand;
                        target = b;
                    }
                }
            }
            if (!target) {
                step(QStringLiteral("F7 round %1: no enabled FIX buttons remain").arg(round));
                break;
            }
            step(QStringLiteral("F7 round %1: opening editor for finding #%2 "
                               "(status before: '%3')")
                     .arg(round).arg(idx).arg(status->text()));
            target->click();
            QTest::qWait(400);   // the inline editor frame is built synchronously
            driveModallessEditor(panel);
            QTest::qWait(300);
            // Apply happened inside driveModallessEditor via the Apply button;
            // wait for the re-scan to settle.
            QTRY_VERIFY_WITH_TIMEOUT(
                status->text().contains(QStringLiteral("gap"), Qt::CaseInsensitive)
                    || status->text().contains(QStringLiteral("No gaps"), Qt::CaseInsensitive)
                    || status->text().contains(QStringLiteral("Fix not applied"),
                                               Qt::CaseInsensitive),
                60000);
            step(QStringLiteral("F7 round %1: post-fix status: '%2'").arg(round)
                     .arg(status->text()));
            if (status->text().contains(QStringLiteral("No gaps"), Qt::CaseInsensitive))
                break;
        }

        // display-doc-title WITHOUT a /Title must stay an honest refusal —
        // find its FIX button and record the disabled tooltip.
        for (QPushButton *b : panel->findChildren<QPushButton *>()) {
            if (b->objectName().startsWith(QStringLiteral("a11yFixButton_"))
                && !b->isEnabled()) {
                step(QStringLiteral("F7 honesty-refusal recorded: a FIX button is "
                                   "disabled with tooltip: '%1'").arg(b->toolTip()));
            }
        }
        step(QStringLiteral("F7 final status: '%1'; unexpected modals during fix "
                           "loop: OK=%2 Yes=%3 No=%4")
                 .arg(status->text()).arg(f7OkSeen).arg(f7YesSeen).arg(f7NoSeen));
    }

    // ── F7b (PR-review §3.1): the tag route is a MUTATION — a read-only
    // session must refuse it with the one honest wording, before any
    // pre-flight surface, with the file byte-identical and the shell
    // runner's hard stop never crossed.
    void flow7b_tag_refused_on_read_only_session()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString doc = dir.filePath("taggable-ro.pdf");
        {
            PoDoFo::PdfMemDocument pdf;
            auto &page = pdf.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto &font = pdf.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);
            painter.DrawText("Tag me if you can", 50, 750);
            painter.FinishDrawing();
            pdf.Save(doc.toUtf8().constData());
        }
        QVERIFY(QFileInfo::exists(doc));
        step("F7b start: untagged fixture, open through the real route");

        runCardRoute("open", doc);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);

        m_win->activateScreen(QStringLiteral("accessibility"));
        auto *panel = m_win->findChild<gp::AccessibilityPanel *>();
        QVERIFY2(panel, "F7b: the accessibility panel must be hosted");
        auto *tagBtn = panel->findChild<QPushButton *>(QStringLiteral("a11yTagButton"));
        auto *status = panel->findChild<QLabel *>(QStringLiteral("a11yStatusLabel"));
        QVERIFY(tagBtn && status);
        // Scan settled: the taggable document enables the Tag action.
        QTRY_VERIFY_WITH_TIMEOUT(tagBtn->isEnabled(), 20000);

        // Flip the REAL session read-only — the production authority the
        // shell's injected gate asks (EditPolicy::mutationBlocked).
        QVERIFY(m_win->appContext() && m_win->appContext()->document);
        m_win->appContext()->document->setReadOnly(true);

        QFile before(doc);
        QVERIFY(before.open(QIODevice::ReadOnly));
        const QByteArray originalBytes = before.readAll();
        before.close();

        tagBtn->click();

        // Honest refusal in the panel's status line — no pre-flight surface.
        QTRY_VERIFY_WITH_TIMEOUT(
            status->text().contains(QStringLiteral("read-only"),
                                    Qt::CaseInsensitive),
            10000);
        step(QStringLiteral("F7b refusal status: '%1'").arg(status->text()));
        auto *confirm = panel->findChild<QWidget *>(QStringLiteral("a11yTagConfirm"));
        QVERIFY2(!confirm || !confirm->isVisible(),
                 "F7b: a read-only refusal must not render the pre-flight");

        QTest::qWait(300);   // a (wrong) async tagging run would surface here
        QFile after(doc);
        QVERIFY(after.open(QIODevice::ReadOnly));
        const QByteArray currentBytes = after.readAll();
        after.close();
        QVERIFY2(currentBytes == originalBytes,
                 "F7b: a read-only session must leave the file byte-identical");
        QVERIFY2(QFileInfo::exists(doc), "F7b: the document must survive");

        // Restore the session so a re-run of the slot starts clean.
        m_win->appContext()->document->setReadOnly(false);
        step("F7b verified: read-only session refuses tagging; file untouched");
    }

    // The a11y fix editor is an INLINE frame (not modal): fill the language
    // combo or alt-text edit and click Apply inside the panel.
    void driveModallessEditor(gp::AccessibilityPanel *panel)
    {
        if (auto *combo = panel->findChild<QComboBox *>(QStringLiteral("a11yLanguageCombo"))) {
            combo->setCurrentIndex(0);   // a real language choice
            if (auto *apply = panel->findChild<QPushButton *>(
                    QStringLiteral("a11yApplyFixButton")))
                apply->click();
            step("F7 editor: language chosen and Apply clicked");
        } else if (auto *edit = panel->findChild<QLineEdit *>(
                       QStringLiteral("a11yAltTextEdit"))) {
            edit->setText(QStringLiteral("Chart of monthly sales"));
            if (auto *apply = panel->findChild<QPushButton *>(
                    QStringLiteral("a11yApplyFixButton")))
                apply->click();
            step("F7 editor: alt text set and Apply clicked");
        } else {
            step("F7 editor: NO editor widgets appeared for this finding");
        }
    }
};

QTEST_MAIN(TestSweepW3UxFlows)
#include "TestSweepW3UxFlows.moc"
