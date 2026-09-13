// SPDX-License-Identifier: Apache-2.0
// T2-6: dynamic stamps + stamp library management.
//
// Pins:
//   * the five built-in templates the research row names and their ids,
//   * APPLY-TIME placeholder substitution (${author}/${date}/${time}/
//     ${datetime}) — deterministic fixed clock, exact strings, empty author
//     honesty ("Unknown"), unknown placeholders left verbatim,
//   * custom stamp persistence (real file written + reloaded; built-in ids
//     cannot be shadowed by a tampered file),
//   * the SAVED annotation carries the RESOLVED text (embedAnnotations →
//     extractAnnotations roundtrip through a real PDF),
//   * the placement gesture: the resolved text rides the pending-stamp seam
//     into the committed AnnotationItem (plain click gets a visible default
//     box and the pending text is consumed after one placement),
//   * the previously silent Stamps menu ids are now registry commands
//     (one handler, EditController).

#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QStandardPaths>
#include <podofo/podofo.h>

#include "core/StampLibrary.h"
#include "core/ToolId.h"
#include "ui/StampLibraryDialog.h"
#include "ui/AnnotationLayer.h"
#include "engines/podofo/PoDoFoBackend.h"
#include "shell/controllers/EditController.h"

#ifdef DrawText
#undef DrawText
#endif

