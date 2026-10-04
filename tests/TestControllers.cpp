#include <QtTest>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include "core/AppContext.h"
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"
#include "engines/SafeSave.h"
#include "engines/SevenZipLocator.h"
#include "shell/controllers/HomeController.h"
#include "shell/controllers/ViewController.h"
#include "shell/controllers/EditController.h"
#include "shell/controllers/PagesController.h"
#include "shell/controllers/ConvertController.h"
#include "shell/controllers/FormsController.h"
#include "shell/controllers/SecurityController.h"
#include "shell/ToolRegistry.h"

class TestControllers : public QObject {
    Q_OBJECT

private:
    AppContext m_ctx;

private slots:
    void initTestCase() {
        // No need to instantiate the full GpMainWindow in offscreen/headless test environment.
        // Passing nullptr to controllers is safe as they only use MainWindow pointer during active tool invocation,
        // not during construction or handledTools() queries.
    }

    void cleanupTestCase() {
    }

    void testHomeControllerHandles() {
        gp::HomeController ctrl(&m_ctx, nullptr);
        const auto tools = ctrl.handledTools();
        QVERIFY(tools.contains(ToolId::Open));
        QVERIFY(tools.contains(ToolId::Save));
        QVERIFY(tools.contains(ToolId::SaveAs));
        QVERIFY(tools.contains(ToolId::Print));
        QVERIFY(tools.contains(ToolId::Share));
        QVERIFY(tools.contains(ToolId::Properties));
        QVERIFY(!tools.contains(ToolId::ZoomIn));
        QVERIFY(!tools.contains(ToolId::Encrypt));
    }

    void testViewControllerHandles() {
        gp::ViewController ctrl(&m_ctx, nullptr);
        const auto tools = ctrl.handledTools();
        QVERIFY(tools.contains(ToolId::ZoomIn));
        QVERIFY(tools.contains(ToolId::ZoomOut));
        QVERIFY(tools.contains(ToolId::ActualSize));
        QVERIFY(tools.contains(ToolId::FitWidth));
        QVERIFY(tools.contains(ToolId::FitPage));
        QVERIFY(tools.contains(ToolId::SinglePage));
        QVERIFY(tools.contains(ToolId::Continuous));
        QVERIFY(tools.contains(ToolId::TwoPage));
        QVERIFY(tools.contains(ToolId::Presentation));
        QVERIFY(tools.contains(ToolId::Fullscreen));
        QVERIFY(tools.contains(ToolId::DarkMode));
        QVERIFY(tools.contains(ToolId::EyeCare));
        QVERIFY(tools.contains(ToolId::NightMode));
        QVERIFY(!tools.contains(ToolId::Open));
        QVERIFY(!tools.contains(ToolId::Encrypt));
    }

    void testEditControllerHandles() {
        gp::EditController ctrl(&m_ctx, nullptr);
        const auto tools = ctrl.handledTools();
        QVERIFY(tools.contains(ToolId::EditText));
        QVERIFY(tools.contains(ToolId::Highlight));
        QVERIFY(tools.contains(ToolId::Underline));
        QVERIFY(tools.contains(ToolId::Strikeout));
        QVERIFY(tools.contains(ToolId::Note));
        QVERIFY(tools.contains(ToolId::Pencil));
        QVERIFY(tools.contains(ToolId::Freehand));
        QVERIFY(tools.contains(ToolId::Line));
        QVERIFY(tools.contains(ToolId::Arrow));
        QVERIFY(tools.contains(ToolId::Rectangle));
        QVERIFY(tools.contains(ToolId::Oval));
        QVERIFY(tools.contains(ToolId::Squiggly));
        QVERIFY(tools.contains(ToolId::Signature));
        QVERIFY(tools.contains(ToolId::Image));
        // §9.2 P0: minimal clipboard editing is wired (Cut/Copy/Delete).
        QVERIFY(tools.contains(ToolId::Cut));
        QVERIFY(tools.contains(ToolId::Copy));
        QVERIFY(tools.contains(ToolId::DeleteSelection));
        QVERIFY(!tools.contains(ToolId::Open));
        QVERIFY(!tools.contains(ToolId::Encrypt));
        // Menu aliases resolve to the new ids.
        QCOMPARE(toolIdFromString(QStringLiteral("copy")).value_or(ToolId::Open), ToolId::Copy);
        QCOMPARE(toolIdFromString(QStringLiteral("delete")).value_or(ToolId::Open), ToolId::DeleteSelection);
    }

