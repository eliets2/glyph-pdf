// SPDX-License-Identifier: Apache-2.0
#include "PagesController.h"
#include "shell/EditPolicy.h"
#include "core/AppContext.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"
#include "commands/RotatePageCommand.h"
#include "commands/DeletePageCommand.h"
#include "commands/InsertPageCommand.h"
#include "commands/CropPageCommand.h"
#include "commands/ReorderPermutationCommand.h"
#include "ui/PageManagementDialog.h"
#include "ui/ResizeDialog.h"
#include "ui/HeaderFooterDialog.h"
#include "ui/BatesNumberingDialog.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QMessageBox>
#include <QUndoStack>
#include "shell/StatusBar.h"
#include "engines/SafeSave.h"

namespace {

// G03 (QUALITY-GATE-2026-09-09): RAII removal of a staged Bates candidate.
// The run created this exact uniquely-owned path (SafeSave::makeUniqueCandidate)
// and only ever removes it — a pre-existing user file at any name is never a
// deletion target.
struct CandidateCleanup {
    QString path;
    ~CandidateCleanup() { if (!path.isEmpty()) QFile::remove(path); }
    CandidateCleanup(const CandidateCleanup&) = delete;
    CandidateCleanup& operator=(const CandidateCleanup&) = delete;
};

} // namespace

