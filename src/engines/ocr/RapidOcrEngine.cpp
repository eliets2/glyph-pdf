#include "engines/ocr/RapidOcrEngine.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QRecursiveMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QFile>

#ifdef HAS_RAPIDOCR
#include <onnxruntime_cxx_api.h>
#endif

class RapidOcrEngine::Private {
public:
#ifdef HAS_RAPIDOCR
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::SessionOptions> sessionOptions;
    std::unique_ptr<Ort::Session> detSession;
    std::unique_ptr<Ort::Session> clsSession;
    std::unique_ptr<Ort::Session> recSession;
#endif
    bool isInitialized = false;
    QString language = "eng";
    QString modelPath;
    QRecursiveMutex mutex;
};

RapidOcrEngine::RapidOcrEngine() : d(std::make_unique<Private>())
{
}

RapidOcrEngine::~RapidOcrEngine() = default;

bool RapidOcrEngine::initialize(const QString &language, const QString &dataPath)
{
    QMutexLocker locker(&d->mutex);

    QString targetDataPath = dataPath;
    if (targetDataPath.isEmpty()) {
        targetDataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/models/ppocrv5";
    }
    
    if (d->isInitialized && d->language == language && d->modelPath == targetDataPath) {
        return true;
    }

    d->language = language;
    d->modelPath = targetDataPath;

#ifdef HAS_RAPIDOCR
    try {
        d->env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "RapidOcrEngine");
        d->sessionOptions = std::make_unique<Ort::SessionOptions>();
        d->sessionOptions->SetIntraOpNumThreads(2);
        d->sessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        QString detModel = QFileInfo(QDir(targetDataPath), "ch_PP-OCRv4_det_infer.onnx").absoluteFilePath();
        QString clsModel = QFileInfo(QDir(targetDataPath), "ch_ppocr_mobile_v2.0_cls_infer.onnx").absoluteFilePath();
        QString recModel = QFileInfo(QDir(targetDataPath), "ch_PP-OCRv4_rec_infer.onnx").absoluteFilePath();

        // Normally we'd load the sessions here. We just check if files exist to avoid throwing if models are missing
        if (QFile::exists(detModel)) {
#ifdef _WIN32
            d->detSession = std::make_unique<Ort::Session>(*d->env, detModel.toStdWString().c_str(), *d->sessionOptions);
#else
            d->detSession = std::make_unique<Ort::Session>(*d->env, detModel.toUtf8().constData(), *d->sessionOptions);
#endif
        } else {
            qWarning() << "RapidOCR Model not found:" << detModel;
            return false;
        }

        if (QFile::exists(clsModel)) {
#ifdef _WIN32
            d->clsSession = std::make_unique<Ort::Session>(*d->env, clsModel.toStdWString().c_str(), *d->sessionOptions);
#else
            d->clsSession = std::make_unique<Ort::Session>(*d->env, clsModel.toUtf8().constData(), *d->sessionOptions);
#endif
        }

        if (QFile::exists(recModel)) {
#ifdef _WIN32
            d->recSession = std::make_unique<Ort::Session>(*d->env, recModel.toStdWString().c_str(), *d->sessionOptions);
#else
            d->recSession = std::make_unique<Ort::Session>(*d->env, recModel.toUtf8().constData(), *d->sessionOptions);
#endif
        } else {
            qWarning() << "RapidOCR Model not found:" << recModel;
            return false;
        }

        d->isInitialized = true;
        return true;
    } catch (const Ort::Exception& e) {
        qWarning() << "ONNXRuntime exception during initialization:" << e.what();
        return false;
    }
#else
    qWarning() << "RapidOCR not available — built without onnxruntime";
    return false;
#endif
}

QList<OcrResult> RapidOcrEngine::processImage(const QImage &image)
{
    QList<OcrResult> results;
#ifdef HAS_RAPIDOCR
    QMutexLocker locker(&d->mutex);
    if (!d->isInitialized) return results;

    // STUB: Actual pre/post processing of tensors for PP-OCRv5 involves DBNet box extraction,
    // perspective transform, SVTR recognition, and CTCLoss decoding.
    // For this boilerplate, we'd feed `image` to `d->detSession`, get map, extract polygons,
    // crop each polygon, pass to `d->clsSession`, then `d->recSession` to get text.
    
    // We mock the return to let the pipeline work.
    OcrResult res;
    res.text = "RapidOCR_Mock";
    res.boundingBox = QRectF(0, 0, 100, 20);
    res.confidence = 85;
    results.append(res);
#else
    Q_UNUSED(image)
#endif
    return results;
}

QString RapidOcrEngine::getRawText(const QImage &image)
{
    QList<OcrResult> results = processImage(image);
    QStringList texts;
    for (const auto &r : results) {
        texts.append(r.text);
    }
    return texts.join(" ");
}
