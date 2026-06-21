// SPDX-License-Identifier: Apache-2.0
#include "core/ErrorInfo.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

// ── ErrorLog helpers ────────────────────────────────────────────────────

int ErrorLog::errorCount() const {
    int n = 0;
    for (const auto& e : entries)
        if (e.severity >= ErrorInfo::Error) ++n;
    return n;
}

int ErrorLog::warningCount() const {
    int n = 0;
    for (const auto& e : entries)
        if (e.severity == ErrorInfo::Warning) ++n;
    return n;
}

bool ErrorLog::hasErrors() const {
    for (const auto& e : entries)
        if (e.severity >= ErrorInfo::Error) return true;
    return false;
}

bool ErrorLog::exportCsv(const QString& path) const {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&f);
    out << "Timestamp,Severity,File,Page,Message,Technical Details\n";
    for (const auto& e : entries) {
        auto esc = [](QString s) {
            s.replace('"', QStringLiteral("\"\""));
            return '"' + s + '"';
        };
        out << esc(e.timestamp.toString(Qt::ISODate)) << ','
            << esc(e.severityString()) << ','
            << esc(e.sourceFile) << ','
            << (e.sourcePage >= 0 ? QString::number(e.sourcePage) : QString()) << ','
            << esc(e.userMessage) << ','
            << esc(e.technicalDetails) << '\n';
    }
    // Don't report success until the bytes are flushed without an IO error — callers
    // (BatchMode, ErrorDialog) surface this bool to the user as "export succeeded".
    out.flush();
    return out.status() == QTextStream::Ok && f.error() == QFileDevice::NoError;
}

bool ErrorLog::exportJson(const QString& path) const {
    QJsonArray arr;
    for (const auto& e : entries) {
        QJsonObject obj;
        obj["timestamp"]        = e.timestamp.toString(Qt::ISODate);
        obj["severity"]         = e.severityString();
        obj["message"]          = e.userMessage;
        obj["technicalDetails"] = e.technicalDetails;
        if (!e.sourceFile.isEmpty())
            obj["file"] = e.sourceFile;
        if (e.sourcePage >= 0)
            obj["page"] = e.sourcePage;
        arr.append(obj);
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    const QByteArray json = QJsonDocument(arr).toJson(QJsonDocument::Indented);
    const qint64 written = f.write(json);
    // Verify the full payload was written and no IO error occurred before claiming success.
    return written == json.size() && f.error() == QFileDevice::NoError;
}
