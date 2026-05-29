# M3-PROMPT-2 Pre-Baked Context

## Current State
- **Commit:** `e3c9e24` (May 29, 2026 — M2-P4 D4: CHANGELOG adversarial crypto + RSA enforcement)
- **Tests:** 17/17 ctest suites passing (9/9 TestSignatureValidation real-crypto, 21/21 Mock)
- **Branch:** `main` | Working tree: clean

---

## BatchMode.h

**Class members:**
- `QProgressBar* m_overallProgress` — tracks total progress across all files
- `QProgressBar* m_fileProgress` — tracks per-file progress percentage
- `QTextEdit* m_logView` — read-only log display (JetBrains Mono, 10px, fixed 130px height)
- `QLabel* m_statusLabel` — monospace status text (IDLE → completion summary)
- `QPushButton* m_exportLogBtn` — hidden until batch completes; visible if ErrorLog has entries
- `QFutureWatcher<void> m_watcher` — QtConcurrent watcher for async batch pipeline
- `QStringList m_filesToProcess` — queue of files to process (currently unused in v1.0.0)
- `ErrorLog m_errorLog` — collects all ErrorInfo from failed files
- `int m_successCount` — counter for successful files
- `int m_failCount` — counter for failed files

**Slots:**
- `onRunClicked()` — triggered by RUN NOW button
- `onBatchProgress(int value)` — progress update from watcher
- `onBatchFinished()` — batch completion handler
- `onExportLog()` — export log to CSV/JSON

**Private helpers:**
- `appendLog(const QString& text, const QString& color = {})` — append HTML-colored text to m_logView
- `appendFileResult(const QString& file, bool success, const QString& detail = {})` — log per-file result (✓ green or ✕ red)
- `showSummary()` — display final batch summary (count of success/fail/warnings)

---

## BatchMode.cpp Preview Shell

**Preview banner:** lines 25–33
- Orange warning box ("Preview — not wired in v1.0.0") stating batch is "scheduled for v1.1"
- All interactive widgets globally disabled (lines 159–167)

**UI structure:**
- **Left panel (220px):** OPERATIONS list with categories (ORGANIZE, TRANSFORM, PROTECT, EXPORT)
- **Center panel (1fr):** PIPELINE · 5 STEPS with hardcoded workflow (INPUT → OCR → COMPRESS → WATERMARK → OUTPUT)
- **Right panel (260px):** CONFIGURE · OCR (hardcoded: EN+FR, Tesseract 5, 80% confidence, etc.)
- **Progress section:** status label + two progress bars (overall + per-file)
- **Log section:** read-only monospace text widget (130px height)

**Constructed widgets:**
- QLabel (preview banner, step cards, cfg panel, arrows)
- QListWidget (operations with category headers + items)
- QScrollArea (pipeline steps)
- QToolButton (RUN NOW button, property "variant"="accent", 38px height)
- QLineEdit (search field, disabled)
- QPushButton (export log, initially hidden)
- QProgressBar × 2 (overall + file)
- QTextEdit (log view, read-only)

**`onRunClicked()` method:** lines 206–247
- **Current behavior (FAKE LOOP):**
  - Clears m_filesToProcess (empty in v1.0.0)
  - Sets progress bar range to `m_filesToProcess.size()` (→ 0)
  - Clears log, error log, counters
  - Logs "Starting batch processing — 0 files queued"
  - `QtConcurrent::map()` over empty QStringList
  - Per-file lambda: simulates work with `QThread::msleep(200)` × 5 (1-second fake delay per "file")
  - Increments counters and logs results via QMetaObject::invokeMethod (Qt::QueuedConnection)
  - Watcher watches the future and emits `progressValueChanged()` + `finished()` signals

- **What needs to happen in M3-PROMPT-2:**
  - Populate m_filesToProcess from user-selected watch folder or file dialog
  - Replace fake sleep loop with real engine calls:
    - For each file: call `ConversionManager::convertTo()` or `PdfEditorEngine::optimizeDocument()` or similar batch operations
    - Catch ErrorInfo from each operation
    - Log results to m_errorLog

**`m_filesToProcess` usage:**
- **Type:** `QStringList`
- **Currently:** Declared but never populated (hardcoded empty in v1.0.0)
- **Will be:** Populated by file-selection dialog or watch-folder scanner
- **Iterated by:** `QtConcurrent::map()` (line 219)

---

## ConversionManager.h

**`convertTo()` signature:**
```cpp
bool convertTo(const QString &pdfPath, const QString &outputPath, TargetFormat format, const QVariantMap &options = {}) override;
```

**Supported formats** (enum `TargetFormat`):
1. `Word` — exportToWord (via TextElement rows)
2. `Excel` — exportToExcel (via TextElement rows)
3. `Html` — exportToHtml (HTML+CSS export)
4. `Image` — exportToImage (rasterized pages, options: DPI, format, etc.)
5. `Csv` — exportToCsv (via TextElement rows)
6. `OfficeToPdf` — convertOfficeToPdf (Office → PDF via external converter)

**Private implementation methods:**
- `exportToWord(const QString &outputPath, const QList<QList<TextElement>> &rows)` — structured table export
- `exportToExcel(const QString &outputPath, const QList<QList<TextElement>> &rows)` — structured table export
- `exportToHtml(const QString &pdfPath, const QString &outputPath)` — DOM-based HTML export
- `exportToImage(const QString &pdfPath, const QString &outputPath, const QVariantMap &options)` — rasterization
- `exportToCsv(const QString &outputPath, const QList<QList<TextElement>> &rows)` — CSV with headers
- `convertOfficeToPdf(const QString &officePath, const QString &outputPath)` — Office binary → PDF

---

