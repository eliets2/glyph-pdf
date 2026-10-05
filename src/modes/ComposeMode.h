// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QWidget>
#include <QByteArray>
#include <QList>
#include <QImage>
#include <QRectF>
#include <QSizeF>
#include <QUndoStack>
#include <QPointer>
#include <functional>
#include <memory>

#include "engines/ImageExtractEngine.h"
#include "commands/CheckedHistory.h"

struct AppContext;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QSpinBox;
class QToolButton;
class IPdfRenderer;
class IPdfEditorEngine;

namespace gp {
class ComposeApplyCommand;
}

namespace gp {

// ── §9.17/§9.18: Side-by-side visual composition ────────────────────────────
//
// A two-pane compose view: left pane = SOURCE document, right pane = TARGET
// document. Both panes are live documents (any two PDFs — or the same PDF
// twice). The user picks pages (thumbnail checkboxes) and images (the visual
// image picker over a page's /XObject /Image inventory) from either pane; the
// picks queue as pending transfers INTO the other pane. Apply commits each
// destination's pending set as ONE checked-history undo step through a
// SafeSave candidate transaction (candidate → validate → identity-guarded
// commit) — the destination file is never partially written, and the source
// of a transfer is only ever written by an explicit Apply aimed at it.

/// Which pane a pick came from (ComposeSide::Source = left, Target = right).
enum class ComposeSide { Source = 0, Target = 1 };

/// One image pick: the page + resource name it came from, plus the native
/// pixel size and /Filter chain the inventory reported.
struct ComposeImagePick {
    int sourcePage = -1;      ///< 0-based page the image was picked from
    QString xobjectName;      ///< resource name in that page's /XObject dict
    QSize pixelSize;          ///< native pixels (/Width × /Height)
    QStringList filters;      ///< /Filter entries as reported by the extract engine

    bool operator==(const ComposeImagePick& other) const {
        return sourcePage == other.sourcePage && xobjectName == other.xobjectName
               && pixelSize == other.pixelSize && filters == other.filters;
    }
};

/// The mode widget. Pure seams are static/public so tests drive the whole
/// contract headlessly (the CompareMode test-seam idiom).
class ComposeMode : public QWidget {
    Q_OBJECT
public:
    explicit ComposeMode(QWidget* parent = nullptr);
    ~ComposeMode() override;

    // Called by ModeController after construction (the PagesMode pattern).
    void setAppContext(const AppContext* ctx);

    // ── Two-document open ────────────────────────────────────────────────
    // Open the two live documents (source = left pane, target = right pane).
    // The same path twice is allowed; a missing or unloadable path refuses
    // with false and leaves the previous state untouched.
    bool setDocuments(const QString& sourcePath, const QString& targetPath);
    QString sourcePath() const { return m_panes[static_cast<int>(ComposeSide::Source)].path; }
    QString targetPath() const { return m_panes[static_cast<int>(ComposeSide::Target)].path; }
    int pageCount(ComposeSide side) const;

    // ── §9.17 pure seams (shared with the tests) ─────────────────────────

    // Insertion index of the k-th picked page (k = 0-based ordinal within the
    // pick group) when the group is inserted "after page N" (0-based;
    // N = -1 means before the first page).
    static int insertionIndexFor(int afterPage0Based, int k);

    // Honest mismatched-page-size disclosure. "" when the two sizes agree
    // within 0.5 pt; otherwise names both sizes and states the truth: the
    // page keeps its own size and is NEVER stretched to the target's.
    static QString pageSizeDisclosure(const QSizeF& sourcePage, const QSizeF& targetPage);

    // Aspect-preserving fit of `imageSizePt` inside `box` (letterboxed,
    // centered) — the rect placeImageOnPage is given. The compose convention
    // is 1 px = 1 pt (72 ppi), disclosed in the picker.
    static QRectF fittedRect(const QSizeF& imageSizePt, const QRectF& box);

    // Honest image-placement disclosure: names the placement scale and states
    // that the aspect ratio is preserved (never stretched).
    static QString imagePlacementDisclosure(const QSizeF& imageSizePt, const QRectF& box);

