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
    return true;
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
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return true;
}
