// SPDX-License-Identifier: Apache-2.0
#include "ui/PrintJob.h"

#include <QImage>
#include <QPainter>
#include <QProgressDialog>
#include <QThread>
#include <QtPdf/QPdfDocument>

#include <algorithm>
#include <memory>

namespace gp {

// ── selection → sequence ────────────────────────────────────────────────────

QVector<int> PrintJob::pageSequence(QPrinter::PrintRange range, int fromPage, int toPage,
                                    int pageCount, QPrinter::PageOrder order,
                                    int currentPage)
{
    QVector<int> pages;
    if (pageCount <= 0)
        return pages;

    switch (range) {
    case QPrinter::CurrentPage:
        if (currentPage >= 1 && currentPage <= pageCount)
            pages.append(currentPage - 1);
        break;
    case QPrinter::PageRange: {
        // Unset range (0/0) degenerates to the full document; otherwise clamp
        // and reject inverted ranges — an impossible selection prints nothing
        // and is reported, never silently widened to "all pages".
        int from = fromPage, to = toPage;
        if (from <= 0 && to <= 0) { from = 1; to = pageCount; }
        from = qBound(1, from, pageCount);
        to = qBound(1, to, pageCount);
        if (from > to)
            break;
        for (int p = from; p <= to; ++p)
            pages.append(p - 1);
        break;
    }
    case QPrinter::Selection:
        // No selection-print surface exists in the view; QPrintDialog cannot
        // produce this range unless the application enables it. Mapped to the
        // full document (documented, never a silent empty job).
        // fallthrough
    case QPrinter::AllPages:
    default:
        for (int p = 0; p < pageCount; ++p)
            pages.append(p);
        break;
    }

    if (order == QPrinter::LastPageFirst)
        std::reverse(pages.begin(), pages.end());
    return pages;
}

// ── job lifecycle ───────────────────────────────────────────────────────────

PrintJob::PrintJob(QPrinter* printer, const QVector<int>& pages,
                   QPointer<QWidget> dialogParent, const QString& snapshotPath)
    : QObject(nullptr)  // deliberately parentless: self-managing, survives the view
    , m_printer(printer)
    , m_pages(pages)
    , m_dialogParent(dialogParent)
    , m_snapshotPath(snapshotPath)
{
    connect(this, &PrintJob::finished, this, &PrintJob::deleteLater);
}

PrintJob::~PrintJob()
{
    delete m_painter;
    delete m_printer;
    delete m_doc;
    delete m_rendered;
}

PrintJob* PrintJob::start(const Snapshot& snapshot, QPrinter* printer,
                          const QVector<int>& pages, QPointer<QWidget> dialogParent)
{
    auto* job = new PrintJob(printer, pages, dialogParent, snapshot.filePath);
    // Queued, so the job exists and callers can connect before any step runs.
    QMetaObject::invokeMethod(job, &PrintJob::begin, Qt::QueuedConnection);
    return job;
}

void PrintJob::cancel()
{
    // QProgressDialog::closeEvent EMITS canceled() — its automatic close when
    // the value reaches maximum and our own terminal close both fire it. A
    // cancel that arrives after the last page is painted is moot: the print
    // is complete and must be reported as a success, not a cancellation.
    if (m_finishedEmitted || m_index >= m_pages.size())
        return;
    m_canceled.store(true);
}

void PrintJob::begin()
{
    if (m_canceled.load()) {
        finish();
        return;
    }
    // The job owns its document instance: opening the SNAPSHOT path here can
    // never touch the view's live document, and the view closing, switching
    // or reloading mid-print cannot touch this one.
    m_doc = new QPdfDocument(this);
    if (m_doc->load(m_snapshotPath) != QPdfDocument::Error::None) {
        m_failed = true;
        m_error = QObject::tr("The document could not be opened for printing: %1")
                      .arg(m_snapshotPath);
        finish();
        return;
    }

    m_progress = new QProgressDialog(
        QObject::tr("Printing pages…"), QObject::tr("Cancel"),
        0, qMax<int>(m_pages.size(), 1), m_dialogParent);
    m_progress->setWindowModality(Qt::WindowModal);
    m_progress->setMinimumDuration(0);
    // QProgressDialog auto-closes at maximum and its closeEvent emits
    // canceled() — that would flip a COMPLETED print into a "canceled"
    // result. We close the dialog ourselves in finish().
    m_progress->setAutoClose(false);
    m_progress->setAutoReset(false);
    m_progress->setValue(0);
    connect(m_progress, &QProgressDialog::canceled, this, &PrintJob::cancel);

    renderNextPage();
}

void PrintJob::renderNextPage()
{
    if (m_canceled.load() || m_failed) {
        finish();
        return;
    }
    if (m_index >= m_pages.size()) {
        finish();
        return;
    }

    // One worker thread per page (sequential), capturing ONLY job-owned state:
    // the raw live view document of the old implementation cannot be
    // use-after-closed here because no view object participates at all.
    const int page = m_pages.at(m_index);
    QThread* worker = QThread::create([this, page] {
        const QSizeF pageSize = m_doc->pagePointSize(page);
        const QSize imageSize(int(pageSize.width() * 3.0), int(pageSize.height() * 3.0));
        QImage rendered = m_doc->render(page, imageSize, QPdfDocumentRenderOptions());
        m_rendered = new QImage(rendered);
        QMetaObject::invokeMethod(this, &PrintJob::paintRenderedPage, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void PrintJob::paintRenderedPage()
{
    if (m_canceled.load() || m_failed) {
        finish();
        return;
    }
    if (!m_rendered) {  // cannot happen (worker always sets it) — defensive
        finish();
        return;
    }
    std::unique_ptr<QImage> rendered(m_rendered);
    m_rendered = nullptr;

    if (rendered->isNull()) {
        m_failed = true;
        m_error = QObject::tr("Page %1 could not be rendered for printing.")
                      .arg(m_pages.at(m_index) + 1);
        finish();
        return;
    }

    // The painter is created on the first page that actually paints: a
    // printer that could not be opened fails HERE, visibly (never a silent
    // zero-page "success").
    if (!m_painter) {
        m_painter = new QPainter();
        if (!m_painter->begin(m_printer) || !m_painter->isActive()) {
            m_failed = true;
            m_error = QObject::tr("The printer could not be started for printing.");
            finish();
            return;
        }
    }

    if (m_printed > 0 && !m_printer->newPage()) {
        m_failed = true;
        m_error = QObject::tr("The printer did not accept the next page (spool error).");
        finish();
        return;
    }

    const QRect target = m_painter->viewport();
    const QSize scaledSize = rendered->size().scaled(target.size(), Qt::KeepAspectRatio);
    const QRect centered((target.width() - scaledSize.width()) / 2,
                         (target.height() - scaledSize.height()) / 2,
                         scaledSize.width(), scaledSize.height());
    m_painter->drawImage(centered, *rendered);
    ++m_printed;
    ++m_index;
    if (m_progress)
        m_progress->setValue(m_printed);

    renderNextPage();
}

void PrintJob::finish()
{
    if (m_finishedEmitted)
        return;
    m_finishedEmitted = true;

    if (m_painter) {
        if (m_painter->isActive())
            m_painter->end();
        delete m_painter;
        m_painter = nullptr;
    }
    // Spool-level failure check AFTER the paint session ends and BEFORE the
    // result is reported (a PdfFormat printer writes its file on destruction —
    // the output is complete before finished() reports success).
    const bool printerError =
        m_printer && m_printer->printerState() == QPrinter::Error;
    if (printerError && !m_failed) {
        m_failed = true;
        m_error = QObject::tr("The printer reported an error after printing.");
    }
    delete m_printer;
    m_printer = nullptr;

    if (m_progress) {
        // The dialog must not fire canceled() into an already-final job.
        disconnect(m_progress, nullptr, this, nullptr);
        m_progress->close();
        m_progress->deleteLater();
        m_progress = nullptr;
    }

    emit finished(m_printed, m_canceled.load(), m_error);
}

} // namespace gp
