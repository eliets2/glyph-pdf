// SPDX-License-Identifier: Apache-2.0
#include "core/StampLibrary.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
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
        t.imagePath = o.value(QStringLiteral("image")).toString();
        if (!t.color.isValid()) t.color = QColor(0xCC, 0x22, 0x22);
        // Built-in ids are reserved — a tampered file must not shadow them.
        if (t.id.isEmpty() || t.name.isEmpty()) continue;
        if (t.id.startsWith(QStringLiteral("builtin:"))) continue;
        // Row 18: an entry carries EXACTLY ONE placement carrier — a text
        // template OR an image path, never both, never neither. The image
        // path must be a safe RELATIVE path (absolute or parent-traversing
        // values are a tampered catalog, not something to resolve) — and an
        // unsafe path refuses the entry regardless of the text carrier.
        const bool hasText = !t.textTemplate.isEmpty();
        const bool hasImage = !t.imagePath.isEmpty();
        const bool safeImage = hasImage
            && !QDir::isAbsolutePath(t.imagePath)
            && !t.imagePath.contains(QStringLiteral(".."));
        if (hasImage && !safeImage) continue;
        if (hasText == hasImage) continue;
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
        if (!t.textTemplate.isEmpty())
            o.insert(QStringLiteral("template"), t.textTemplate);
        if (!t.imagePath.isEmpty())
            o.insert(QStringLiteral("image"), t.imagePath);
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

// ── Row 18: image-variant import ────────────────────────────────────────────

QString StampLibrary::defaultCustomPath() {
    return customStampsDefaultPath();
}

QString StampLibrary::imageStampsDirFor(const QString& jsonPath) {
    return QDir(QFileInfo(jsonPath).absolutePath())
        .filePath(QStringLiteral("stamp-images"));
}

QString StampLibrary::imageAbsolutePath(const QString& jsonPath,
                                        const QString& storedPath) {
    if (storedPath.isEmpty()) return QString();
    // Only safe relative paths are ever resolved; anything else was already
    // refused at load — this is the second gate for in-memory callers.
    if (QDir::isAbsolutePath(storedPath)) return QString();
    if (storedPath.contains(QStringLiteral(".."))) return QString();
    return QDir(QFileInfo(jsonPath).absolutePath()).filePath(storedPath);
}

std::optional<StampTemplate> StampLibrary::addImageStampTo(const QString& jsonPath,
                                                           const QString& name,
                                                           const QString& sourceImagePath,
                                                           QString* error) {
    const auto refuse = [error](const QString& msg)
                            -> std::optional<StampTemplate> {
        if (error) *error = msg;
        return std::nullopt;
    };
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty())
        return refuse(QObject::tr("Enter a name for the stamp."));
    if (sourceImagePath.isEmpty() || !QFileInfo::exists(sourceImagePath))
        return refuse(QObject::tr("Choose an image file first."));

    // The source must FULLY decode now — a file that merely sniffs as an
    // image header (or decodes to nothing) is refused with its reason,
    // never accepted into the catalog.
    QImageReader reader(sourceImagePath);
    reader.setAutoTransform(true);   // honor EXIF orientation
    const QImage img = reader.read();
    if (img.isNull() || img.width() < 1 || img.height() < 1)
        return refuse(QObject::tr("Could not read %1 as an image "
                                  "(unsupported or corrupt file).")
                          .arg(QFileInfo(sourceImagePath).fileName()));

    // The catalog owns its own normalized PNG copy — the user's original
    // file is never referenced (moving or deleting it must not break stamps).
    const QString imageDir = imageStampsDirFor(jsonPath);
    if (!QDir().mkpath(imageDir))
        return refuse(QObject::tr("Could not create the stamp image folder."));
    const QString stored = QStringLiteral("stamp-images/%1.png")
                               .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!img.save(imageAbsolutePath(jsonPath, stored)))
        return refuse(QObject::tr("Could not save a copy of the image with the stamps."));

    // Append + persist. A failed save must not leave the copied image behind
    // (no orphans, no half import).
    QList<StampTemplate> stamps = loadCustomFrom(jsonPath);
    StampTemplate t;
    t.id = QStringLiteral("custom:")
           + QUuid::createUuid().toString(QUuid::WithoutBraces);
    t.name = trimmedName;
    t.imagePath = stored;
    stamps.append(t);
    if (!saveCustomTo(jsonPath, stamps)) {
        QFile::remove(imageAbsolutePath(jsonPath, stored));
        return refuse(QObject::tr("Could not save the custom stamp."));
    }
    return t;
}

QImage StampLibrary::loadStampImage(const QString& jsonPath, const StampTemplate& t) {
    const QString abs = imageAbsolutePath(jsonPath, t.imagePath);
    if (abs.isEmpty()) return QImage();
    QImageReader reader(abs);
    reader.setAutoTransform(true);
    // Null when missing or corrupt — the caller surfaces that honestly.
    return reader.read();
}