    void testPagesControllerHandles() {
        gp::PagesController ctrl(&m_ctx, nullptr);
        const auto tools = ctrl.handledTools();
        QVERIFY(tools.contains(ToolId::RotateCW));
        QVERIFY(tools.contains(ToolId::RotateCCW));
        QVERIFY(tools.contains(ToolId::DeletePage));
        QVERIFY(tools.contains(ToolId::InsertPage));
        QVERIFY(tools.contains(ToolId::Extract));
        QVERIFY(tools.contains(ToolId::Reorder));
        QVERIFY(!tools.contains(ToolId::Open));
        QVERIFY(!tools.contains(ToolId::ZoomIn));
    }

    void testConvertControllerHandles() {
        gp::ConvertController ctrl(&m_ctx, nullptr);
        const auto tools = ctrl.handledTools();
        QVERIFY(tools.contains(ToolId::ToWord));
        QVERIFY(tools.contains(ToolId::ToExcel));
        QVERIFY(tools.contains(ToolId::Combine));
        QVERIFY(tools.contains(ToolId::Compress));
        QVERIFY(!tools.contains(ToolId::Open));
        QVERIFY(!tools.contains(ToolId::Encrypt));
    }

    void testFormsControllerHandles() {
        gp::FormsController ctrl(&m_ctx, nullptr);
        const auto tools = ctrl.handledTools();
        QVERIFY(tools.contains(ToolId::TextField));
        QVERIFY(tools.contains(ToolId::Checkbox));
        QVERIFY(tools.contains(ToolId::Radio));
        QVERIFY(tools.contains(ToolId::Dropdown));
        // §9.6 P0: CalcField must be routed through FormsController (was
        // hard-disabled in MenuBar despite working end-to-end via canvas).
        QVERIFY(tools.contains(ToolId::CalcField));
        QVERIFY(!tools.contains(ToolId::Open));
        QVERIFY(!tools.contains(ToolId::ZoomIn));
    }

    void testSecurityControllerHandles() {
        gp::SecurityController ctrl(&m_ctx, nullptr);
        const auto tools = ctrl.handledTools();
        QVERIFY(tools.contains(ToolId::Encrypt));
        QVERIFY(tools.contains(ToolId::Sign));
        QVERIFY(tools.contains(ToolId::Sanitize));
        QVERIFY(tools.contains(ToolId::ApplyRedact));
        QVERIFY(tools.contains(ToolId::Certify));
        QVERIFY(tools.contains(ToolId::Timestamp));
        QVERIFY(tools.contains(ToolId::Permissions));
        // §9.11 P0: expiry must be settable from the UI again.
        QVERIFY(tools.contains(ToolId::ExpiryDate));
        QVERIFY(!tools.contains(ToolId::Open));
        QVERIFY(!tools.contains(ToolId::ZoomIn));
    }

    void testNoOverlappingToolIds() {
        gp::HomeController home(&m_ctx, nullptr);
        gp::ViewController view(&m_ctx, nullptr);
        gp::EditController edit(&m_ctx, nullptr);
        gp::PagesController pages(&m_ctx, nullptr);
        gp::ConvertController convert(&m_ctx, nullptr);
        gp::FormsController forms(&m_ctx, nullptr);
        gp::SecurityController security(&m_ctx, nullptr);

        QVector<IToolController*> controllers = {
            &home, &view, &edit, &pages, &convert, &forms, &security
        };

        for (int i = 0; i < static_cast<int>(ToolId::COUNT); ++i) {
            ToolId id = static_cast<ToolId>(i);
            int handlerCount = 0;
            for (auto* ctrl : controllers) {
                if (ctrl->handledTools().contains(id)) {
                    ++handlerCount;
                }
            }
            QVERIFY2(handlerCount <= 1,
                qPrintable(QString("Tool ID '%1' handled by %2 controllers (expected at most 1)").arg(toolIdToString(id)).arg(handlerCount)));
        }
    }

    void testUnknownToolIdHandledByNone() {
        gp::HomeController home(&m_ctx, nullptr);
        gp::ViewController view(&m_ctx, nullptr);
        gp::EditController edit(&m_ctx, nullptr);
        gp::PagesController pages(&m_ctx, nullptr);
        gp::ConvertController convert(&m_ctx, nullptr);
        gp::FormsController forms(&m_ctx, nullptr);
        gp::SecurityController security(&m_ctx, nullptr);

        QVector<IToolController*> controllers = {
            &home, &view, &edit, &pages, &convert, &forms, &security
        };

        QStringList bogusIds = {"nonexistent", "foo-bar", "", "💀"};
        for (const auto& str : bogusIds) {
            auto opt = toolIdFromString(str);
            QVERIFY(!opt.has_value());
        }
    }

