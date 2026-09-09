// SPDX-License-Identifier: Apache-2.0
// G14 (QUALITY-GATE-2026-09-09) regression tests: sidecar-only annotation
// edits must keep their unsaved-PDF state across a document reopen.
//
// Reproduced defect (reviewer probe, architecture/probe.log):
//   ANNOTATION_DIRTY true
//   DIRTY_SWITCH_PROMPTS 0 ... A_SIDECAR_OWNS_NOTE true
//   REOPEN_UNEMBEDDED_NOTE_DIRTY false ANNOTATIONS 1
// — the comment flushed to A's sidecar correctly (no cross-document leak;
// that ARC02 part passed), but no Save/Discard/Cancel guard ran, and
// reopening A restored the unembedded comment while marking the session
// CLEAN although the PDF on disk still lacked the annotation.
//
// Contract pinned here (all on real saved artifacts + the real shell):
//   * the sidecar is INTERMEDIATE durability: it records whether its
//     annotations are committed INTO the PDF ("embeddedIntoPdf" envelope);
//   * leaving a document with unembedded annotation work runs the explicit
//     checked transition (Save / Discard / Cancel) at the openDocument
//     boundary — Save must be a checked success (annotations really in the
//     PDF, verified via PoDoFo readback), Discard keeps sidecar-only
//     durability, Cancel aborts the open;
//   * reopening a document whose sidecar recorded UNEMBEDDED work restores
//     the annotations AND the dirty (pending-embed) session state;
//   * reopening after a real commit is CLEAN (no resurrected "unsaved" work);
//   * legacy array sidecars (no commit information) never fabricate pending
//     state — the pre-fix clean-reopen behavior is preserved for old files.
//
// Modal driving follows the validated pattern from
// .context/research/step3-persistence-topics.md (§3): a zero/10 ms timer that
// scans QApplication::topLevelWidgets() and clicks the requested button of
// the "Unsaved Changes" box inside its nested event loop.
#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QFile>
#include <QTimer>
#include <QMessageBox>
#include <QSettings>
#include <QPushButton>
#include <QPdfWriter>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonObject>

#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "core/AnnotationSerializer.h"
#include "core/PdfEnums.h"
#include "engines/DocumentSession.h"
#include "engines/podofo/PoDoFoBackend.h"
#include "ui/PdfViewerWidget.h"

using gp::MainWindow;

namespace {

void makePdf(const QString &path, const QByteArray &marker)
{
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    QPainter p(&w);
    p.drawText(100, 100, marker);
    p.end();
    QVERIFY(QFileInfo::exists(path));
}

AnnotationItem commentOnPage0(const QString &text)
{
    AnnotationItem note;
    note.pageIndex = 0;
    note.mode = ToolMode::AddComment;
    note.rect = QRectF(70, 70, 120, 24);
    note.text = text;
    return note;
}

// Modal driver: which button of the "Unsaved Changes" box to click when it
// appears (empty = just dismiss boxes without choosing).
struct ModalDriver {
    QTimer timer;
    QString choose;          // "Save" / "Discard" / "" (generic accept)
    int prompts = 0;

    void start(const QString &button)
    {
        choose = button;
        prompts = 0;   // per-scenario count (cleanup only stops the timer)
        QObject::connect(&timer, &QTimer::timeout, [this] {
            for (auto *w : QApplication::topLevelWidgets()) {
                if (auto *box = qobject_cast<QMessageBox *>(w)) {
                    if (!box->isVisible()) continue;
                    if (box->windowTitle() == QStringLiteral("Unsaved Changes")) {
                        ++prompts;
                        if (!choose.isEmpty()) {
                            for (auto *btn : box->buttons())
                                if (btn->text() == choose) { btn->click(); return; }
                        }
                    } else {
                        box->accept();   // dismiss unrelated boxes (e.g. save errors)
                    }
                }
            }
        });
        timer.start(10);
    }
    void stop() { timer.stop(); }
};

} // namespace

class TestSidecarReopenState : public QObject {
    Q_OBJECT