namespace gp {

PagesController::PagesController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

// ARC07: dispatch and enablement share ONE predicate (shell/EditPolicy.h).
bool PagesController::isEnabled(ToolId id) const {
    return !EditPolicy::toolRefusedByReadOnly(
        _ctx && _ctx->document ? _ctx->document.get() : nullptr, id);
}

QList<ToolId> PagesController::handledTools() const {
    return {
        ToolId::RotateCW, ToolId::RotateCCW,
        ToolId::DeletePage, ToolId::InsertPage,
        ToolId::Extract, ToolId::Split, ToolId::Reorder,
        ToolId::Crop, ToolId::Resize, ToolId::AddHeader, ToolId::AddFooter,
        ToolId::AddPageNumbers, ToolId::BatesNumber
    };
}

void PagesController::activate(ToolId id) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) {
        _mainWindow->statusBar()->showMessage(tr("No document is open."), 3000);
        return;
    }

    switch (id) {
    case ToolId::RotateCW:
        rotateRight();
        break;
    case ToolId::RotateCCW:
        rotateLeft();
        break;
    case ToolId::DeletePage:
        if (_ctx && _ctx->undoStack && viewer->pageCount() > 1) {
            _ctx->document->setPath(viewer->filePath());
            _ctx->undoStack->push(new DeletePageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), viewer->currentPage()));
            _mainWindow->statusBar()->showMessage(tr("Deleted current page."), 3000);
        } else {
            _mainWindow->statusBar()->showMessage(tr("Cannot delete the last page of the document."), 3000);
        }
        break;
    case ToolId::InsertPage:
        if (_ctx && _ctx->undoStack) {
            _ctx->document->setPath(viewer->filePath());
            _ctx->undoStack->push(new InsertPageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), viewer->currentPage() + 1));
            _mainWindow->statusBar()->showMessage(tr("Inserted blank page."), 3000);
        }
        break;
    case ToolId::Extract:
        showPageManagement();
        break;
    case ToolId::Split:
    case ToolId::Reorder:
        _mainWindow->activateScreen("pages");
        break;
    case ToolId::Crop:
        viewer->setToolMode(ToolMode::Crop);
        _mainWindow->statusBar()->showMessage(tr("Crop Tool: Drag a rectangle on the page to crop."), 5000);
        break;
    case ToolId::Resize: {
        ResizeDialog dlg(_mainWindow);
        if (dlg.exec() == QDialog::Accepted) {
            QSizeF sz = dlg.selectedSize();
            if (sz.width() > 0 && sz.height() > 0) {
                if (_ctx && _ctx->pdfEditor) {
                    _ctx->pdfEditor->resizePage(viewer->filePath(), viewer->currentPage(), sz);
                    viewer->reload();
                    _mainWindow->statusBar()->showMessage(tr("Page resized."), 3000);
                }
            }
        }
        break;
    }
    case ToolId::AddHeader:
    case ToolId::AddFooter:
    case ToolId::AddPageNumbers: {
        HeaderFooterDialog dlg(_mainWindow);
        if (dlg.exec() == QDialog::Accepted) {
            HeaderFooterOptions opt = dlg.options();
            if (_ctx && _ctx->pdfEditor) {
                _ctx->pdfEditor->addHeaderFooter(viewer->filePath(), opt);
                viewer->reload();
                _mainWindow->statusBar()->showMessage(tr("Header/Footer added."), 3000);
            }
        }
        break;
    }
    case ToolId::BatesNumber: {
        BatesNumberingDialog dlg(_mainWindow);
        dlg.setPageCount(viewer->pageCount());  // bound the range UI to the doc
        if (dlg.exec() == QDialog::Accepted) {
            BatesNumberingOptions opt = dlg.options();
            if (_ctx && _ctx->pdfEditor) {
                const QStringList batch = dlg.batchFiles();
                if (!batch.isEmpty()) {
                    // §9.9 P1: number the listed files as ONE continuous
                    // sequence. Each input is copied to `<stem>_bated.pdf`
                    // and the copy is stamped, so the inputs are never
                    // modified; document N+1 starts at document N's last
                    // number + 1 (reported by the engine overload).
                    //
                    // G03 (P1, QUALITY-GATE-2026-09-09): the old loop removed
                    // each output BEFORE opening/validating its input, so a
                    // nonexistent input deleted its existing `_bated.pdf`
                    // destination; and with inputs `first.pdf` +
                    // `first_bated.pdf`, processing the first overwrote the
                    // second original (its default output IS a later input).
                    // The whole batch is now preflighted before anything is
                    // written, and each file is processed through a uniquely
                    // owned SafeSave candidate with a checked replacement —
                    // an existing output is only ever replaced after its own
                    // stamped copy is complete, and an input is never a
                    // deletion target.
                    const auto sameFile = [](const QString& a, const QString& b) {
                        if (QString::compare(a, b, Qt::CaseInsensitive) == 0) return true;
                        // Windows aliases: differing casing, 8.3 names,
                        // symlinks/hardlinks resolve to the same file.
                        const QFileInfo ai(a), bi(b);
                        const QString ca = ai.canonicalFilePath();
                        const QString cb = bi.canonicalFilePath();
                        return !ca.isEmpty() && ca == cb;
                    };
                    struct BatesJob { QString input; QString output; };
                    QList<BatesJob> jobs;
                    QString preflightConflict;
                    for (const QString& input : batch) {
                        const QFileInfo fi(input);
                        if (!fi.exists() || !fi.isFile()) {
                            preflightConflict = input;
                            _mainWindow->statusBar()->showMessage(
                                tr("Bates batch stopped before writing anything: "
                                   "input %1 does not exist; no file was changed.")
                                    .arg(fi.fileName()), 8000);
                            break;
                        }
                        const QString output = fi.dir().filePath(
                            fi.completeBaseName() + QStringLiteral("_bated.pdf"));
                        bool collision = false;
                        for (const BatesJob& job : jobs) {
                            if (sameFile(output, job.output)) { collision = true; break; }
                        }
                        for (const QString& other : batch) {
                            if (sameFile(output, other)) { collision = true; break; }
                        }
                        if (collision) {
                            preflightConflict = output;
                            _mainWindow->statusBar()->showMessage(
                                tr("Bates batch stopped before writing anything: "
                                   "output %1 is itself a listed input; no file was "
                                   "changed (rename the file or drop it from the list).")
                                    .arg(QFileInfo(output).fileName()), 8000);
                            break;
                        }
                        jobs.append({ input, output });
                    }

                    qint64 counter = opt.startNumber;
                    int stamped = 0;
                    QString stoppedAt;
                    QString lastStagedCandidate;   // G15: released on every exit
                    if (preflightConflict.isEmpty()) {
                        for (const BatesJob& job : jobs) {
                            // Uniquely owned candidate: the run created it and
                            // only ever removes this exact path (RAII guard
                            // covers every early exit).
                            QString candidate, commitErr;
                            if (!gp::SafeSave::makeUniqueCandidate(&candidate, &commitErr)) {
                                stoppedAt = job.input;
                                _mainWindow->statusBar()->showMessage(
                                    tr("Bates batch stopped: could not create a staging "
                                       "copy (%1).").arg(commitErr), 8000);
                                break;
                            }
                            const CandidateCleanup guard{ candidate };
                            lastStagedCandidate = candidate;   // G15
                            QFile srcFile(job.input);
                            if (!srcFile.open(QIODevice::ReadOnly)
                                || !QFile::remove(candidate)
                                || !srcFile.copy(candidate)) {
                                stoppedAt = job.input;
                                _mainWindow->statusBar()->showMessage(
                                    tr("Bates batch stopped: could not stage %1.")
                                        .arg(QFileInfo(job.input).fileName()), 5000);
                                break;
                            }
                            srcFile.close();
                            // The engine refuses to mutate a path other than its
                            // resident document — load each staged copy first.
                            if (!_ctx->pdfEditor->loadDocumentForEditing(candidate)) {
                                stoppedAt = job.input;
                                _mainWindow->statusBar()->showMessage(
                                    tr("Bates batch stopped: could not open the staged "
                                       "copy of %1.").arg(QFileInfo(job.input).fileName()), 5000);
                                break;
                            }
                            BatesNumberingOptions step = opt;
                            step.startNumber = static_cast<int>(counter);
                            int lastUsed = static_cast<int>(counter) - 1;
                            if (!_ctx->pdfEditor->applyBatesNumbering(candidate, step, &lastUsed)) {
                                stoppedAt = job.input;
                                _mainWindow->statusBar()->showMessage(
                                    tr("Bates batch stopped while stamping %1.")
                                        .arg(QFileInfo(job.input).fileName()), 5000);
                                break;
                            }
                            // Checked replacement: the previous output (if any)
                            // survives until the fully stamped candidate commits.
                            if (!gp::SafeSave::commitFileToDestination(candidate, job.output, &commitErr)) {
                                stoppedAt = job.input;
                                _mainWindow->statusBar()->showMessage(
                                    tr("Bates batch stopped: could not write %1 (%2).")
                                        .arg(QFileInfo(job.output).fileName(), commitErr), 5000);
                                break;
                            }
                            // Release the staged copy from the shared editor so
                            // its OS device is closed before the RAII guard
                            // removes the candidate file.
                            _ctx->pdfEditor->releaseResidentFile(candidate);
                            counter = lastUsed + 1;
                            ++stamped;
                        }
                        // The batch hijacked the shared interactive editor for
                        // per-file staging copies. Restore the editor to the
                        // ACTIVE document on every exit so the engine never
                        // stays on a (now removed) staging file. (Preflight
                        // refusals never touched the editor and skip this.)
                        // G15 (P2, QUALITY-GATE-2026-09-09): the re-anchor is
                        // CHECKED. When the active document cannot be loaded
                        // (vanished mid-session, locked, empty path) the
                        // engine would stay resident on the staged candidate
                        // the RAII guard is about to delete — every later
                        // interactive edit failing against a doomed identity.
                        // Dropping the staged copy from the editor leaves one
                        // COHERENT identity on every exit: either the active
                        // document, or an honestly empty engine.
                        {
                            const QString activePath = viewer->filePath();
                            const bool reanchored = !activePath.isEmpty() && QFile::exists(activePath)
                                && _ctx->pdfEditor->loadDocumentForEditing(activePath);
                            if (!reanchored && !lastStagedCandidate.isEmpty())
                                _ctx->pdfEditor->releaseResidentFile(lastStagedCandidate);
                        }
                        if (stoppedAt.isEmpty()) {
                            _mainWindow->statusBar()->showMessage(
                                tr("Bates batch applied to %1 file(s) (numbers %2–%3).")
                                    .arg(stamped)
                                    .arg(opt.prefix + QString::number(opt.startNumber).rightJustified(opt.digitCount, QLatin1Char('0')))
                                    .arg(opt.prefix + QString::number(counter - 1).rightJustified(opt.digitCount, QLatin1Char('0'))),
                                5000);
                        }
                    }
                } else {
                    _ctx->pdfEditor->applyBatesNumbering(viewer->filePath(), opt);
                    viewer->reload();
                    _mainWindow->statusBar()->showMessage(tr("Bates Numbering applied."), 3000);
                }
            }
        }
        break;
    }
    default:
        break;
    }
}

