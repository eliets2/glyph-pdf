#include "engines/DiffEngine.h"
#include "engines/pdfium/PdfiumBackend.h"
#include <QFile>
#include <QCryptographicHash>
#include <QColor>
#include <QSet>
#include <QDebug>
#include <QRegularExpression>

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

        // Text diff (simple per-word set diff for now as placeholder for Myers)
        QString text1 = backend1.extractText(i);
        QString text2 = backend2.extractText(i);

        QStringList words1 = text1.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        QStringList words2 = text2.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
        QSet<QString> set1 = QSet<QString>::fromList(words1);
        QSet<QString> set2 = QSet<QString>::fromList(words2);
#else
        QSet<QString> set1(words1.begin(), words1.end());
        QSet<QString> set2(words2.begin(), words2.end());
#endif

        for (const QString &w : set1) {
            if (!set2.contains(w)) pd.textRemoved.append(w);
        }
        for (const QString &w : set2) {
            if (!set1.contains(w)) pd.textAdded.append(w);
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

    return result;
}
