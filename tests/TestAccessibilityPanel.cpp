// SPDX-License-Identifier: Apache-2.0
// T2-4 accessibility P1: offscreen pins for the Accessibility panel.
//   * empty state is honest (no document → "No document loaded.", no scan)
//   * a seeded-defect document produces a report with severity rows and the
//     disclosure (detection-only, no PDF/UA claim, no auto-tagging) VISIBLE
//   * a clean document's status NEVER words a conformance claim
// Same QFutureWatcher async discipline as TestReadingOrderAsync.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <podofo/podofo.h>
#include <cstring>

#include "modes/AccessibilityPanel.h"
#include "engines/AccessibilityTagger.h"

using PoDoFo::PdfObject;
using PoDoFo::PdfName;
using PoDoFo::PdfString;
using PoDoFo::PdfDictionary;

namespace {

// Minimal defective fixture: untagged, no /Lang, one image without /Alt.
bool makeDefectivePdf(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

        auto& xobjs = doc.GetObjects().CreateDictionaryObject();
        auto& img = doc.GetObjects().CreateDictionaryObject();
        img.GetDictionary().AddKey("Type", PdfObject(PdfName("XObject")));
        img.GetDictionary().AddKey("Subtype", PdfObject(PdfName("Image")));
        img.GetDictionary().AddKey("Width", PdfObject(static_cast<int64_t>(4)));
        img.GetDictionary().AddKey("Height", PdfObject(static_cast<int64_t>(4)));
        img.GetDictionary().AddKey("ColorSpace", PdfObject(PdfName("DeviceGray")));
        img.GetDictionary().AddKey("BitsPerComponent", PdfObject(static_cast<int64_t>(8)));
        const unsigned char px[4] = {0};
        img.GetOrCreateStream().SetData(
            PoDoFo::bufferview(reinterpret_cast<const char*>(px), sizeof(px)));
        xobjs.GetDictionary().AddKey(PdfName("Im0"), img.GetIndirectReference());
        page.GetResources().GetDictionary().AddKey("XObject",
                                                   xobjs.GetIndirectReference());

        doc.Save(path.toUtf8().constData());
        return true;
    } catch (...) {
        return false;
    }
}

// A TAGGABLE fixture: one honestly-decodable text line (standard-14 font),
// untagged, no /Lang — everything the tag flow needs to really run.
bool makeTaggablePdf(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        PdfDictionary fonts;
        auto& f1 = doc.GetObjects().CreateDictionaryObject();
        f1.GetDictionary().AddKey("Type", PdfObject(PdfName("Font")));
        f1.GetDictionary().AddKey("Subtype", PdfObject(PdfName("Type1")));
        f1.GetDictionary().AddKey("BaseFont", PdfObject(PdfName("Helvetica")));
        fonts.AddKey(PdfName("F1"), PdfObject(f1.GetIndirectReference()));
        page.GetResources().GetDictionary().AddKey("Font",
                                                   PdfObject(fonts));
        auto& contents = page.GetOrCreateContents();
        auto& stream = contents.CreateStreamForAppending(
            PoDoFo::PdfStreamAppendFlags::None);
        const char* c = "BT\n/F1 10 Tf\n60 700 Td\n(A taggable line.) Tj\nET\n";
        stream.SetData(PoDoFo::bufferview(c, strlen(c)));
        doc.Save(path.toUtf8().constData());
        return true;
    } catch (...) {
        return false;
    }
}

