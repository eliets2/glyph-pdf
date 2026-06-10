// SPDX-License-Identifier: Apache-2.0
#include "engines/ocr/RapidOcrEngine.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QRecursiveMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QFile>
#include <QCoreApplication>

#include "engines/ocr/PpOcrDecoder.h"

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
    PpOcrDecoder decoder;
    bool decoderReady = false;
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
        // Resolution order: installed location (AppLocalData), then next to the
        // executable (dev/test builds copy models/ppocrv5 beside the binary).
        const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/models/ppocrv5";
        const QString nextToExe = QCoreApplication::applicationDirPath() + "/models/ppocrv5";
        targetDataPath = QFile::exists(appData + "/PP-OCRv5_mobile_det_infer.onnx") ? appData : nextToExe;
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

        QString detModel = QFileInfo(QDir(targetDataPath), "PP-OCRv5_mobile_det_infer.onnx").absoluteFilePath();
        QString clsModel = QFileInfo(QDir(targetDataPath), "PP-LCNet_x1_0_textline_ori_infer.onnx").absoluteFilePath();
        QString recModel = QFileInfo(QDir(targetDataPath), "PP-OCRv5_mobile_rec_infer.onnx").absoluteFilePath();

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

        // Wire the warm sessions into the decoder + load the recognition vocabulary.
        d->decoder.setSessions(d->detSession.get(),
                               d->clsSession ? d->clsSession.get() : nullptr,
                               d->recSession.get());
        const QString dictPath = QFileInfo(QDir(targetDataPath), "ppocrv5_rec_dict.txt").absoluteFilePath();
        d->decoderReady = d->decoder.loadDictionary(dictPath);
        if (!d->decoderReady)
            qWarning() << "RapidOCR: recognition dictionary not loaded:" << dictPath;

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
    if (!d->isInitialized || !d->decoderReady) return results;

    // Real PP-OCRv5 pipeline: det → cls → perspective warp → SVTR rec → CTC decode.
    results = d->decoder.run(image);
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

bool RapidOcrEngine::isMockImplementation() const
{
#ifdef HAS_RAPIDOCR
    // Real PP-OCRv5 ONNX implementation is compiled in. (Model files may still
    // be absent at runtime — that is reported by initialize() returning false,
    // and gated in the UI by a file-existence check — but the implementation
    // itself is not a stub.)
    return false;
#else
    // Built without onnxruntime: processImage() is a no-op that returns no
    // results. This IS a stub, so report it honestly.
    return true;
#endif
}
