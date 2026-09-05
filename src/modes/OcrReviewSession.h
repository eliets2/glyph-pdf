// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QImage>
#include <QList>
#include <QRectF>
#include <QString>

namespace gp {

/// R08 (F04): one reviewed OCR word.
///
/// stableId is the index of the word inside the OCR run's delivered word list;
/// it is the stable identity used by the review interaction and by acceptance
/// to match reviewed records back to the recognized session. boundingBox is
/// the ORIGINAL source box in pageImage pixel coordinates — review edits never
/// move, split or resize it (arbitrary edited text is never force-zipped
/// against original boxes; there are no invented coordinates).
struct OcrReviewedWord {
    int     stableId     = -1;
    QString originalText;    // text as recognized
    QString reviewedText;    // current reviewed text (== originalText until edited;
                             // empty/whitespace means the word was removed)
    bool    deleted = false; // user removed the word from the review
    QRectF  boundingBox;     // source box (pageImage pixel space) — immutable
    int     confidence  = 0;
    QString sourceEngine;    // provenance of the ORIGINAL recognition
};

/// R07/R08: the review session that travels with acceptance into the
/// searchable-PDF export. Built by EditController when an OCR job delivers
/// (source identity + revision + original image + words with stable IDs); the
/// review panel attaches per-word reviewed text through OcrReviewedWord
/// records; acceptance re-validates the identity against the live viewer
/// before exporting.
struct OcrReviewSession {
    qint64  generation      = -1;    // job generation that produced the words
    QString sourcePath;              // source-document identity
    int     sourcePage      = -1;    // 0-based page the words were recognized on
    int     sourcePageCount = -1;    // cheap revision proxy (page insert/delete)
    QImage  pageImage;               // image the words were recognized from;
                                     // word boxes are in this image's pixel space
    QList<OcrReviewedWord> words;

    bool isValid() const
    {
        return generation >= 0 && !sourcePath.isEmpty() && sourcePage >= 0
            && !pageImage.isNull();
    }
};

} // namespace gp
