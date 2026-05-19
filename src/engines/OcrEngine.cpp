#include "engines/OcrEngine.h"
#include <memory>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QDir>
#include <QFileInfo>
#include <QRecursiveMutex>
#include <QMutexLocker>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QFile>
#include <QTimer>
#include <QUrl>
#include <QImage>
#include <QThread>
#include <QCoreApplication>
#ifdef HAS_TESSERACT
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#endif
#include <cstring>

namespace {

constexpr int DownloadTimeoutMs = 15000;
constexpr qsizetype MaxTrainedDataBytes = 100 * 1024 * 1024;

const QSet<QString> &allowedLanguages()
{
    static const QSet<QString> languages = {
        "ara", "chi_sim", "chi_tra", "deu", "eng", "fra", "ita",
        "jpn", "kor", "nld", "osd", "por", "rus", "spa"
    };
    return languages;
}

QString normalizedLanguage(const QString &language)
{
    return language.trimmed().toLower();
}

bool downloadTrainedData(const QString &language, const QString &filepath)
{
    const QString filename = language + ".traineddata";
    const QUrl url(QString("https://raw.githubusercontent.com/tesseract-ocr/tessdata_best/main/%1").arg(filename));
    QNetworkAccessManager networkManager;
    QNetworkRequest request(url);
    request.setTransferTimeout(DownloadTimeoutMs);
    request.setMaximumRedirectsAllowed(0);

    QNetworkReply *reply = networkManager.get(request);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    timeout.start(DownloadTimeoutMs);
    loop.exec();

    if (!reply->isFinished()) {
#ifdef QT_DEBUG
        qWarning() << "Timed out downloading OCR language pack:" << language;
#endif
        reply->deleteLater();
        return false;
    }

    if (reply->error() != QNetworkReply::NoError) {
#ifdef QT_DEBUG
        qWarning() << "Failed to download OCR language pack:" << reply->errorString();
#endif
        reply->deleteLater();
        return false;
    }

    const QByteArray payload = reply->readAll();
    reply->deleteLater();

    if (payload.isEmpty() || payload.size() > MaxTrainedDataBytes) {
#ifdef QT_DEBUG
        qWarning() << "Rejected OCR language pack with unexpected size:" << payload.size();
#endif
        return false;
    }

    QSaveFile file(filepath);
    if (!file.open(QIODevice::WriteOnly)) {
#ifdef QT_DEBUG
        qWarning() << "Could not write OCR language pack:" << filepath << file.errorString();
#endif
        return false;
    }

    if (file.write(payload) != payload.size()) {
#ifdef QT_DEBUG
        qWarning() << "Could not write complete OCR language pack:" << filepath;
#endif
        return false;
    }

    if (!file.commit()) {
#ifdef QT_DEBUG
        qWarning() << "Could not commit OCR language pack:" << filepath << file.errorString();
#endif
        return false;
    }

    return true;
}

} // namespace

class OcrEngine::Private {
public:
#ifdef HAS_TESSERACT
    struct TessDeleter {
        void operator()(tesseract::TessBaseAPI* ptr) const {
            if (ptr) {
                ptr->End();
                delete ptr;
            }
        }
    };
    std::unique_ptr<tesseract::TessBaseAPI, TessDeleter> api;
#endif
    bool isInitialized = false;
    QString language = "eng";
    QString tessDataPath;
    QRecursiveMutex mutex;
};

OcrEngine::OcrEngine() : d(std::make_unique<Private>())
{
#ifdef HAS_TESSERACT
    d->api = std::unique_ptr<tesseract::TessBaseAPI, Private::TessDeleter>(new tesseract::TessBaseAPI());
#endif
}

OcrEngine::~OcrEngine() = default;

