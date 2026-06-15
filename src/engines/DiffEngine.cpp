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

    // ── Page-level reorder detection ────────────────────────────────────────
    // Fingerprint each page (first 200 chars), then run an O(n²) LCS over the
    // two page sequences where two pages are "equal" if their word-set
    // similarity is ≥ 80%. Pages outside the aligned common subsequence that
    // still match a page in the other document at a different index are moves.
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
        QList<QSet<QString>> ws1, ws2;
        for (int i = 0; i < n1; ++i) { fp1 << fingerprint(backend1.extractText(i)); ws1 << wordSet(fp1.last()); }
        for (int j = 0; j < n2; ++j) { fp2 << fingerprint(backend2.extractText(j)); ws2 << wordSet(fp2.last()); }

        // LCS over page sequences (equal := similarity ≥ kSame).
        QVector<QVector<int>> dp(n1 + 1, QVector<int>(n2 + 1, 0));
        for (int i = 1; i <= n1; ++i)
            for (int j = 1; j <= n2; ++j)
                dp[i][j] = (similarity(ws1[i - 1], ws2[j - 1]) >= kSame)
                    ? dp[i - 1][j - 1] + 1
                    : qMax(dp[i - 1][j], dp[i][j - 1]);

        // Backtrack to mark the aligned (stable) pages.
        QSet<int> alignedA, alignedB;
        for (int i = n1, j = n2; i > 0 && j > 0; ) {
            if (similarity(ws1[i - 1], ws2[j - 1]) >= kSame && dp[i][j] == dp[i - 1][j - 1] + 1) {
                alignedA.insert(i - 1);
                alignedB.insert(j - 1);
                --i; --j;
            } else if (dp[i - 1][j] >= dp[i][j - 1]) {
                --i;
            } else {
                --j;
            }
        }

        // A doc2 page outside the alignment that still matches an (also-unaligned)
        // doc1 page at a different index is a moved page.
        for (int b = 0; b < n2; ++b) {
            if (alignedB.contains(b)) continue;
            int    bestA   = -1;
            double bestSim = 0.0;
            for (int a = 0; a < n1; ++a) {
                if (alignedA.contains(a)) continue;
                const double s = similarity(ws1[a], ws2[b]);
                if (s >= kSame && s > bestSim) { bestSim = s; bestA = a; }
            }
            if (bestA >= 0 && bestA != b) {
                DiffResult::PageMove mv;
                mv.fromPage = bestA;
                mv.toPage   = b;
                mv.excerpt  = fp2[b].left(60);
                result.pageMoves.append(mv);
                alignedA.insert(bestA);  // consume so it isn't reused
            }
        }
    }

    return result;
}
