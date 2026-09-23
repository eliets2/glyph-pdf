// SPDX-License-Identifier: Apache-2.0
#include "core/StampLibrary.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

QList<StampTemplate> StampLibrary::builtIns() {
    return {
        { QStringLiteral("builtin:approved"),     QStringLiteral("Approved"),
          QStringLiteral("Approved | ${author} | ${date}"), QColor(0x1B, 0x7F, 0x3B) },
        { QStringLiteral("builtin:draft"),        QStringLiteral("Draft"),
          QStringLiteral("DRAFT | ${date}"),                QColor(0xCC, 0x88, 0x00) },
        { QStringLiteral("builtin:confidential"), QStringLiteral("Confidential"),
          QStringLiteral("CONFIDENTIAL"),                   QColor(0xCC, 0x22, 0x22) },
        { QStringLiteral("builtin:received"),     QStringLiteral("Received"),
          QStringLiteral("Received | ${author} | ${datetime}"), QColor(0x1B, 0x4F, 0x8F) },
        { QStringLiteral("builtin:reviewed"),     QStringLiteral("Reviewed"),
          QStringLiteral("Reviewed | ${author} | ${date}"), QColor(0x6B, 0x21, 0xA8) },
    };
}

QList<StampTemplate> StampLibrary::all() {
    QList<StampTemplate> out = builtIns();
    out.append(custom());
    return out;
}

QString customStampsDefaultPath() {
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) return QString();
    return QDir(base).filePath(QStringLiteral("stamps.json"));
}

QList<StampTemplate> StampLibrary::custom() {
    const QString path = customStampsDefaultPath();
    if (path.isEmpty() || !QFile::exists(path)) return {};
    return loadCustomFrom(path);
}

bool StampLibrary::saveCustom(const QList<StampTemplate>& stamps) {
    const QString path = customStampsDefaultPath();
    if (path.isEmpty()) return false;
    // Fresh profile / test mode: the AppLocalData directory does not exist
    // yet and QFile::open(WriteOnly) refuses to create parent directories.
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    return saveCustomTo(path, stamps);
}

QList<StampTemplate> StampLibrary::loadCustomFrom(const QString& path) {
    QList<StampTemplate> out;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return out;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) return out;
    const QJsonArray arr = doc.object().value(QStringLiteral("stamps")).toArray();
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        StampTemplate t;
        t.id = o.value(QStringLiteral("id")).toString();
        t.name = o.value(QStringLiteral("name")).toString();
        t.textTemplate = o.value(QStringLiteral("template")).toString();
        t.color = QColor(o.value(QStringLiteral("color")).toString());
        if (t.id.isEmpty() || t.name.isEmpty() || t.textTemplate.isEmpty()) continue;
        if (!t.color.isValid()) t.color = QColor(0xCC, 0x22, 0x22);
        // Built-in ids are reserved — a tampered file must not shadow them.
        if (t.id.startsWith(QStringLiteral("builtin:"))) continue;
        out.append(t);
    }
    return out;
}

bool StampLibrary::saveCustomTo(const QString& path, const QList<StampTemplate>& stamps) {
    QJsonArray arr;
    for (const auto& t : stamps) {
        if (t.id.startsWith(QStringLiteral("builtin:"))) continue;
        QJsonObject o;
        o.insert(QStringLiteral("id"), t.id);
        o.insert(QStringLiteral("name"), t.name);
        o.insert(QStringLiteral("template"), t.textTemplate);
        o.insert(QStringLiteral("color"), t.color.name());
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("stamps"), arr);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        file.close();
        return false;
    }
    file.close();
    return true;
}

std::optional<StampTemplate> StampLibrary::findById(const QString& id) {
    for (const auto& t : all())
        if (t.id == id) return t;
    return std::nullopt;
}

QString StampLibrary::resolveText(const QString& textTemplate,
                                  const QString& author,
                                  const QDateTime& when) {
    QString out = textTemplate;
    const QString name = author.trimmed().isEmpty()
                             ? QStringLiteral("Unknown")
                             : author.trimmed();
    out.replace(QStringLiteral("${author}"), name);
    out.replace(QStringLiteral("${date}"),
                when.date().toString(QStringLiteral("yyyy-MM-dd")));
    out.replace(QStringLiteral("${time}"),
                when.time().toString(QStringLiteral("HH:mm")));
    out.replace(QStringLiteral("${datetime}"),
                when.toString(QStringLiteral("yyyy-MM-dd HH:mm")));
    return out;
}

QString StampLibrary::placeholderHelp() {
    return QStringLiteral("${author}, ${date} (yyyy-MM-dd), ${time} (HH:mm), "
                          "${datetime} (yyyy-MM-dd HH:mm)");
}
