#include <QtTest>
#include <QApplication>
#include "core/AppContext.h"
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"
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
        QVERIFY(!tools.contains(ToolId::Open));
        QVERIFY(!tools.contains(ToolId::Encrypt));
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
};

QTEST_MAIN(TestControllers)
#include "TestControllers.moc"
