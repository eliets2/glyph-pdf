#include <QtTest>
#include <QApplication>
#include "core/AppContext.h"
#include "GpMainWindow.h"
#include "shell/controllers/HomeController.h"
#include "shell/controllers/ViewController.h"
#include "shell/controllers/EditController.h"
#include "shell/controllers/PagesController.h"
#include "shell/controllers/ConvertController.h"
#include "shell/controllers/FormsController.h"
#include "shell/controllers/SecurityController.h"

class TestControllers : public QObject {
    Q_OBJECT

private:
    AppContext m_ctx;
    std::unique_ptr<gp::MainWindow> m_mainWindow;

private slots:
    void initTestCase() {
        m_mainWindow = std::make_unique<gp::MainWindow>(&m_ctx);
        QVERIFY(m_mainWindow != nullptr);
    }

    void cleanupTestCase() {
        m_mainWindow.reset();
    }

    void testHomeControllerHandles() {
        gp::HomeController ctrl(&m_ctx, m_mainWindow.get());
        QVERIFY(ctrl.handles("open"));
        QVERIFY(ctrl.handles("save"));
        QVERIFY(ctrl.handles("saveAs"));
        QVERIFY(ctrl.handles("save-as"));
        QVERIFY(ctrl.handles("print"));
        QVERIFY(ctrl.handles("share"));
        QVERIFY(ctrl.handles("properties"));
        QVERIFY(!ctrl.handles("zoom-in"));
        QVERIFY(!ctrl.handles("encrypt"));
        QVERIFY(!ctrl.handles(""));
    }

    void testViewControllerHandles() {
        gp::ViewController ctrl(&m_ctx, m_mainWindow.get());
        QVERIFY(ctrl.handles("zoomIn"));
        QVERIFY(ctrl.handles("zoom-in"));
        QVERIFY(ctrl.handles("zoomOut"));
        QVERIFY(ctrl.handles("zoom-out"));
        QVERIFY(ctrl.handles("actual"));
        QVERIFY(ctrl.handles("actual-size"));
        QVERIFY(ctrl.handles("fitWidth"));
        QVERIFY(ctrl.handles("fit-width"));
        QVERIFY(ctrl.handles("fitPage"));
        QVERIFY(ctrl.handles("fit-page"));
        QVERIFY(ctrl.handles("single"));
        QVERIFY(ctrl.handles("single-page"));
        QVERIFY(ctrl.handles("continuous"));
        QVERIFY(ctrl.handles("two"));
        QVERIFY(ctrl.handles("two-page"));
        QVERIFY(ctrl.handles("presentation"));
        QVERIFY(ctrl.handles("fullscreen"));
        QVERIFY(ctrl.handles("darkMode"));
        QVERIFY(ctrl.handles("eyeCare"));
        QVERIFY(!ctrl.handles("open"));
        QVERIFY(!ctrl.handles("encrypt"));
    }

    void testEditControllerHandles() {
        gp::EditController ctrl(&m_ctx, m_mainWindow.get());
        QVERIFY(ctrl.handles("edit-text"));
        QVERIFY(ctrl.handles("highlight"));
        QVERIFY(ctrl.handles("underline"));
        QVERIFY(ctrl.handles("strikeout"));
        QVERIFY(ctrl.handles("note"));
        QVERIFY(ctrl.handles("pencil"));
        QVERIFY(ctrl.handles("freehand"));
        QVERIFY(ctrl.handles("line"));
        QVERIFY(ctrl.handles("arrow"));
        QVERIFY(ctrl.handles("rect"));
        QVERIFY(ctrl.handles("oval"));
        QVERIFY(ctrl.handles("squiggly"));
        QVERIFY(ctrl.handles("signature"));
        QVERIFY(ctrl.handles("image"));
        QVERIFY(!ctrl.handles("open"));
        QVERIFY(!ctrl.handles("encrypt"));
    }

    void testPagesControllerHandles() {
        gp::PagesController ctrl(&m_ctx, m_mainWindow.get());
        QVERIFY(ctrl.handles("rotate-cw"));
        QVERIFY(ctrl.handles("rotate-ccw"));
        QVERIFY(ctrl.handles("delete-page"));
        QVERIFY(ctrl.handles("insert-page"));
        QVERIFY(ctrl.handles("extract"));
        QVERIFY(ctrl.handles("reorder"));
        QVERIFY(!ctrl.handles("open"));
        QVERIFY(!ctrl.handles("zoom-in"));
    }

