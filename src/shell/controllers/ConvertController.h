// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>
#include "core/ToolId.h"
#include "core/Capability.h"
#include "core/interfaces/IToolController.h"

struct AppContext;

namespace gp {

class MainWindow;

class ConvertController : public QObject, public IToolController {
    Q_OBJECT
public:
    ConvertController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;

    // §9.16 P0: surfaced on export/import completions — every conversion
    // runs on-device (in-process engines or a local LibreOffice/qpdf
    // subprocess), so the privacy claim is factual, not marketing.
    static QString localProcessingNotice();

private:
    void exportToWord();
    void exportToExcel();
    void exportToCsv();
    void exportToHtml();
    void exportToText();
    void exportToPowerPoint();
    void exportToImage();
    void openCompressDialog();
    void mergePdfs();
    void linearizeDocument();
    void exportAsPdfA();

    // ── U08 pre-execution capability disclosure ─────────────────────────────
    // gateExport: false → the registry reported the format unavailable; the
    // whyNot + alternative are surfaced and NO file dialog / worker runs.
    // A null registry (tests, early boot) keeps the previous behavior.
    bool gateExport(gp::CapId id);
    // Pre-dialog format disclosure: with the in-house OOXML writers the real-
    // OOXML path is unconditional, so the notice names the writer that WILL
    // run (the §9.16 honest badge, moved before the file dialog).
    QString exportFormatNotice(gp::CapId id) const;

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
};

} // namespace gp