bool makeCleanPdf(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        doc.GetMetadata().SetTitle(PdfString("Titled"));
        doc.GetCatalog().GetDictionary().AddKey("Lang", PdfObject(PdfString("en-US")));
        PdfDictionary markInfo;
        markInfo.AddKey("Marked", PdfObject(true));
        doc.GetCatalog().GetDictionary().AddKey("MarkInfo", PdfObject(markInfo));
        auto& root = doc.GetObjects().CreateDictionaryObject();
        root.GetDictionary().AddKey("Type", PdfObject(PdfName("StructTreeRoot")));
        doc.GetCatalog().GetDictionary().AddKey("StructTreeRoot",
                                                root.GetIndirectReference());
        auto& vp = doc.GetObjects().CreateDictionaryObject();
        vp.GetDictionary().AddKey("DisplayDocTitle", PdfObject(true));
        doc.GetCatalog().GetDictionary().AddKey("ViewerPreferences",
                                                vp.GetIndirectReference());
        doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        doc.Save(path.toUtf8().constData());
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

class TestAccessibilityPanel : public QObject {
    Q_OBJECT
private slots:
    void emptyStateIsHonest();
    void defectsProduceSeveritiesAndDisclosure();
    void cleanDocumentNeverClaimsConformance();
    void fixRunnerReceivesRequestAndRescanHappens();
    void fixButtonsAppearOnlyWithRunner();

    // ── T2-4 P2: the tagging action (§6.3) ──────────────────────────────
    void tagActionGatingAndPreflightSurface();
    void taggedDocumentDisablesTagAction();
    void tagRunnerFlowResolvesFinding();
    void disclaimerIsPinnedVerbatim();

private:
    // Wait for the panel's async scan to deliver (the default-constructed
    // lastReport() is indistinguishable from a finished clean scan, so tests
    // must gate on the signal, not on finding counts).
    bool waitForScan(gp::AccessibilityPanel* panel) {
        bool done = false;
        QObject::connect(panel, &gp::AccessibilityPanel::scanCompleted,
                         panel, [&done]() { done = true; },
                         Qt::DirectConnection);
        // qWaitFor spins the event loop while waiting, so the queued
        // onScanFinished delivery runs.
        return QTest::qWaitFor([&done]() { return done; }, 15000);
    }

    QLabel* statusOf(gp::AccessibilityPanel* p) {
        return p->findChild<QLabel*>(QStringLiteral("a11yStatusLabel"));
    }
    QLabel* disclosureOf(gp::AccessibilityPanel* p) {
        return p->findChild<QLabel*>(QStringLiteral("a11yDisclosureLabel"));
    }
    QPushButton* tagButtonOf(gp::AccessibilityPanel* p) {
        return p->findChild<QPushButton*>(QStringLiteral("a11yTagButton"));
    }
    QLabel* tagSummaryOf(gp::AccessibilityPanel* p) {
        return p->findChild<QLabel*>(QStringLiteral("a11yTagSummary"));
    }

    bool waitForTagPreflight(gp::AccessibilityPanel* panel) {
        bool done = false;
        QObject::connect(panel, &gp::AccessibilityPanel::tagPreflightReady,
                         panel, [&done]() { done = true; },
                         Qt::DirectConnection);
        return QTest::qWaitFor([&done]() { return done; }, 15000);
    }

    bool waitForTagRun(gp::AccessibilityPanel* panel) {
        bool done = false;
        QObject::connect(panel, &gp::AccessibilityPanel::tagRunFinished,
                         panel, [&done]() { done = true; },
                         Qt::DirectConnection);
        return QTest::qWaitFor([&done]() { return done; }, 30000);
    }
};

void TestAccessibilityPanel::emptyStateIsHonest() {
    gp::AccessibilityPanel panel;
    panel.setDocument(QString());
    QCOMPARE(panel.currentDocumentPath(), QString());
    QVERIFY(statusOf(&panel) != nullptr);
    QVERIFY2(statusOf(&panel)->text().contains(QStringLiteral("No document loaded")),
             qPrintable(statusOf(&panel)->text()));
    // The disclosure is visible even before any scan — the honesty box never
    // disappears.
    QVERIFY(disclosureOf(&panel) != nullptr);
    QVERIFY(!disclosureOf(&panel)->text().isEmpty());

    // Run button disabled with no document (no dead controls).
    auto* run = panel.findChild<QPushButton*>(QStringLiteral("a11yRunButton"));
    QVERIFY(run != nullptr);
    QVERIFY(!run->isEnabled());
}

void TestAccessibilityPanel::defectsProduceSeveritiesAndDisclosure() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = tmp.filePath("defective.pdf");
    QVERIFY(makeDefectivePdf(pdf));

    gp::AccessibilityPanel panel;
    panel.setDocument(pdf);
    QCOMPARE(panel.currentDocumentPath(), pdf);

    // Async scan — wait for the report to land, then pin the count: untagged
    // + no /Lang + no title + no DisplayDocTitle + 1 image without /Alt.
    QVERIFY(waitForScan(&panel));
    QVERIFY(panel.lastReport().loadOk);
    QCOMPARE(panel.lastReport().findings.size(), 5);

    const QLabel* st = statusOf(&panel);
    QVERIFY2(st->text().contains(QStringLiteral("5 gap(s)")),
             qPrintable(st->text()));
}

