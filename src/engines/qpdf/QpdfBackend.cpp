// SPDX-License-Identifier: Apache-2.0
#include "engines/qpdf/QpdfBackend.h"
#include <QDebug>

#ifdef HAS_QPDF
#include <qpdf/qpdf-c.h>
#include <QTemporaryFile>
#include <QFile>
#endif

#ifdef HAS_QPDF

extern "C" int qpdf_write_string_callback(const char* data, size_t len, void* udata) {
    std::string* s = static_cast<std::string*>(udata);
    s->append(data, len);
    return 0;
}

bool QpdfBackend::linearize(const QString &inputPath, const QString &outputPath) {
    qpdf_data qpdf = qpdf_init();
    if (!qpdf) return false;
    bool ok = false;
    if (qpdf_read(qpdf, inputPath.toUtf8().constData(), nullptr) == 0) {
        if (qpdf_init_write(qpdf, outputPath.toUtf8().constData()) == 0) {
            qpdf_set_linearization(qpdf, 1);
            if (qpdf_write(qpdf) == 0) {
                ok = true;
            }
        }
    }
    qpdf_cleanup(&qpdf);
    return ok;
}

bool QpdfBackend::repair(const QString &inputPath, const QString &outputPath) {
    qpdf_data qpdf = qpdf_init();
    if (!qpdf) return false;
    bool ok = false;
    // WP-R09a (WHOLE-ARCHITECTURE-REVIEW A04): a REPAIR is about recovering a
    // damaged file. The qpdf C API returns QPDF_WARNINGS (not QPDF_SUCCESS)
    // from read/init_write/write when a damaged file was recovered with
    // warnings — the old `== 0` checks treated every such recovery as a
    // failure, so the engine's structural-repair path could never repair any
    // genuinely damaged input and always fell through to "unable to open".
    // Only the QPDF_ERRORS bit is a hard failure.
    const QPDF_ERROR_CODE readCode =
        qpdf_read(qpdf, inputPath.toUtf8().constData(), nullptr);
    if ((readCode & QPDF_ERRORS) == 0) {
        const QPDF_ERROR_CODE initCode =
            qpdf_init_write(qpdf, outputPath.toUtf8().constData());
        if ((initCode & QPDF_ERRORS) == 0) {
            const QPDF_ERROR_CODE writeCode = qpdf_write(qpdf);
            ok = (writeCode & QPDF_ERRORS) == 0;
        }
    }
    qpdf_cleanup(&qpdf);
    return ok;
}

bool QpdfBackend::isLinearized(const QString &inputPath) {
    qpdf_data qpdf = qpdf_init();
    if (!qpdf) return false;
    bool ok = false;
    if (qpdf_read(qpdf, inputPath.toUtf8().constData(), nullptr) == 0) {
        ok = (qpdf_is_linearized(qpdf) != 0);
    }
    qpdf_cleanup(&qpdf);
    return ok;
}

QString QpdfBackend::inspectJson(const QString &inputPath) {
    qpdf_data qpdf = qpdf_init();
    if (!qpdf) return QString();
    QString result;
    if (qpdf_read(qpdf, inputPath.toUtf8().constData(), nullptr) == 0) {
        std::string jsonStr;
        if (qpdf_write_json(
                qpdf,
                2,
                qpdf_write_string_callback,
                &jsonStr,
                qpdf_dl_none,
                qpdf_sj_none,
                "",
                nullptr
            ) == 0) {
            result = QString::fromStdString(jsonStr);
        }
    }
    qpdf_cleanup(&qpdf);
    return result;
}

#else // Fallback implementations

bool QpdfBackend::linearize(const QString &inputPath, const QString &outputPath) {
    Q_UNUSED(inputPath); Q_UNUSED(outputPath);
    return false;
}

bool QpdfBackend::repair(const QString &inputPath, const QString &outputPath) {
    Q_UNUSED(inputPath); Q_UNUSED(outputPath);
    return false;
}

bool QpdfBackend::isLinearized(const QString &inputPath) {
    Q_UNUSED(inputPath);
    return false;
}

QString QpdfBackend::inspectJson(const QString &inputPath) {
    Q_UNUSED(inputPath);
    return QString();
}

#endif
