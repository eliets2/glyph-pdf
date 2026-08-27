// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"
#include "ui/ExportPresetsPanel.h" // Preset (§9.16 export-preset plan seam)

struct AppContext;
class PdfViewerWidget;

namespace gp {

class MainWindow;

class HomeController : public QObject, public IToolController {
    Q_OBJECT
public:
    HomeController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;

    void addRecentFile(const QString& filePath);
    void removeFromRecents(const QString& filePath);
    int  pruneMissingRecents();
    QStringList recentFiles() const;

    // §9.16 test seam: map an export preset onto the concrete post-processing
    // steps the export flow will run (linearize → qpdf-backed
    // linearizeDocument; PDF/A → exportPdfA). Pure function so the preset
    // execution path can be tested without a document or dialogs.
    struct ExportPlan {
        bool linearize = false;
        int  pdfALevel = 0;   // 0 = no PDF/A step; else 1/2/3 (b-conformance)
    };
    static ExportPlan planForExport(const ExportPresetsPanel::Preset& p);

private:
    void onSave();
    void onSaveAs();
    void onShare();
    void shareViaEmail(const QString& filePath);
    void createEncryptedPackage(const QString& filePath);
    void onPrint();
    void onPrintPreview();
    void onPageSetup();
    void onExportPresets();
    void showProperties();
    void onImportOffice();
    void onImagesToPdf();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
};

} // namespace gp