bool OcrEngine::initialize(const QString &language, const QString &dataPath)
{
    if (QThread::currentThread() == QCoreApplication::instance()->thread()) {
        qWarning() << "CRITICAL: OcrEngine::initialize() called from main GUI thread! Blocking operations must run on worker threads.";
        return false;
    }

    if (dataPath.contains("..")) {
        qWarning() << "SECURITY: Rejected OCR data path containing directory traversal sequences (..).";
        return false;
    }

    const QString safeLanguage = normalizedLanguage(language);
    if (!allowedLanguages().contains(safeLanguage)) {
#ifdef QT_DEBUG
        qWarning() << "Rejected unsupported OCR language:" << language;
#endif
        return false;
    }

    QMutexLocker locker(&d->mutex);
    if (d->isInitialized && d->language == safeLanguage && (dataPath.isEmpty() || d->tessDataPath == dataPath)) {
        return true;
    }

    QString targetDataPath = dataPath;
    if (targetDataPath.isEmpty()) {
        targetDataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/tessdata";
    }
    targetDataPath = QDir::cleanPath(targetDataPath);

    // STRICT SECURITY PATH VALIDATION
    const QString appDataDir = QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    const QString targetAbs = QDir::cleanPath(QFileInfo(targetDataPath).absoluteFilePath());
    if (!targetAbs.startsWith(appDataDir)) {
        qWarning() << "SECURITY: Rejected OCR data path outside the application data directory:" << targetAbs;
        return false;
    }

    d->tessDataPath = targetDataPath;
    d->language = safeLanguage;

    QDir dir(targetDataPath);
    if (!dir.exists() && !dir.mkpath(".")) {
#ifdef QT_DEBUG
        qWarning() << "Could not create OCR data directory:" << targetDataPath;
#endif
        return false;
    }

    const QString filename = safeLanguage + ".traineddata";
    const QString filepath = QFileInfo(dir, filename).absoluteFilePath();

    if (!QFile::exists(filepath)) {
#ifdef QT_DEBUG
        qDebug() << "OCR language pack not found locally. Downloading:" << safeLanguage;
#endif
        if (!downloadTrainedData(safeLanguage, filepath)) {
            return false;
        }
    }

#ifdef HAS_TESSERACT
    if (d->isInitialized) {
        d->api->End();
        d->isInitialized = false;
    }

    if (d->api->Init(targetDataPath.toUtf8().constData(), safeLanguage.toUtf8().constData())) {
#ifdef QT_DEBUG
        qWarning() << "Could not initialize tesseract with language:" << safeLanguage;
#endif
        return false;
    }

#ifdef QT_DEBUG
    qDebug() << "Tesseract OCR API Initialized with language:" << safeLanguage;
#endif
    d->isInitialized = true;
    return true;
#else
#ifdef QT_DEBUG
    qWarning() << "OCR not available — built without Tesseract";
#endif
    return false;
#endif
}

QList<OcrResult> OcrEngine::processImage(const QImage &image)
{
    QList<OcrResult> results;
    if (image.isNull()) return results;
    if (image.width() > 10000 || image.height() > 10000) {
        qWarning() << "SECURITY: Rejected oversized OCR image matrix to prevent memory exhaustion.";
        return results;
    }

#ifdef HAS_TESSERACT
    QMutexLocker locker(&d->mutex);
    if (!d->isInitialized && !initialize()) return results;
    if (!d->isInitialized || !d->api) return results;

    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    Pix *pix = pixCreate(gray.width(), gray.height(), 8);
    if (!pix) return results;
    for (int y = 0; y < gray.height(); ++y) {
        l_uint32 *data = pixGetData(pix);
        l_int32 wpl = pixGetWpl(pix);
        std::memcpy(reinterpret_cast<uint8_t*>(data) + y * wpl * sizeof(l_uint32),
                     gray.scanLine(y), gray.width());
    }

    d->api->SetImage(pix);
    d->api->Recognize(0);

    tesseract::ResultIterator* ri = d->api->GetIterator();
    tesseract::PageIteratorLevel level = tesseract::RIL_WORD;

    if (ri != nullptr) {
        do {
            const char* word = ri->GetUTF8Text(level);
            if (word) {
                float conf = ri->Confidence(level);
                int x1, y1, x2, y2;
                ri->BoundingBox(level, &x1, &y1, &x2, &y2);

                OcrResult res;
                res.text = QString::fromUtf8(word);
                res.boundingBox = QRectF(x1, y1, x2 - x1, y2 - y1);
                res.confidence = static_cast<int>(conf);
                results.append(res);

                delete[] word;
            }
        } while (ri->Next(level));
    }

    pixDestroy(&pix);
#else
    Q_UNUSED(image)
#endif
    return results;
}

QString OcrEngine::getRawText(const QImage &image)
{
    if (image.isNull()) return QString();
    if (image.width() > 10000 || image.height() > 10000) {
        qWarning() << "SECURITY: Rejected oversized OCR image matrix to prevent memory exhaustion.";
        return QString();
    }

#ifdef HAS_TESSERACT
    QMutexLocker locker(&d->mutex);
    if (!d->isInitialized && !initialize()) return QString();
    if (!d->isInitialized || !d->api) return QString();

    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    Pix *pix = pixCreate(gray.width(), gray.height(), 8);
    if (!pix) return QString();
    for (int y = 0; y < gray.height(); ++y) {
        l_uint32 *data = pixGetData(pix);
        l_int32 wpl = pixGetWpl(pix);
        std::memcpy(reinterpret_cast<uint8_t*>(data) + y * wpl * sizeof(l_uint32),
                     gray.scanLine(y), gray.width());
    }

    d->api->SetImage(pix);
    char* outText = d->api->GetUTF8Text();
    QString result = QString::fromUtf8(outText);

    pixDestroy(&pix);
    delete[] outText;

    return result;
#else
    Q_UNUSED(image)
    return QString();
#endif
}