    void testToolRegistryRegistrationAndDispatch() {
        gp::ToolRegistry registry;
        gp::HomeController home(&m_ctx, nullptr);

        registry.registerController(&home);
        QCOMPARE(registry.controllerFor(ToolId::Open), &home);

        // String-based lookup with standard aliases
        QVERIFY(isValidToolIdString("save-as"));
        auto parsed = toolIdFromString("save-as");
        QVERIFY(parsed.has_value());
        QCOMPARE(parsed.value(), ToolId::SaveAs);
    }

    void testExpiryDateToolIdWiring() {
        // §9.11 P0: the ExpiryDate tool id must resolve from its menu alias.
        auto parsed = toolIdFromString("expiry-date");
        QVERIFY(parsed.has_value());
        QCOMPARE(parsed.value(), ToolId::ExpiryDate);
        QCOMPARE(toolIdToString(ToolId::ExpiryDate), QStringLiteral("expiryDate"));
    }

    // ── M-2 (AUDIT-SECURITY-2026-09-25, CWE-93): mailto header injection ──
    //
    // The non-MAPI share fallback interpolates the document FILENAME (fully
    // attacker-influenceable document data) into a mailto query unencoded:
    // a file named "q1 report&bcc=attacker@evil.example.pdf" produced
    // "mailto:?subject=...q1 report&bcc=attacker@evil.example..." — the mail
    // client parsed a hidden BCC to the attacker. A CR/LF payload
    // ("%0d%0aBcc: attacker@evil.example") injected headers the same way.
    // The composed URL must carry exactly one raw query separator and no
    // header-significant byte inside either interpolated value.
    void testShareEmailUrlEncodesHeaderSignificantChars() {
        // (1) The audit's attack shape: raw CR/LF BCC injection.
        {
            const QString crlfSubject =
                QStringLiteral("PDF Document: q.pdf%0d%0aBcc: attacker@evil.example");
            const QString url = gp::HomeController::shareEmailUrl(
                crlfSubject, QStringLiteral("Please find the attached PDF document."));
            QVERIFY2(!url.contains(QLatin1Char('\r')) && !url.contains(QLatin1Char('\n')),
                     "CRLF must be percent-encoded, not injected as a header break");
            QCOMPARE(url.count(QLatin1Char('&')), 1);   // only the subject/body separator
            QVERIFY2(!url.contains(QStringLiteral("bcc="), Qt::CaseInsensitive),
                     "no raw bcc= header may survive encoding");
            QVERIFY(url.startsWith(QStringLiteral("mailto:?subject=")));
            QVERIFY(url.contains(QStringLiteral("&body=")));
        }

        // (2) The audit's second shape: raw '&' query-smuggling filename.
        {
            const QString hostileSubject = QStringLiteral("PDF Document: q1 report&bcc=attacker@evil.example.pdf");
            const QString url = gp::HomeController::shareEmailUrl(
                hostileSubject, QStringLiteral("body"));
            QCOMPARE(url.count(QLatin1Char('&')), 1);
            QVERIFY2(!url.contains(QStringLiteral("bcc="), Qt::CaseInsensitive),
                     "a hostile filename must not add a query parameter");
        }

        // (3) Benign semantics survive: unreserved characters stay readable.
        {
            const QString url = gp::HomeController::shareEmailUrl(
                QStringLiteral("PDF Document: report.pdf"),
                QStringLiteral("Please find the attached PDF document."));
            QVERIFY(url.startsWith(QStringLiteral("mailto:?subject=PDF%20Document%3A%20report.pdf")));
            QVERIFY(url.contains(QStringLiteral("&body=Please%20find")));
        }
    }