void PagesController::rotateLeft() {
    auto* viewer = _mainWindow->pdfViewer();
    if (viewer && _ctx && _ctx->undoStack) {
        _ctx->undoStack->push(new RotatePageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), viewer->currentPage(), -90));
    }
}

void PagesController::rotateRight() {
    auto* viewer = _mainWindow->pdfViewer();
    if (viewer && _ctx && _ctx->undoStack) {
        _ctx->undoStack->push(new RotatePageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), viewer->currentPage(), 90));
    }
}

void PagesController::onPageRotateRequested(int degrees) {
    // §9.1 P0: viewer rotation requests land here — the real page bitmap
    // rotates (engine-side /Rotate + reload), not just the overlay.
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->undoStack || !_ctx->pdfEditor) return;
    // ARC07: this entry bypasses the registry (a viewer signal), so the
    // shared read-only gate is applied here too.
    if (EditPolicy::mutationBlocked(_ctx->document.get())) {
        _mainWindow->statusBar()->showMessage(EditPolicy::readOnlyMessage(), 5000);
        return;
    }
    _ctx->document->setPath(viewer->filePath());
    _ctx->undoStack->push(new RotatePageCommand(
        _ctx->pdfEditor.get(), _ctx->document.get(), viewer->currentPage(), degrees));
}