    void testConvertControllerHandles() {
        gp::ConvertController ctrl(&m_ctx, m_mainWindow.get());
        QVERIFY(ctrl.handles("to-word"));
        QVERIFY(ctrl.handles("to-excel"));
        QVERIFY(ctrl.handles("combine"));
        QVERIFY(ctrl.handles("compress"));
        QVERIFY(!ctrl.handles("open"));
        QVERIFY(!ctrl.handles("encrypt"));
    }

    void testFormsControllerHandles() {
        gp::FormsController ctrl(&m_ctx, m_mainWindow.get());
        QVERIFY(ctrl.handles("text-field"));
        QVERIFY(ctrl.handles("checkbox"));
        QVERIFY(ctrl.handles("radio"));
        QVERIFY(ctrl.handles("dropdown"));
        QVERIFY(!ctrl.handles("open"));
        QVERIFY(!ctrl.handles("zoom-in"));
    }

    void testSecurityControllerHandles() {
        gp::SecurityController ctrl(&m_ctx, m_mainWindow.get());
        QVERIFY(ctrl.handles("encrypt"));
        QVERIFY(ctrl.handles("sign"));
        QVERIFY(ctrl.handles("sanitize"));
        QVERIFY(ctrl.handles("applyRedact"));
        QVERIFY(ctrl.handles("certify"));
        QVERIFY(ctrl.handles("timestamp"));
        QVERIFY(ctrl.handles("permissions"));
        QVERIFY(!ctrl.handles("open"));
        QVERIFY(!ctrl.handles("zoom-in"));
    }

    void testNoOverlappingToolIds() {
        gp::HomeController home(&m_ctx, m_mainWindow.get());
        gp::ViewController view(&m_ctx, m_mainWindow.get());
        gp::EditController edit(&m_ctx, m_mainWindow.get());
        gp::PagesController pages(&m_ctx, m_mainWindow.get());
        gp::ConvertController convert(&m_ctx, m_mainWindow.get());
        gp::FormsController forms(&m_ctx, m_mainWindow.get());
        gp::SecurityController security(&m_ctx, m_mainWindow.get());

        QStringList allIds = {
            "open", "save", "saveAs", "save-as", "print", "share", "properties",
            "zoomIn", "zoom-in", "zoomOut", "zoom-out", "actual", "actual-size",
            "fitWidth", "fit-width", "fitPage", "fit-page", "single", "single-page",
            "continuous", "two", "two-page", "presentation", "fullscreen", "darkMode", "eyeCare",
            "edit-text", "highlight", "underline", "strikeout", "note", "pencil",
            "freehand", "line", "arrow", "rect", "oval", "squiggly", "image",
            "rotate-cw", "rotate-ccw", "delete-page", "insert-page", "extract", "reorder",
            "to-word", "to-excel", "combine", "compress",
            "text-field", "checkbox", "radio", "dropdown",
            "encrypt", "sign", "sanitize", "applyRedact", "certify", "timestamp", "permissions"
        };

        using Handler = std::function<bool(const QString&)>;
        QVector<Handler> controllers = {
            [&](const QString& id) { return home.handles(id); },
            [&](const QString& id) { return view.handles(id); },
            [&](const QString& id) { return edit.handles(id); },
            [&](const QString& id) { return pages.handles(id); },
            [&](const QString& id) { return convert.handles(id); },
            [&](const QString& id) { return forms.handles(id); },
            [&](const QString& id) { return security.handles(id); },
        };

        for (const auto& id : allIds) {
            int handlerCount = 0;
            for (const auto& ctrl : controllers) {
                if (ctrl(id)) ++handlerCount;
            }
            QVERIFY2(handlerCount == 1,
                qPrintable(QString("Tool ID '%1' handled by %2 controllers (expected 1)").arg(id).arg(handlerCount)));
        }
    }

    void testUnknownToolIdHandledByNone() {
        gp::HomeController home(&m_ctx, m_mainWindow.get());
        gp::ViewController view(&m_ctx, m_mainWindow.get());
        gp::EditController edit(&m_ctx, m_mainWindow.get());
        gp::PagesController pages(&m_ctx, m_mainWindow.get());
        gp::ConvertController convert(&m_ctx, m_mainWindow.get());
        gp::FormsController forms(&m_ctx, m_mainWindow.get());
        gp::SecurityController security(&m_ctx, m_mainWindow.get());

        QStringList bogusIds = {"nonexistent", "foo-bar", "", "💀"};
        for (const auto& id : bogusIds) {
            QVERIFY(!home.handles(id));
            QVERIFY(!view.handles(id));
            QVERIFY(!edit.handles(id));
            QVERIFY(!pages.handles(id));
            QVERIFY(!convert.handles(id));
            QVERIFY(!forms.handles(id));
            QVERIFY(!security.handles(id));
        }
    }
};

QTEST_MAIN(TestControllers)
#include "TestControllers.moc"