    // ── Picks ────────────────────────────────────────────────────────────
    void clearPicks();
    void addPagePick(ComposeSide side, int page0Based);
    void removePagePick(ComposeSide side, int page0Based);
    QList<int> pagePicks(ComposeSide side) const;
    void addImagePick(ComposeSide side, const ComposeImagePick& pick);
    void clearImagePicks(ComposeSide side);
    QList<ComposeImagePick> imagePicks(ComposeSide side) const;

    // Insert position for transfers INTO `destination`: "after page N"
    // (0-based, -1 = before the first page). Default -1.
    void setInsertAfterPage(ComposeSide destination, int afterPage0Based);
    int insertAfterPage(ComposeSide destination) const;

    // Placement box for image transfers INTO `destination` (PDF user space,
    // bottom-left origin — the PdfImageInfo::placement convention). The drawn
    // rect is fittedRect(imageSizePt, box). Default: centered half-page box.
    void setImagePlacementBox(ComposeSide destination, const QRectF& box);
    QRectF imagePlacementBox(ComposeSide destination) const;

    // ── Preview + apply ──────────────────────────────────────────────────

    // One row per pending transfer as it will be applied, including the
    // honest size disclosure for a mismatched page. The live preview of
    // record for the composed result.
    QStringList pendingSummary() const;

    // Live textual preview of the composed TARGET page order, e.g.
    // "1 2 [S1] [S2] 3" — bracketed entries are the pending source picks.
    QString composedOrderPreview() const;

    // Apply every pending transfer. Each affected destination session gets
    // ONE checked-history undo step (ComposeApplyCommand). Returns false with
    // a user-presentable `why` on refusal: nothing is committed for the
    // refused destination, no history step is pushed, retry is possible.
    bool applyTransfers(QString* why = nullptr);

    // Checked traversal over one destination session's compose history
    // (the CheckedHistory idiom — a failed restore stays retryable).
    bool undoTransfers(ComposeSide destination);
    bool redoTransfers(ComposeSide destination);
    int historyCount(ComposeSide destination) const;

    // ── §9.17 image inventory (visual picker data) ───────────────────────
    // Inventory of one page of one side. Testable without the widget state.
    static QList<ComposeImageInfo> imageInventory(const QString& pdfPath, int pageIndex);

    // Number of pages of the PDF at `path` through the renderer backend
    // (the PagesMode page-count probe). -1 when the file cannot be opened.
    static int pageCountFor(const QString& path);

signals:
    // §9.8 P0 parity: status text relayed to the host's status bar by
    // ModeController (the redactStatusMessage idiom).
    void statusMessageRequested(const QString& message);

private:
    friend class ComposeApplyCommand;   // reads pane paths for its transaction

    struct Pane {
        QString path;
        QString role;                    // "Source" / "Target"
        std::unique_ptr<IPdfRenderer> renderer;
        int pageCount = 0;
        QList<QSizeF> pageSizes;
        QListWidget* grid = nullptr;     // page thumbnails (checkable picks)
        QListWidget* imagePicker = nullptr; // images of the current page
        QLabel* header = nullptr;
        int currentPage = 0;
    };

    Pane& pane(ComposeSide side) { return m_panes[static_cast<int>(side)]; }
    const Pane& pane(ComposeSide side) const { return m_panes[static_cast<int>(side)]; }
    static ComposeSide other(ComposeSide side) {
        return side == ComposeSide::Source ? ComposeSide::Target : ComposeSide::Source;
    }

    void buildUi();
    bool loadPane(ComposeSide side, const QString& path);   // renders + fills the grid
    void fillPageGrid(ComposeSide side);
    void fillImagePicker(ComposeSide side, int pageIndex);
    void refreshTransferUi();
    void updateFilesLabel();
    void setStatus(const QString& message);
    QString sideTitle(ComposeSide side) const;
    QWidget* paneFrameOf(ComposeSide side);

    // Apply internals (GREEN stage).
    // Apply the picks of `other(destination)` INTO `destination` as ONE
    // checked-history step. Returns false with a user-presentable `err`;
    // the destination is untouched.
    bool applyOneSide(ComposeSide destination, int& outPages, int& outImages, QString* err);
    // The configured image placement box, or the auto box (centered half
    // page of the placement page) when none was set.
    QRectF effectiveImageBox(ComposeSide destination) const;
    // The destination page an image lands on: the first page AFTER the whole
    // inserted block (clamped).
    int imageTargetPage(ComposeSide destination) const;
    void reloadPaneIfLoaded(ComposeSide side);
    Pane m_panes[2];
    QUndoStack m_history[2];                    // one compose history per session
    QList<int> m_pagePicks[2];
    QList<ComposeImagePick> m_imagePicks[2];
    int m_insertAfter[2] = { -1, -1 };          // destination-keyed
    QRectF m_imageBox[2];                       // destination-keyed (default in ctor)