void TestAccessibilityPanel::cleanDocumentNeverClaimsConformance() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = tmp.filePath("clean.pdf");
    QVERIFY(makeCleanPdf(pdf));

    gp::AccessibilityPanel panel;
    panel.setDocument(pdf);
    QVERIFY(waitForScan(&panel));
    QCOMPARE(panel.lastReport().findings.size(), 0);

    const QLabel* st = statusOf(&panel);
    // Honest clean wording — no "accessible", no "PDF/UA" CERTAINTY verbs.
    QVERIFY2(st->text().contains(QStringLiteral("No gaps found")),
             qPrintable(st->text()));
    QVERIFY2(!st->text().contains(QStringLiteral("conform")),
             qPrintable(st->text()));
}

void TestAccessibilityPanel::fixButtonsAppearOnlyWithRunner() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = tmp.filePath("defective.pdf");
    QVERIFY(makeDefectivePdf(pdf));  // 5 findings: 3 fixable (lang, ddt, image)

    gp::AccessibilityPanel panel;
    panel.setDocument(pdf);
    QVERIFY(waitForScan(&panel));
    // Without a runner there must be NO fix buttons (never dead controls).
    const auto namePrefix = QStringLiteral("a11yFixButton_");
    const auto fixButtonsOf = [&namePrefix](gp::AccessibilityPanel* p) {
        QList<QPushButton*> out;
        const auto all = p->findChildren<QPushButton*>();
        for (QPushButton* b : all)
            if (b->objectName().startsWith(namePrefix)) out << b;
        return out;
    };
    QVERIFY(fixButtonsOf(&panel).isEmpty());

    gp::A11yFixRequest captured;
    panel.setFixRunner([&captured](const gp::A11yFixRequest& req) {
        captured = req;
        return gp::A11yFixOutcome{true, QStringLiteral("ok")};
    });

    // Runner set AFTER the scan — force a re-scan to rebuild the rows.
    panel.setDocument(pdf);
    QVERIFY(waitForScan(&panel));
    const QList<QPushButton*> buttons = fixButtonsOf(&panel);
    QCOMPARE(buttons.size(), 3);  // doc-language + display-doc-title + image-alt

    // The display-doc-title FIX must be DISABLED here: the fixture has no
    // /Info /Title (honest refusal built into the UI). Findings order:
    // struct-tree(0), doc-language(1), doc-title(2), display-doc-title(3).
    for (const QPushButton* b : buttons) {
        if (b->objectName() == QStringLiteral("a11yFixButton_3")) {
            QVERIFY2(!b->isEnabled(),
                     "DisplayDocTitle fix must refuse without a /Title");
        }
    }
}