class TestDynamicStamps : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Temp directory creation failed");
        // Redirect AppLocalDataLocation so the dialog's custom-stamp
        // persistence stays in a temp tree (deterministic, no user-dir writes).
        QStandardPaths::setTestModeEnabled(true);
    }

    // ── built-in library ─────────────────────────────────────────────────

    void builtInLibraryMatchesResearchRow() {
        const auto builtins = StampLibrary::builtIns();
        QCOMPARE(builtins.size(), 5);
        QList<QString> names;
        for (const auto& t : builtins) names << t.name;
        QVERIFY(names.contains(QStringLiteral("Approved")));
        QVERIFY(names.contains(QStringLiteral("Draft")));
        QVERIFY(names.contains(QStringLiteral("Confidential")));
        QVERIFY(names.contains(QStringLiteral("Received")));
        QVERIFY(names.contains(QStringLiteral("Reviewed")));
        QVERIFY(StampLibrary::findById(QStringLiteral("builtin:approved")).has_value());
        QVERIFY(!StampLibrary::findById(QStringLiteral("builtin:nonexistent")).has_value());
    }

    // ── apply-time substitution (deterministic clock) ────────────────────

    void resolveTextSubstitutesAtApplyTime() {
        const QDateTime when(QDate(2026, 9, 9), QTime(14, 30));
        QCOMPARE(StampLibrary::resolveText(QStringLiteral("Received | ${author} | ${datetime}"),
                                           QStringLiteral("Alice"), when),
                 QStringLiteral("Received | Alice | 2026-09-09 14:30"));
        QCOMPARE(StampLibrary::resolveText(QStringLiteral("Reviewed ${author} ${date} ${time}"),
                                           QStringLiteral("Bob"), when),
                 QStringLiteral("Reviewed Bob 2026-09-09 14:30"));
        QCOMPARE(StampLibrary::resolveText(QStringLiteral("Approved | ${author} | ${date}"),
                                           QStringLiteral("  Carol  "), when),
                 QStringLiteral("Approved | Carol | 2026-09-09"));
    }

    void resolveTextHonestyRules() {
        const QDateTime when(QDate(2026, 9, 9), QTime(9, 5));
        // Empty author must not silently produce "Received |  | ..." — the
        // placeholder resolves to a visible "Unknown".
        QCOMPARE(StampLibrary::resolveText(QStringLiteral("Received | ${author}"),
                                           QString(), when),
                 QStringLiteral("Received | Unknown"));
        // Unknown placeholders stay VERBATIM (never silently dropped).
        QCOMPARE(StampLibrary::resolveText(QStringLiteral("St ${batch}"), QStringLiteral("A"), when),
                 QStringLiteral("St ${batch}"));
        // Plain template (Confidential) resolves to itself.
        QCOMPARE(StampLibrary::resolveText(QStringLiteral("CONFIDENTIAL"), QString(), when),
                 QStringLiteral("CONFIDENTIAL"));
    }

    // ── custom stamp persistence (real file, reloaded) ───────────────────

    void customStampsPersistAndReload() {
        const QString path = m_tmpDir.filePath(QStringLiteral("stamps.json"));
        QList<StampTemplate> stamps;
        stamps.append({ QStringLiteral("custom:abc"), QStringLiteral("Escrow Release"),
                        QStringLiteral("Escrow | ${author} | ${date}"), QColor(0x00, 0x66, 0x00) });
        stamps.append({ QStringLiteral("custom:def"), QStringLiteral("Paid"),
                        QStringLiteral("PAID ${datetime}"), QColor(0x66, 0x00, 0x00) });
        QVERIFY2(StampLibrary::saveCustomTo(path, stamps),
                 "saveCustomTo must write the JSON file");

        const QList<StampTemplate> loaded = StampLibrary::loadCustomFrom(path);
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded.first().id, QStringLiteral("custom:abc"));
        QCOMPARE(loaded.first().name, QStringLiteral("Escrow Release"));
        QCOMPARE(loaded.last().textTemplate, QStringLiteral("PAID ${datetime}"));
        QCOMPARE(loaded.first().color, QColor(0x00, 0x66, 0x00));

        // A tampered file must not shadow built-in ids.
        QFile bad(m_tmpDir.filePath(QStringLiteral("bad.json")));
        QVERIFY(bad.open(QIODevice::WriteOnly | QIODevice::Truncate));
        bad.write("{\"stamps\":[{\"id\":\"builtin:approved\",\"name\":\"FAKE\","
                  "\"template\":\"FAKE\"}]}");
        bad.close();
        QVERIFY(StampLibrary::loadCustomFrom(m_tmpDir.filePath(QStringLiteral("bad.json"))).isEmpty());
    }

    // ── the SAVED annotation carries the RESOLVED text ───────────────────

    void savedAnnotationCarriesResolvedText() {
        // Create a base single-page PDF (same helper pattern as
        // TestAnnotationDjot).
        const QString base = m_tmpDir.filePath(QStringLiteral("stamp_base.pdf"));
        try {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            doc.Save(base.toUtf8().constData());
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QStringLiteral("base pdf failed: %1").arg(e.what())));
        }

        const QDateTime when(QDate(2026, 9, 9), QTime(14, 30));
        const QString resolved = StampLibrary::resolveText(
            StampLibrary::findById(QStringLiteral("builtin:received"))->textTemplate,
            QStringLiteral("Alice"), when);

        AnnotationItem stamp;
        stamp.id = QStringLiteral("st-1");
        stamp.mode = ToolMode::Stamp;
        stamp.pageIndex = 0;
        stamp.rect = QRectF(200, 200, 180, 44);
        stamp.text = resolved;              // <- substituted AT APPLY TIME
        stamp.reviewState = ReviewState::None;

        // REAL engine roundtrip: the persisted annotation dictionary must
        // carry the resolved text (never a template).
        PoDoFoBackend backend;
        const QString out = m_tmpDir.filePath(QStringLiteral("stamp_out.pdf"));
        QVERIFY(backend.embedAnnotations(base, out, { stamp }));
        const QList<AnnotationItem> loaded = backend.extractAnnotations(out);
        bool found = false;
        for (const auto& a : loaded) {
            if (a.id == QStringLiteral("st-1")) {
                found = true;
                QVERIFY2(a.text.contains(QStringLiteral("Alice")),
                         qPrintable(QStringLiteral("saved text must contain the resolved author, got: ")
                                    + a.text));
                QVERIFY2(a.text.contains(QStringLiteral("2026-09-09 14:30")),
                         qPrintable(QStringLiteral("saved text must contain the applied datetime, got: ")
                                    + a.text));
                QCOMPARE(a.mode, ToolMode::Stamp);
            }
        }
        QVERIFY2(found, "the placed stamp must reload from the saved PDF");
    }

    // ── the placement gesture consumes the pending resolved text ─────────

    void placementCarriesPendingTextAndConsumesIt() {
        AnnotationLayer layer;
        layer.resize(400, 400);
        layer.setPendingStampText(QStringLiteral("Received | Alice | 2026-09-09 14:30"));
        layer.setMode(ToolMode::Stamp);   // arms Stamp (keeps pending text)
        QCOMPARE(layer.pendingStampText(), QStringLiteral("Received | Alice | 2026-09-09 14:30"));

        // A plain click places the stamp with the resolved text.
        QTest::mouseClick(&layer, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
        QCOMPARE(layer.annotations().size(), 1);
        const AnnotationItem& placed = layer.annotations().first();
        QCOMPARE(placed.mode, ToolMode::Stamp);
        QCOMPARE(placed.text, QStringLiteral("Received | Alice | 2026-09-09 14:30"));
        // Plain click → visible default box, not a zero-sized rect.
        QVERIFY2(placed.rect.width() > 50 && placed.rect.height() > 20,
                 "a plain click must still place a visible stamp box");

        // The resolution is CONSUMED after one placement — a second click is
        // a bare stamp, never a stale reuse of the first placement's text.
        QCOMPARE(layer.pendingStampText(), QString());
        QTest::mouseClick(&layer, Qt::LeftButton, Qt::NoModifier, QPoint(150, 150));
        QCOMPARE(layer.annotations().size(), 2);
        QVERIFY2(layer.annotations().last().text.isEmpty(),
                 "the second placement must not inherit the first stamp's text");

        // Arming another tool clears any pending stamp text.
        layer.setPendingStampText(QStringLiteral("Approved | Bob | 2026-09-09"));
        layer.setMode(ToolMode::Highlight);
        QCOMPARE(layer.pendingStampText(), QString());
    }

    // ── the library dialog: list, add, delete, place signal ──────────────

    void dialogManagesLibraryAndEmitsPlace() {
        StampLibraryDialog dlg;
        dlg.reload();

        auto* list = dlg.findChild<QListWidget*>(QStringLiteral("stampList"));
        QVERIFY(list);
        // Five built-ins listed (custom storage untouched in this env).
        QVERIFY2(list->count() >= 5,
                 qPrintable(QString("expected >= 5 stamps, got %1").arg(list->count())));

        // The previously-dead "Custom Stamp…" wire target: adding a custom
        // stamp persists it (via StampLibrary's AppData default) and lists it.
        QVERIFY(dlg.addCustomStamp(QStringLiteral("Escrow Release"),
                                   QStringLiteral("Escrow | ${author}")));
        QVERIFY2(dlg.selectedTemplateId().startsWith(QStringLiteral("custom:")),
                 qPrintable(dlg.selectedTemplateId()));
        QVERIFY(list->count() >= 6);

        // Place emits the request through ONE stamp flow (the same
        // armDynamicStamp path the Stamps menu items take).
        QSignalSpy spy(&dlg, &StampLibraryDialog::placeRequested);
        auto* placeBtn = dlg.findChild<QPushButton*>(QStringLiteral("stampPlaceButton"));
        QVERIFY(placeBtn);
        QTest::mouseClick(placeBtn, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), dlg.selectedTemplateId());

        // The dialog discloses the substitution contract.
        QVERIFY2(dlg.findChild<QLabel*>(QStringLiteral("stampDisclosure")) != nullptr,
                 "the placeholder disclosure label must exist");
    }

    // ── registry wiring of the previously silent menu items ──────────────

    void stampsMenuIdsAreRegistryCommands() {
        // The MenuBar Stamps entries route through these ids (aliases kept
        // for the legacy "custom-stamp" ribbon/menu id).
        const auto approved = toolIdFromString(QStringLiteral("stamp-approved"));
        QVERIFY(approved.has_value());
        QCOMPARE(*approved, ToolId::StampApproved);
        QCOMPARE(toolIdFromString(QStringLiteral("custom-stamp")),
                 std::optional<ToolId>(ToolId::StampLibraryManage));

        // One handler only: EditController (checked exhaustively by
        // TestControllers; here the pure mapping seam is pinned).
        QCOMPARE(gp::EditController::stampTemplateIdForTool(ToolId::StampApproved),
                 QStringLiteral("builtin:approved"));
        QCOMPARE(gp::EditController::stampTemplateIdForTool(ToolId::StampReviewed),
                 QStringLiteral("builtin:reviewed"));
        QVERIFY(gp::EditController::stampTemplateIdForTool(ToolId::Stamp).isEmpty());
    }
};

QTEST_MAIN(TestDynamicStamps)
#include "TestDynamicStamps.moc"