    // toolbar
    QLabel* m_filesLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QSpinBox* m_afterSpin = nullptr;
    QToolButton* m_applyBtn = nullptr;
    QToolButton* m_undoBtn = nullptr;
    QToolButton* m_redoBtn = nullptr;
    QToolButton* m_chooseBtn = nullptr;
    QListWidget* m_transferList = nullptr;

    const AppContext* m_ctx = nullptr;

    // Operation-owned engines (the SplitEngineFactory idiom): the default
    // creates a fresh PdfEditorEngine; tests may inject a different factory.
    using EngineFactory = std::function<std::shared_ptr<IPdfEditorEngine>()>;
    EngineFactory m_engineFactory;
    void setEngineFactoryForTest(EngineFactory factory) { m_engineFactory = std::move(factory); }

private slots:
    void onPageItemChanged(QListWidgetItem* item);
    void onPaneCurrentRowChanged(int row);
    void onApply();
    void onUndo();
    void onRedo();
    void onChooseDocuments();
};

// ── ComposeApplyCommand — ONE checked-history step per destination ─────────
//
// redo()/applyChecked(): the FULL SafeSave transaction — target bytes → unique
// candidate → page inserts + image placements (operation-owned engines) →
// validate → identity-guarded commitFileToDestination. undo()/restoreChecked():
// the exact inverse transaction (image pages restored from pre-place backups,
// inserted pages deleted in reverse). A refusal anywhere leaves the
// destination byte-identical and the history position untouched.
class ComposeApplyCommand : public CheckedUndoCommand {
public:
    struct PageInsert {
        QByteArray onePagePdf;   // complete single-page PDF (extractPageAsBytes)
        int atIndex;             // destination index, ascending within one apply
        QString disclosure;      // honest size disclosure (may be empty)
    };
    struct ImagePlacement {
        int pageIndex;           // destination page (post-insert index)
        QImage image;
        QRectF rect;             // exact draw rect (PDF user space)
        double opacity = 1.0;
        QByteArray pageBackup;   // pre-place page bytes (extractPageAsBytes)
    };

    // `engineFactory` creates operation-owned engines; `beforePageCount` is
    // the destination's page count before the inserts (the undo validation
    // target); `label` is the human command text.
    using EngineFactory = std::function<std::shared_ptr<IPdfEditorEngine>()>;
    ComposeApplyCommand(EngineFactory factory, const QString& destinationPath,
                        QList<PageInsert> inserts, QList<ImagePlacement> placements,
                        int beforePageCount, const QString& label);

    bool applyChecked() override;
    bool restoreChecked() override;
    void redo() override;
    void undo() override;

    int insertedPageCount() const { return m_inserts.size(); }
    int placedImageCount() const { return m_placements.size(); }
    const QString& destinationPath() const { return m_destination; }
    // The last refusal reason (empty after a successful application). The
    // mode folds this into its user-facing refusal wording.
    const QString& lastError() const { return m_lastError; }

    // The full transaction against `destination`'s bytes. Returns false with
    // `err` set and the destination byte-identical on any refusal.
    // `inserts`/`placements` are applied as stored; the placements' page
    // backups are captured here (before any draw) when empty.
    static bool runTransaction(EngineFactory& factory, const QString& destination,
                               const QList<PageInsert>& inserts,
                               QList<ImagePlacement>& placements,
                               int expectedPageCountAfter, QString* err);

private:
    static bool copyFile(const QString& from, const QString& to, QString* err);
    bool runRestore(QString* err);

    EngineFactory m_factory;
    QString m_destination;
    QList<PageInsert> m_inserts;
    QList<ImagePlacement> m_placements;
    int m_beforePageCount;
    QString m_lastError;   // last refusal reason (diagnostics; the mode owns the user-facing wording)
    bool m_appliedOnce = false;
};

} // namespace gp