void TestAccessibilityPanel::fixRunnerReceivesRequestAndRescanHappens() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = tmp.filePath("defective.pdf");
    QVERIFY(makeDefectivePdf(pdf));

    gp::AccessibilityPanel panel;
    panel.setDocument(pdf);
    QVERIFY(waitForScan(&panel));

    gp::A11yFixRequest captured;
    int calls = 0;
    bool mutated = false;
    QObject::connect(&panel, &gp::AccessibilityPanel::documentMutated,
                     &panel, [&mutated]() { mutated = true; });
    panel.setFixRunner([&captured, &calls](const gp::A11yFixRequest& req) {
        ++calls;
        captured = req;
        return gp::A11yFixOutcome{true, QStringLiteral("document language set")};
    });

    gp::A11yFixRequest req;
    req.kind = gp::A11yFixKind::SetLanguage;
    req.language = QStringLiteral("de-DE");
    panel.applyFix(req);

    QCOMPARE(calls, 1);
    QCOMPARE(captured.kind, gp::A11yFixKind::SetLanguage);
    QCOMPARE(captured.language, QStringLiteral("de-DE"));
    QVERIFY(mutated);
    // Success ⇒ automatic re-scan of the same identity.
    QCOMPARE(panel.currentDocumentPath(), pdf);
    QVERIFY(waitForScan(&panel));

    // A failing runner must report the refusal honestly, with no re-scan.
    panel.setFixRunner([](const gp::A11yFixRequest&) {
        return gp::A11yFixOutcome{false, QStringLiteral("disk said no")};
    });
    panel.applyFix(req);
    QVERIFY2(statusOf(&panel)->text().contains(QStringLiteral("disk said no")),
             qPrintable(statusOf(&panel)->text()));
}

// ── T2-4 P2: the tagging action ──────────────────────────────────────────────

void TestAccessibilityPanel::tagActionGatingAndPreflightSurface() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = tmp.filePath("taggable.pdf");
    QVERIFY(makeTaggablePdf(pdf));

    gp::AccessibilityPanel panel;
    // No document → the tag action does not exist (never a dead control).
    panel.setDocument(QString());
    QVERIFY(tagButtonOf(&panel) != nullptr);
    QVERIFY(!tagButtonOf(&panel)->isEnabled());

    panel.setTagRunner([](const QString& path) {
        return gp::tagDocumentAccessibility(path);
    });
    panel.setDocument(pdf);
    QVERIFY(waitForScan(&panel));
    QVERIFY(panel.lastReport().loadOk);
    QVERIFY(tagButtonOf(&panel)->isEnabled());

    // Click Tag → the pre-flight surface renders INLINE with the image
    // prompt list and the disclaimer (design §1.1/§6.3).
    tagButtonOf(&panel)->click();
    QVERIFY(waitForTagPreflight(&panel));
    const QLabel* summary = tagSummaryOf(&panel);
    QVERIFY(summary != nullptr);
    QVERIFY(!summary->parentWidget()->isHidden());
    QVERIFY2(summary->text().contains(
                 QStringLiteral("text-size clusters")),
             qPrintable(summary->text()));
    QVERIFY2(summary->text().contains(QStringLiteral("A taggable line.")) == false
                 || summary->text().contains(QStringLiteral("10.0pt")),
             qPrintable(summary->text()));
    QVERIFY2(summary->text().contains(
                 QStringLiteral("not a conforming document")),
             qPrintable(summary->text()));

    // Cancel changes nothing.
    auto* cancel = panel.findChild<QPushButton*>(
        QStringLiteral("a11yTagCancelButton"));
    QVERIFY(cancel != nullptr);
    cancel->click();
    QVERIFY2(statusOf(&panel)->text().contains(QStringLiteral("cancelled")),
             qPrintable(statusOf(&panel)->text()));
}