## PdfEditorEngine.h Batch-Relevant Methods

**Document operations:**
```cpp
bool loadDocumentForEditing(const QString &filePath) override;
bool saveDocument(const QString &outputPath) override;
```

**Structural/QPDF tasks:**
```cpp
bool linearizeDocument(const QString &outputPath) override;
bool exportPdfA(const QString &outputPath, int conformanceLevel) override;
bool encryptDocument(const QString &userPassword, const QString &ownerPassword, bool canPrint, bool canCopy, bool canModify) override;
bool encryptWithCertificate(const QString &inputPath, const QString &outputPath, const QStringList &certPaths) override;
bool sanitizeDocument(const QString &outputPath) override;
```

**Page operations:**
```cpp
bool rotatePage(const QString &path, int pageIndex, int degrees) override;
bool deletePage(const QString &path, int pageIndex) override;
bool insertBlankPage(const QString &path, int atIndex) override;
bool cropPage(const QString &path, int pageIndex, const QRectF &cropRect) override;
bool resizePage(const QString &path, int pageIndex, const QSizeF &size) override;
bool reorderPages(const QString &path, int fromIndex, int toIndex) override;
```

**Content injection:**
```cpp
bool addHeaderFooter(const QString &path, const HeaderFooterOptions &options) override;
bool applyBatesNumbering(const QString &path, const BatesNumberingOptions &options) override;
bool addTextWatermark(const TextWatermarkOptions &options) override;
bool addImageWatermark(const ImageWatermarkOptions &options) override;
```

**Image + annotation:**
```cpp
bool replaceImage(int pageIndex, const QString &xobjectName, const QString &newImagePath) override;
bool deleteImage(int pageIndex, const QString &xobjectName) override;
bool applyRedactions(int pageIndex, const QList<QRectF> &rects) override;
bool embedAnnotations(const QString &inputPath, const QString &outputPath, const QList<AnnotationItem> &annotations) override;
```

**Optimization:**
```cpp
OptimizeEstimate estimateOptimization(const OptimizeOptions &options) override;
bool optimizeDocument(const QString &outputPath, const OptimizeOptions &options) override;
```

**Error reporting:**
```cpp
ErrorInfo lastError() const override;
void clearError() override;
```

---

## ErrorInfo.h / ErrorLog

**ErrorInfo struct:**
- **Severity enum:** `Info, Warning, Error, Critical`
- **SuggestedAction flags:** `None | Retry | Skip | ExportLog | Repair`
- **Fields:**
  - `Severity severity` (default: Error)
  - `QString userMessage` — human-readable, shown in UI
  - `QString technicalDetails` — stack trace, expandable
  - `SuggestedActions suggestedActions` (default: None)
  - `QString sourceFile` — originating file path (batch context)
  - `int sourcePage` (default: -1)
  - `QDateTime timestamp` (auto-set to current time)

**Static constructors:**
- `ErrorInfo::info(const QString& msg)`
- `ErrorInfo::warning(const QString& msg, const QString& tech = {})`
- `ErrorInfo::error(const QString& msg, const QString& tech = {}, SuggestedActions actions = None)`
- `ErrorInfo::critical(const QString& msg, const QString& tech = {})`

**ErrorLog struct:**
```cpp
struct ErrorLog {
    QList<ErrorInfo> entries;
    
    void append(const ErrorInfo& e);
    void append(ErrorInfo&& e);
    int count() const;
    int errorCount() const;
    int warningCount() const;
    bool hasErrors() const;
    bool exportCsv(const QString& path) const;    // ← BATCH EXPORT
    bool exportJson(const QString& path) const;   // ← BATCH EXPORT
    void clear();
};
```

**Export signatures:**
- `exportCsv(const QString& path) const` — writes CSV with columns (timestamp, severity, sourceFile, sourcePage, userMessage, technicalDetails)
- `exportJson(const QString& path) const` — writes JSON array of ErrorInfo objects

---

## Integration Approach

**What BatchMode.cpp `onRunClicked()` needs to call:**

1. **Get file list:**
   - `QFileDialog::getOpenFileNames()` OR watch a folder via `QFileSystemWatcher`
   - Populate `m_filesToProcess` with selected PDFs

2. **For each file in the QtConcurrent lambda:**
   - Call either:
     - `ConversionManager::convertTo(file, outputPath, format, options)` for export operations
     - `PdfEditorEngine::loadDocumentForEditing(file)` → operation (optimize, watermark, redact, etc.) → `saveDocument(outputPath)` for in-place edits
     - `PdfEditorEngine::exportPdfA(outputPath, level)` for PDF/A conformance
     - `PdfEditorEngine::encryptDocument()` or `encryptWithCertificate()` for protection
   - Capture return bool + `lastError()` from engine
   - On failure: create `ErrorInfo::error()` with technical details from engine's ErrorInfo
   - On success: increment `m_successCount`

3. **Logging & error collection:**
   - Call `appendFileResult(file, success, errorDetail)` for each file
   - Append ErrorInfo to `m_errorLog` if operation failed
   - Update progress bar via `m_overallProgress->setValue(m_successCount + m_failCount)`

4. **Cleanup on completion:**
   - `onBatchFinished()` already calls `showSummary()`
   - `onExportLog()` already calls `m_errorLog.exportCsv(path)` or `exportJson(path)` when user clicks "Export Log"

**Key dependencies:**
- `AppContext` (Bootstrapper) must provide instance of `PdfEditorEngine` + `ConversionManager`
- Engine methods must be exception-safe; errors returned as `ErrorInfo` (not exceptions)
- QtConcurrent::map already handles thread pooling; no manual QThread creation needed
- Watcher signals automatically invoke slots on GUI thread (Qt::QueuedConnection handled)
