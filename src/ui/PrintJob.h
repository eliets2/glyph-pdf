// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QPointer>
#include <QPrinter>
#include <QString>
#include <QVector>
#include <atomic>

class QPdfDocument;
class QPainter;
class QProgressDialog;
class QImage;

namespace gp {

// ── WP-R08: ONE owned print job ─────────────────────────────────────────────
// (WHOLE-PRODUCT-AND-PLAN-REVIEW-2026-09-10, PP03 + WHOLE-ARCHITECTURE-REVIEW
// job-lifetime rule)
//
// The previous print path ignored the print dialog's page-range selection
// (it always printed pages 0..N-1) and every page was rendered by a raw
// QThread capturing the LIVE view-owned QPdfDocument — a document switch,
// close or reload during printing dereferenced a document another component
// owns, and render/painter/newPage failures were silently ignored.
//
// This job owns an immutable input (its OWN QPdfDocument instance opened on
// the snapshot path — the live view's document is never touched), an explicit
// selected-page sequence (pageSequence()), cooperative cancellation (safe at
// every page boundary; nothing shared with the viewer is dereferenced), and
// exactly one terminal finished() outcome with render/painter/spool failures
// reported. The job is self-managing: PrintJob::start() returns immediately
// and the job deletes itself after finished().
class PrintJob : public QObject {
    Q_OBJECT
public:
    // Immutable print input. The job opens its own QPdfDocument on this path;
    // it never receives the view's document instance.
    struct Snapshot {
        QString filePath;
    };

    // The dialog's choices as an explicit 0-based page sequence.
    //   AllPages/Selection → 0..pageCount-1 (the view has no selection-print
    //                        surface; QPrintDialog cannot produce Selection
    //                        unless the app enables it — mapped to all)
    //   CurrentPage        → [currentPage-1] (empty when unknown/out of range)
    //   PageRange          → clamped 1-based [fromPage,toPage]; an inverted or
    //                        fully out-of-range range yields an empty sequence
    //   LastPageFirst      → sequence reversed
    static QVector<int> pageSequence(QPrinter::PrintRange range, int fromPage, int toPage,
                                     int pageCount, QPrinter::PageOrder order,
                                     int currentPage = 0);

    // Starts a self-managing job and returns immediately. Takes ownership of
    // `printer`. Shows a window-modal progress dialog parented to
    // `dialogParent` (when alive) with a working Cancel. The job never touches
    // `dialogParent`'s document; destroying the widget mid-print cannot affect
    // the job (the view's finished() box is skipped when there is no UI left).
    static PrintJob* start(const Snapshot& snapshot, QPrinter* printer,
                           const QVector<int>& pages, QPointer<QWidget> dialogParent = {});

    // Cooperative cancel: stops the sequence at the next page boundary (an
    // in-flight render cannot be interrupted — QtPdf has no cancellable
    // render; this matches the contract of safe page-boundary cancellation).
    void cancel();

signals:
    // Exactly one terminal outcome. printedPages counts pages actually handed
    // to the printer; canceled is true when cancel() stopped the run; error is
    // empty on success and carries a user-presentable reason on failure
    // (render failure, painter failure, newPage failure, printer error).
    void finished(int printedPages, bool canceled, const QString& error);

private:
    explicit PrintJob(QPrinter* printer, const QVector<int>& pages,
                      QPointer<QWidget> dialogParent, const QString& snapshotPath);
    ~PrintJob() override;

    // GUI-thread steps (queued invocations — no event-loop re-entrancy).
    void begin();             // open the owned snapshot document
    void renderNextPage();    // schedule one page render on a worker thread
    void paintRenderedPage(); // paint the rendered page / advance
    void finish();            // single terminal cleanup + finished()

    QPrinter* m_printer = nullptr;
    QVector<int> m_pages;
    QPointer<QWidget> m_dialogParent;
    QProgressDialog* m_progress = nullptr;
    QString m_snapshotPath;

    QPdfDocument* m_doc = nullptr;       // OWNED by the job — not the view's
    QPainter* m_painter = nullptr;
    QImage* m_rendered = nullptr;        // handoff from the render worker
    int m_index = 0;                     // next position in m_pages
    int m_printed = 0;
    std::atomic<bool> m_canceled{false};
    bool m_failed = false;
    bool m_finishedEmitted = false;
    QString m_error;
};

} // namespace gp