void PagesController::showPageManagement() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx) return;

    PageManagementDialog dlg(viewer->pageCount(), _mainWindow);
    if (dlg.exec() == QDialog::Accepted) {
        PageManagementDialog::Operation op = dlg.selectedOperation();
        int from = dlg.fromPage() - 1;
        int to = dlg.toPage() - 1;

        if (op == PageManagementDialog::Operation::ExtractPages) {
            QString outputFile = QFileDialog::getSaveFileName(_mainWindow, tr("Save Extracted Pages"), "", tr("PDF Files (*.pdf)"));
            if (outputFile.isEmpty()) return;
            viewer->extractPages(from, to, outputFile);
        } else {
            _ctx->document->setPath(viewer->filePath());
            _ctx->undoStack->beginMacro(tr("Page Management"));
            if (op == PageManagementDialog::Operation::DeletePages) {
                for (int i = to; i >= from; --i) {
                    _ctx->undoStack->push(new DeletePageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), i));
                }
                _mainWindow->statusBar()->showMessage(tr("Deleted pages %1 to %2").arg(from + 1).arg(to + 1), 5000);
            } else if (op == PageManagementDialog::Operation::RotatePages) {
                for (int i = from; i <= to; ++i) {
                    _ctx->undoStack->push(new RotatePageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), i, dlg.rotationAngle()));
                }
                _mainWindow->statusBar()->showMessage(tr("Rotated pages %1 to %2").arg(from + 1).arg(to + 1), 5000);
            } else if (op == PageManagementDialog::Operation::InsertBlankPage) {
                _ctx->undoStack->push(new InsertPageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), from + 1));
                _mainWindow->statusBar()->showMessage(tr("Inserted blank page at %1").arg(from + 2), 5000);
            }
            _ctx->undoStack->endMacro();
        }
    }
}

QList<int> PagesController::buildMovePermutation(int pageCount, int from, int to) {
    QList<int> perm;
    if (pageCount <= 0 || from < 0 || from >= pageCount || to < 0 || to >= pageCount)
        return perm; // empty = invalid input
    for (int i = 0; i < pageCount; ++i) perm.append(i);
    const int moved = perm.takeAt(from);
    perm.insert(to, moved);
    return perm;
}

void PagesController::onPageReordered(int from, int to) {
    if (_ctx && _ctx->undoStack && _mainWindow->pdfViewer()) {
        auto* viewer = _mainWindow->pdfViewer();
        // ARC07: direct-entry mutation (thumbnail drag) — shared gate.
        if (EditPolicy::mutationBlocked(_ctx->document.get())) {
            _mainWindow->statusBar()->showMessage(EditPolicy::readOnlyMessage(), 5000);
            return;
        }
        _ctx->document->setPath(viewer->filePath());
        // §9.9 P0: route through the atomic permutation command (single disk
        // write, single undo step) instead of the legacy single-swap command.
        const QList<int> perm = buildMovePermutation(viewer->pageCount(), from, to);
        if (perm.isEmpty()) return;
        _ctx->undoStack->push(new ReorderPermutationCommand(_ctx->pdfEditor.get(), _ctx->document.get(), perm));
        _mainWindow->statusBar()->showMessage(tr("Reordered page %1 to %2.").arg(from + 1).arg(to + 1), 3000);
    }
}

void PagesController::onCropRequested(int pageIndex, QRectF rect) {
    if (_ctx && _ctx->undoStack && _mainWindow->pdfViewer()) {
        // ARC07: direct-entry mutation (crop rubber band) — shared gate.
        if (EditPolicy::mutationBlocked(_ctx->document.get())) {
            _mainWindow->statusBar()->showMessage(EditPolicy::readOnlyMessage(), 5000);
            return;
        }
        _ctx->document->setPath(_mainWindow->pdfViewer()->filePath());
        _ctx->undoStack->push(new CropPageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), pageIndex, rect));
        _mainWindow->statusBar()->showMessage(tr("Cropped page %1.").arg(pageIndex + 1), 3000);
    }
}

} // namespace gp

