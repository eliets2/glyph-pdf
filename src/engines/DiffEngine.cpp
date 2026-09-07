// SPDX-License-Identifier: Apache-2.0
#include "engines/DiffEngine.h"
#include "engines/MyersDiff.h"
#include "engines/pdfium/PdfiumBackend.h"
#include <QFile>
#include <QCryptographicHash>
#include <QColor>
#include <QDebug>
#include <QRegularExpression>
#include <QSet>
#include <QVector>
#include <algorithm>

DiffEngine::DiffEngine() {}
DiffEngine::~DiffEngine() {}

DiffResult DiffEngine::compare(const QString &file1, const QString &file2, int dpi) {
    DiffResult result;
    result.isIdentical = false;

    QFile f1(file1);
    QFile f2(file2);
    if (!f1.open(QIODevice::ReadOnly) || !f2.open(QIODevice::ReadOnly)) {
        return result;
    }

    QByteArray hash1 = QCryptographicHash::hash(f1.readAll(), QCryptographicHash::Sha256);
    QByteArray hash2 = QCryptographicHash::hash(f2.readAll(), QCryptographicHash::Sha256);
    
    if (hash1 == hash2) {
        result.isIdentical = true;
        return result;
    }

    PdfiumBackend backend1;
    PdfiumBackend backend2;

    if (!backend1.loadDocument(file1) || !backend2.loadDocument(file2)) {
        return result;
    }

    result.pageCount1 = backend1.pageCount();
    result.pageCount2 = backend2.pageCount();

    int minPages = qMin(result.pageCount1, result.pageCount2);

    for (int i = 0; i < minPages; ++i) {
        PageDiff pd;
        pd.pageIndex = i;
        pd.pixelDiffCount = 0;

        // Text diff via Myers 1986 LCS + move-detection post-pass.
        const QString text1 = backend1.extractText(i);
        const QString text2 = backend2.extractText(i);
        const QStringList words1 = text1.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        const QStringList words2 = text2.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        const QList<EditOp> edits = MyersDiff::compute(words1, words2);
        pd.moves = MyersDiff::detectMoves(edits);

        // Populate textRemoved / textAdded from non-move edits for backward compat.
        // Build sets of moved tokens so we can exclude them.
        QHash<QString, int> movedFromA, movedToB;
        for (const MoveOperation& mv : pd.moves) {
            movedFromA[mv.token]++;
            movedToB[mv.token]++;
        }
        QHash<QString, int> excludeA = movedFromA;
        QHash<QString, int> excludeB = movedToB;

        for (const EditOp& op : edits) {
            if (op.type == EditOp::Type::Delete) {
                auto it = excludeA.find(op.token);
                if (it != excludeA.end() && it.value() > 0) {
                    --(it.value());  // consume one move slot
                } else {
                    pd.textRemoved.append(op.token);
                }
            } else if (op.type == EditOp::Type::Insert) {
                auto it = excludeB.find(op.token);
                if (it != excludeB.end() && it.value() > 0) {
                    --(it.value());
                } else {
                    pd.textAdded.append(op.token);
                }
            }
        }

        // Pixel diff
        QImage img1 = backend1.renderPage(i, dpi);
        QImage img2 = backend2.renderPage(i, dpi);

        if (!img1.isNull() && !img2.isNull()) {
            int w = qMax(img1.width(), img2.width());
            int h = qMax(img1.height(), img2.height());

            QImage diffImg(w, h, QImage::Format_ARGB32);
            diffImg.fill(Qt::transparent);

            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    bool in1 = (x < img1.width() && y < img1.height());
                    bool in2 = (x < img2.width() && y < img2.height());
                    
                    if (in1 && in2) {
                        QRgb p1 = img1.pixel(x, y);
                        QRgb p2 = img2.pixel(x, y);

                        int rDiff = qAbs(qRed(p1) - qRed(p2));
                        int gDiff = qAbs(qGreen(p1) - qGreen(p2));
                        int bDiff = qAbs(qBlue(p1) - qBlue(p2));

                        // Antialiasing threshold
                        if (rDiff > 30 || gDiff > 30 || bDiff > 30) {
                            diffImg.setPixel(x, y, qRgba(255, 0, 0, 150)); // Red overlay
                            pd.pixelDiffCount++;
                        }
                    } else if (in1 || in2) {
                        diffImg.setPixel(x, y, qRgba(255, 0, 0, 150));
                        pd.pixelDiffCount++;
                    }
                }
            }
            pd.diffImage = diffImg;
        }

        result.pages.append(pd);
    }

    // ── Page-level structural alignment ─────────────────────────────────────
    // U04/R11 follow-up: middle insertions must surface as ONE PageAdded at
    // their true position (surrounding pages matched), never as remove+add
    // chains. Two-stage alignment:
    //
    // 1. Exact fingerprints (primary). Every page's extracted text is
    //    normalized (lowercased, whitespace collapsed, first 200 chars — the
    //    same normalization the excerpts use) and hashed with SHA-256. The
    //    classic order-preserving LCS DP then aligns pages whose fingerprints
    //    match byte-for-byte; every page is consumed at most once.
    //    Deterministic tie-break: the backtrack prefers doc1-side skips
    //    (dp[i-1][j] >= dp[i][j-1]), which aligns the LATEST doc2 occurrence
    //    of a repeated fingerprint — so the EARLIEST unmatched doc2
    //    occurrence is the one that surfaces as PageAdded (pinned by
    //    duplicateOfExistingPageInsertionPinsDeterministicTieBreak).
    //    A page whose extraction yields no text (image-only page or a
    //    genuinely blank page) has NO fingerprint: it can never take part in
    //    this pass and falls through to stage 2 unchanged.
    //
    // 2. Fuzzy fallback (pre-fingerprint behavior, unchanged). Pages the
    //    exact alignment left over may still match on word-set similarity
    //    (Jaccard ≥ 80%): a leftover pair at a DIFFERENT index becomes one
    //    PageMoved (never re-reported as add+remove), a same-index pair
    //    realigns without a move record, and fingerprint-less pages keep the
    //    index-wise/blank-page semantics they had before fingerprints.
    // 3. Substitution (V04). Leftover pairs the first two stages could not
    //    anchor pair up in order as ALIGNED MODIFIED pages — structurally
    //    silent, their content changes live in result.pages. Only the
    //    one-sided remainder is PageRemoved (doc1) / PageAdded (doc2).
    {
        auto fingerprint = [](const QString& text) -> QString {
            QString t = text.left(200).toLower();
            t.replace(QRegularExpression("\\s+"), QStringLiteral(" "));
            return t.trimmed();
        };
        auto wordSet = [](const QString& fp) -> QSet<QString> {
            QSet<QString> s;
            const auto parts = fp.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (const QString& w : parts) s.insert(w);
            return s;
        };
        auto similarity = [](const QSet<QString>& a, const QSet<QString>& b) -> double {
            if (a.isEmpty() && b.isEmpty()) return 1.0;
            if (a.isEmpty() || b.isEmpty()) return 0.0;
            int inter = 0;
            for (const QString& w : a) if (b.contains(w)) ++inter;
            const int uni = a.size() + b.size() - inter;
            return uni > 0 ? double(inter) / double(uni) : 0.0;
        };
        constexpr double kSame = 0.80;

        const int n1 = result.pageCount1;
        const int n2 = result.pageCount2;

        QStringList fp1, fp2;
        QList<QByteArray> fpHash1, fpHash2;   // empty = no fingerprint (no text)
        QList<QSet<QString>> ws1, ws2;
        for (int i = 0; i < n1; ++i) {
            const QString norm = fingerprint(backend1.extractText(i));
            fp1 << norm;
            fpHash1 << (norm.isEmpty()
                          ? QByteArray()
                          : QCryptographicHash::hash(norm.toUtf8(),
                                                     QCryptographicHash::Sha256));
            ws1 << wordSet(norm);
        }
        for (int j = 0; j < n2; ++j) {
            const QString norm = fingerprint(backend2.extractText(j));
            fp2 << norm;
            fpHash2 << (norm.isEmpty()
                          ? QByteArray()
                          : QCryptographicHash::hash(norm.toUtf8(),
                                                     QCryptographicHash::Sha256));
            ws2 << wordSet(norm);
        }

        // Exact matches only: both sides must carry a fingerprint (an empty
        // hash means "extraction produced no text") and the hashes must be
        // byte-equal. This is what keeps a middle insertion — even a
        // boilerplate near-twin of an adjacent page — from being absorbed
        // into a shifted alignment.
        auto exactMatch = [](const QByteArray& a, const QByteArray& b) -> bool {
            return !a.isEmpty() && !b.isEmpty() && a == b;
        };

        // LCS over page sequences (equal := exact fingerprint match).
        QVector<QVector<int>> dp(n1 + 1, QVector<int>(n2 + 1, 0));
        for (int i = 1; i <= n1; ++i)
            for (int j = 1; j <= n2; ++j)
                dp[i][j] = exactMatch(fpHash1[i - 1], fpHash2[j - 1])
                    ? dp[i - 1][j - 1] + 1
                    : qMax(dp[i - 1][j], dp[i][j - 1]);

        // Backtrack to mark the aligned (stable) pages.
        QSet<int> alignedA, alignedB;
        for (int i = n1, j = n2; i > 0 && j > 0; ) {
            if (exactMatch(fpHash1[i - 1], fpHash2[j - 1])
                && dp[i][j] == dp[i - 1][j - 1] + 1) {
                alignedA.insert(i - 1);
                alignedB.insert(j - 1);
                --i; --j;
            } else if (dp[i - 1][j] >= dp[i][j - 1]) {
                --i;
            } else {
                --j;
            }
        }

        // Fuzzy fallback over the exact alignment's leftovers: a doc2 page
        // outside the alignment that still fuzzy-matches (word-set similarity
        // ≥ kSame) an (also-unaligned) doc1 page at a different index is a
        // moved page. A match at the SAME index (repeated-page tie-break, or
        // a fingerprint-less page pair) is consumed as an aligned pair
        // without a move record, so a matched pair is never re-reported as an
        // add+remove below (R11: no double counting).
        QSet<int> movedB;
        for (int b = 0; b < n2; ++b) {
            if (alignedB.contains(b)) continue;
            int    bestA   = -1;
            double bestSim = 0.0;
            for (int a = 0; a < n1; ++a) {
                if (alignedA.contains(a)) continue;
                const double s = similarity(ws1[a], ws2[b]);
                if (s >= kSame && s > bestSim) { bestSim = s; bestA = a; }
            }
            if (bestA >= 0) {
                alignedA.insert(bestA);  // consume so it isn't reused
                if (bestA != b) {
                    DiffResult::PageMove mv;
                    mv.fromPage = bestA;
                    mv.toPage   = b;
                    mv.excerpt  = fp2[b].left(60);
                    result.pageMoves.append(mv);
                    movedB.insert(b);
                } else {
                    alignedB.insert(b);  // same-index fallback pair: aligned, not moved
                }
            }
        }

        // ── V04: aligned modified-page pairs are not structural changes ────
        // A page left unmatched by the exact-fingerprint alignment AND the
        // fuzzy move pass is not proof that the page structure changed: a
        // one-page document whose text was rewritten in place ("Apple" →
        // "Orange") used to surface as PageRemoved + PageAdded on top of its
        // ordinary text difference, because word-set similarity 0.0 misses
        // the fuzzy floor. Explicit substitution policy (the sequence-context
        // / change-hunk semantics of a page-level diff): the k-th leftover of
        // doc1 pairs with the k-th leftover of doc2, order-preserving, as one
        // ALIGNED MODIFIED pair. Modified pairs are deliberately NOT entries
        // in pageChanges — their content changes are already reported per
        // page in result.pages, which the CHANGES tree, the change-type
        // filters, the navigation sequence and the exported reports all
        // render as ordinary page rows, so every consumer agrees on the
        // classification. Surplus leftovers keep their one-sided
        // PageRemoved / PageAdded. Anchors are untouched: this pass runs
        // after exact fingerprints and fuzzy moves, so real insertions
        // (middle insertions, duplicates) and reorders keep their pinned
        // classifications. Documented ambiguity: when an insertion and an
        // in-place rewrite coexist with no anchor between them, the in-order
        // pairing may attach the rewrite to the insertion — the per-page
        // word diff stays authoritative in either reading.
        QList<int> leftoverA, leftoverB;
        for (int a = 0; a < n1; ++a)
            if (!alignedA.contains(a)) leftoverA.append(a);
        for (int b = 0; b < n2; ++b)
            if (!alignedB.contains(b) && !movedB.contains(b)) leftoverB.append(b);
        const int nModified = qMin(leftoverA.size(), leftoverB.size());
        for (int k = 0; k < nModified; ++k) {
            alignedA.insert(leftoverA.at(k));
            alignedB.insert(leftoverB.at(k));
        }

        // ── R11: explicit structural changes for everything the alignment ─────
        // left unmatched. Pages present on exactly one side are added/removed
        // (missing side = -1, never a page-zero sentinel); pages consumed by
        // the move pass appear here once as PageMoved. pageChanges is the
        // single canonical sequence read by the CHANGES tree, the change-type
        // filters, the next/previous sequence, the status totals and the
        // exported reports; pageMoves stays populated for backward compat.
        for (const auto& mv : result.pageMoves) {
            DiffResult::PageChange ch;
            ch.type    = DiffResult::PageChangeType::PageMoved;
            ch.oldPage = mv.fromPage;
            ch.newPage = mv.toPage;
            ch.excerpt = mv.excerpt;
            result.pageChanges.append(ch);
        }
        for (int a = 0; a < n1; ++a) {
            if (alignedA.contains(a)) continue;
            DiffResult::PageChange ch;
            ch.type    = DiffResult::PageChangeType::PageRemoved;  // doc1 only
            ch.oldPage = a;
            ch.excerpt = fp1[a].left(60);
            result.pageChanges.append(ch);
        }
        for (int b = 0; b < n2; ++b) {
            if (alignedB.contains(b) || movedB.contains(b)) continue;
            DiffResult::PageChange ch;
            ch.type    = DiffResult::PageChangeType::PageAdded;    // doc2 only
            ch.newPage = b;
            ch.excerpt = fp2[b].left(60);
            result.pageChanges.append(ch);
        }
        if (!result.pageChanges.isEmpty()) {
            result.isIdentical = false;  // surplus pages are changes (defensive)
            // Deterministic reading order: anchor each change on the side where
            // the page lives (doc2 position when it exists, doc1 otherwise).
            std::stable_sort(result.pageChanges.begin(), result.pageChanges.end(),
                             [](const DiffResult::PageChange& l,
                                const DiffResult::PageChange& r) {
                                 const int la = l.hasNewSide() ? l.newPage : l.oldPage;
                                 const int ra = r.hasNewSide() ? r.newPage : r.oldPage;
                                 return la < ra;
                             });
        }
    }

    return result;
}
