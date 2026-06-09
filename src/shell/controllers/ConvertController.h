// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>
#include "core/ToolId.h"
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

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
};

} // namespace gp