    std::unique_ptr<MainWindow> m_win;
    ModalDriver m_modals;

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestSidecarReopenState"));
        // Infra pin (QUALITY-GATE-2026-09-09): the MainWindow ctor's
        // orphaned-autosave dialog is driven by PERSISTED recents; one
        // interrupted run must not poison later runs with a modal nobody
        // dismisses. This suite never relies on recents — start clean.
        QSettings settings;
        settings.remove(QStringLiteral("recentFiles"));
    }

    void init()
    {
        m_win = std::make_unique<MainWindow>(Bootstrapper::createContext());
        QVERIFY(m_win->pdfViewer());
        QVERIFY(m_win->appContext()->document);
    }

    void cleanup()
    {
        m_modals.stop();
        m_win.reset();
    }

    // ── Core repro: add a comment to A, open B (Discard), reopen A ──────────
    // The work is sidecar-only; the reopen must restore the annotations AND
    // the dirty pending-embed state (pre-fix: session came back CLEAN).
    void reopenWithUnembeddedSidecarAnnotationsIsDirty()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        const QString b = dir.filePath("b.pdf");
        makePdf(a, "DOC_A");
        makePdf(b, "DOC_B");
        auto *ctx = m_win->appContext();
        auto *viewer = m_win->pdfViewer();

        m_win->openDocument(a);
        viewer->setAnnotations({ commentOnPage0(QStringLiteral("ONLY_A_GATE_NOTE")) });
        QVERIFY(ctx->document->isDirty());

        // Leave A: the checked transition appears; Discard keeps the
        // sidecar-only durability and proceeds to B.
        m_modals.start(QStringLiteral("Discard"));
        m_win->openDocument(b);
        m_modals.stop();
        QCOMPARE(m_modals.prompts, 1);
        QCOMPARE(ctx->document->path(), b);
        QCOMPARE(viewer->annotations().size(), 0);         // no cross-document leak
        QVERIFY(QFileInfo::exists(a + ".ann"));

        // The sidecar envelope records the work as NOT committed into the PDF.
        QFile ann(a + ".ann");
        QVERIFY(ann.open(QIODevice::ReadOnly));
        const QJsonDocument doc = QJsonDocument::fromJson(ann.readAll());
        ann.close();
        QVERIFY2(doc.isObject(), "the sidecar must use the envelope shape");
        QCOMPARE(doc.object().value(QStringLiteral("embeddedIntoPdf")).toBool(true), false);
        QCOMPARE(doc.object().value(QStringLiteral("annotations")).toArray().size(), 1);

        // Reopen A: annotations restored AND the session is dirty — the PDF on
        // disk still lacks the annotation, and the state says so.
        m_win->openDocument(a);
        QCOMPARE(viewer->annotations().size(), 1);
        QVERIFY2(ctx->document->isDirty(),
                 "G14: reopening a document with unembedded sidecar annotations "
                 "must restore the pending-embed dirty state");
        QVERIFY(viewer->hasPendingEmbedAnnotations());
    }

    // ── The checked transition: Cancel aborts the open, nothing changes ─────
    void cancelOnPendingAnnotationSwitchAbortsTheOpen()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        const QString b = dir.filePath("b.pdf");
        makePdf(a, "DOC_A");
        makePdf(b, "DOC_B");
        auto *ctx = m_win->appContext();
        auto *viewer = m_win->pdfViewer();

        m_win->openDocument(a);
        viewer->setAnnotations({ commentOnPage0(QStringLiteral("STAY_ON_A")) });

        m_modals.start(QStringLiteral("Cancel"));
        m_win->openDocument(b);
        m_modals.stop();

        QCOMPARE(m_modals.prompts, 1);
        QVERIFY2(viewer->filePath() == a,
                 "G14: Cancel must abort the document switch");
        QCOMPARE(ctx->document->path(), a);
        QCOMPARE(viewer->annotations().size(), 1);
        QVERIFY(viewer->hasPendingEmbedAnnotations());
    }

    // ── Save in the checked transition really commits into the PDF ──────────
    // The reopened document must be CLEAN (the annotations are IN the PDF),
    // proven by a PoDoFo readback of the saved artifact.
    void saveOnSwitchEmbedsAnnotationsAndReopenIsClean()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        const QString b = dir.filePath("b.pdf");
        makePdf(a, "DOC_A");
        makePdf(b, "DOC_B");
        auto *ctx = m_win->appContext();
        auto *viewer = m_win->pdfViewer();

        m_win->openDocument(a);
        viewer->setAnnotations({ commentOnPage0(QStringLiteral("COMMITTED_NOTE")) });

        m_modals.start(QStringLiteral("Save"));
        m_win->openDocument(b);
        m_modals.stop();
        QCOMPARE(m_modals.prompts, 1);
        QCOMPARE(ctx->document->path(), b);
        QVERIFY2(!ctx->document->isDirty(),
                 "the checked Saved outcome clears the session");

        // Disk truth: the sidecar records committed state…
        QFile ann(a + ".ann");
        QVERIFY(ann.open(QIODevice::ReadOnly));
        const QJsonDocument doc = QJsonDocument::fromJson(ann.readAll());
        ann.close();
        QCOMPARE(doc.object().value(QStringLiteral("embeddedIntoPdf")).toBool(false), true);

        // …and the PDF itself carries the annotation (PoDoFo readback).
        {
            PoDoFoBackend readback;
            const auto embedded = readback.extractAnnotations(a);
            bool found = false;
            for (const auto &item : embedded)
                if (item.text.contains(QStringLiteral("COMMITTED_NOTE"))) found = true;
            QVERIFY2(found, "G14: Save must commit the annotation INTO the PDF");
        }

        // Reopen: clean — committed work is not resurrected as unsaved.
        m_win->openDocument(a);
        QVERIFY2(!ctx->document->isDirty(),
                 "G14: reopening after a real commit must be clean");
        QVERIFY(!viewer->hasPendingEmbedAnnotations());
        QCOMPARE(viewer->annotations().size(), 1);
    }

    // ── Upgrade safety: legacy ARRAY sidecars never fabricate pending state ─
    void legacyArraySidecarReopensClean()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a, "LEGACY");
        {
            QFile f(a + ".ann");
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(AnnotationSerializer::toJson(
                        { commentOnPage0(QStringLiteral("OLD_NOTE")) }).toJson());
        }
        auto *ctx = m_win->appContext();

        m_win->openDocument(a);
        QCOMPARE(m_win->pdfViewer()->annotations().size(), 1);
        QVERIFY2(!ctx->document->isDirty(),
                 "a legacy sidecar carries no commit information — the "
                 "pre-fix clean reopen is preserved (no fabricated work)");
    }
};

QTEST_MAIN(TestSidecarReopenState)
#include "TestSidecarReopenState.moc"