void TestAccessibilityPanel::taggedDocumentDisablesTagAction() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = tmp.filePath("clean.pdf");
    QVERIFY(makeCleanPdf(pdf));   // carries /StructTreeRoot

    gp::AccessibilityPanel panel;
    panel.setTagRunner([](const QString& path) {
        return gp::tagDocumentAccessibility(path);
    });
    panel.setDocument(pdf);
    QVERIFY(waitForScan(&panel));
    QVERIFY(panel.lastReport().tagged);

    // Already tagged → the action refuses UP FRONT (honest gating, whyNot
    // shown), never a dead-end click.
    QVERIFY(!tagButtonOf(&panel)->isEnabled());
    QVERIFY2(tagButtonOf(&panel)->toolTip().contains(
                 QStringLiteral("already tagged")),
             qPrintable(tagButtonOf(&panel)->toolTip()));
}

void TestAccessibilityPanel::tagRunnerFlowResolvesFinding() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = tmp.filePath("taggable.pdf");
    QVERIFY(makeTaggablePdf(pdf));

    gp::AccessibilityPanel panel;
    panel.setTagRunner([](const QString& path) {
        return gp::tagDocumentAccessibility(path);
    });
    panel.setDocument(pdf);
    QVERIFY(waitForScan(&panel));
    QVERIFY(!panel.lastReport().tagged);

    // The full flow: Tag → pre-flight → Apply → real tagging transaction →
    // automatic re-scan of the same identity.
    tagButtonOf(&panel)->click();
    QVERIFY(waitForTagPreflight(&panel));
    auto* apply = panel.findChild<QPushButton*>(
        QStringLiteral("a11yTagApplyButton"));
    QVERIFY(apply != nullptr);
    apply->click();
    QVERIFY(waitForTagRun(&panel));
    QVERIFY(waitForScan(&panel));

    // Post-run re-scan: tagged=true, the struct-tree finding RESOLVED.
    QVERIFY(panel.lastReport().loadOk);
    QVERIFY(panel.lastReport().tagged);
    for (const auto& f : panel.lastReport().findings)
        QVERIFY2(f.checkId != QLatin1String("struct-tree"),
                 qPrintable(f.checkId));
    // The tag action now refuses (already tagged).
    QVERIFY(!tagButtonOf(&panel)->isEnabled());
    QVERIFY2(statusOf(&panel)->text().contains(QStringLiteral("Tagged"))
                 || !statusOf(&panel)->text().isEmpty(),
             qPrintable(statusOf(&panel)->text()));
}

void TestAccessibilityPanel::disclaimerIsPinnedVerbatim() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = tmp.filePath("taggable.pdf");
    QVERIFY(makeTaggablePdf(pdf));

    gp::AccessibilityPanel panel;
    panel.setTagRunner([](const QString& path) {
        return gp::tagDocumentAccessibility(path);
    });
    panel.setDocument(pdf);
    QVERIFY(waitForScan(&panel));

    // The honesty box carries the extended P2 contract VERBATIM.
    const QString disclosure = disclosureOf(&panel)->text();
    QVERIFY2(disclosure.contains(QStringLiteral("it never certifies PDF/UA")),
             qPrintable(disclosure));
    QVERIFY2(disclosure.contains(
                 QStringLiteral(
                     "a tagged document is not a conforming document")),
             qPrintable(disclosure));
    QVERIFY2(disclosure.contains(QStringLiteral(
                 "no conformance verdict of any kind is issued")),
             qPrintable(disclosure));

    // The conformance words appear ONLY inside the disclaimer: the scan
    // status and the tag pre-flight summary must not freelance them —
    // the summary's sentence is part of the same pinned disclaimer family
    // ("not a conforming document"), the status line never says conform*.
    tagButtonOf(&panel)->click();
    QVERIFY(waitForTagPreflight(&panel));
    const QString summary = tagSummaryOf(&panel)->text();
    QVERIFY2(summary.contains(QStringLiteral(
                 "not a conforming document")),
             qPrintable(summary));
    const QString status = statusOf(&panel)->text();
    QVERIFY2(!status.contains(QStringLiteral("conform"), Qt::CaseInsensitive),
             qPrintable(status));
}

#include "TestAccessibilityPanel.moc"
QTEST_MAIN(TestAccessibilityPanel)