    // ── M-1 (AUDIT-SECURITY-2026-09-25, CWE-214): package password off argv ──
    //
    // The AES-256 ZIP package password used to travel as `-p<password>` on the
    // 7-Zip command line — readable by any same-user process (and
    // /proc/<pid>/cmdline on Linux) for the whole bounded run. The password
    // now rides the stdin pipe: the shipped 7-Zip prompts for it (verified
    // against 26.02, re-verified against 26.03 — bare
    // `-p` on create; NO -p switch on the read-back — a bare `-p` on `t`
    // parses as an EMPTY password there) and SafeSave::runBoundedProcess
    // delivers the reply and closes the channel.
    void testEncryptedPackageArgsCarryNoPassword() {
        const QString pw = QStringLiteral("S3cret-Passw0rd");
        const QString cand = QStringLiteral("C:/tmp/glyphpdf-candidate.zip");
        const QString doc = QStringLiteral("C:/docs/plan.pdf");

        // (a) argv shape: bare `-p`, password appears NOWHERE in argv.
        const QStringList createArgs =
            gp::HomeController::encryptedPackageCreateArgs(cand, doc);
        QVERIFY2(createArgs.contains(QStringLiteral("-p")),
                 "create must use the bare -p prompt switch");
        QCOMPARE(createArgs.size(), 6);
        for (const QString& a : createArgs) {
            QVERIFY2(!a.contains(pw),
                     qPrintable(QString("argv element leaks the password: %1").arg(a)));
        }

        // (b) the read-back argv carries NO -p switch at all.
        const QStringList validateArgs =
            gp::HomeController::encryptedPackageValidateArgs(cand);
        QCOMPARE(validateArgs.size(), 2);
        for (const QString& a : validateArgs) {
            QVERIFY2(!a.startsWith(QLatin1String("-p")),
                     qPrintable(QString("read-back must not pass -p (empty-password parse): %1").arg(a)));
            QVERIFY2(!a.contains(pw),
                     qPrintable(QString("argv element leaks the password: %1").arg(a)));
        }

        // (c) live end-to-end through the REAL 7-Zip + stdin path (QSKIP only
        // when the vendored bundle is absent): the archive must be genuinely
        // encrypted with the stdin-delivered password and unreadable without
        // it. Resolution via the app-owned locator — the bundled copy staged
        // beside this test binary is the ONLY legitimate resolution (F-02:
        // no PATH/system-install fallback exists any more).
        const QString sevenZip = gp::SevenZipLocator::locate();
        if (sevenZip.isEmpty())
            QSKIP("no vendored 7-Zip bundle beside the test binary — argv-shape assertions above still ran");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString payloadPath = dir.filePath(QStringLiteral("doc.txt"));
        {
            QFile f(payloadPath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QVERIFY(f.write("encrypted package payload") > 0);
        }
        const QString archive = dir.filePath(QStringLiteral("pkg.zip"));

        bool canceled = false;
        int exitCode = -1;
        QString err;
        const QByteArray pwStdin = pw.toUtf8() + '\n';

        // create — password on stdin only
        QVERIFY2(gp::SafeSave::runBoundedProcess(
                     sevenZip,
                     gp::HomeController::encryptedPackageCreateArgs(archive, payloadPath),
                     60000, {}, &canceled, &exitCode, &err, pwStdin),
                 qPrintable(QStringLiteral("7z create via stdin password failed: %1").arg(err)));
        QCOMPARE(exitCode, 0);
        QVERIFY(QFileInfo::exists(archive));

        // read-back with the stdin password must pass
        exitCode = -1;
        QVERIFY(gp::SafeSave::runBoundedProcess(
                    sevenZip, gp::HomeController::encryptedPackageValidateArgs(archive),
                    60000, {}, &canceled, &exitCode, &err, pwStdin));
        QCOMPARE(exitCode, 0);

        // negative: a WRONG password on argv must fail (archive really encrypted).
        // Either a clean nonzero exit or the bounded kill of a hung prompt
        // both count as "did not open".
        exitCode = -1;
        {
            const bool finished = gp::SafeSave::runBoundedProcess(
                sevenZip,
                QStringList{ QStringLiteral("t"), QStringLiteral("-pWRONG"),
                             QDir::toNativeSeparators(archive) },
                15000, {}, &canceled, &exitCode, &err);
            QVERIFY2(!finished || exitCode != 0,
                     "a wrong password must not open the archive");
        }

        // negative: NO password at all must fail (nothing is on argv to leak).
        // 7z prompts and blocks on the open write channel, so the bounded run
        // kills it at the deadline — that is a fail-closed outcome too.
        exitCode = -1;
        {
            const bool finished = gp::SafeSave::runBoundedProcess(
                sevenZip, gp::HomeController::encryptedPackageValidateArgs(archive),
                15000, {}, &canceled, &exitCode, &err);
            QVERIFY2(!finished || exitCode != 0,
                     "the archive must not open without the stdin password");
        }
    }
};

QTEST_MAIN(TestControllers)
#include "TestControllers.moc"
