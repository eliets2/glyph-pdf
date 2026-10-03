// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QColor>
#include <QDateTime>
#include <QImage>
#include <QList>
#include <QString>
#include <optional>

// ── T2-6: stamp library + dynamic stamp templates ───────────────────────────
// A stamp template is a NAME plus a TEXT TEMPLATE that may carry dynamic
// placeholders. Placeholders are substituted AT APPLY TIME (when the user
// clicks the page), so the SAVED annotation text always carries the concrete
// author/date the stamp was placed with — never a live field that re-evaluates
// on reopen (the honest, Acrobat-compatible model, and the persistence claim
// the tests verify through embedAnnotations → extractAnnotations).
//
// Supported placeholders (disclosed to the user in the library dialog):
//   ${author}   — the default author name (empty name → "Unknown")
//   ${date}     — apply date, yyyy-MM-dd
//   ${time}     — apply time, HH:mm
//   ${datetime} — apply date+time, "yyyy-MM-dd HH:mm"
// Unknown placeholders are left VERBATIM (visible, never silently dropped).
//
// ── Row 18 image variant ────────────────────────────────────────────────────
// A template carries EXACTLY ONE placement carrier: a text template OR an
// image path (text XOR image — a text entry with a non-empty imagePath, or
// an image entry with no path, is an INVALID carrier). The rule is enforced
// at every boundary that builds or reads persisted entries — loadCustomFrom
// refuses a text entry carrying an unsafe image path, and addImageStampTo
// writes exactly one carrier — but StampTemplate is an aggregate, so an
// IN-MEMORY construction site owns the duty too: set imagePath only for the
// image variant and keep textTemplate the sole carrier otherwise. Placing
// goes through loadStampImage/imageAbsolutePath, which fail closed on an
// unsafe path either way. An image stamp is an imported picture (pick file →
// name it → it lands in the catalog); placing it arms the EXISTING
// signature-Upload placement so the annotation rides the §9.7 P0 /Stamp +
// image-appearance writer unchanged. The image itself is a COPY owned by
// the catalog (never a reference to the user's original file), stored as a
// path RELATIVE to the stamps.json directory — the profile stays
// self-contained, and absolute/traversing paths are refused at load (tamper
// resistance).
struct StampTemplate {
    QString id;            // "builtin:<name>" or "custom:<uuid>"
    QString name;          // display name ("Approved")
    QString textTemplate;  // may carry ${...} placeholders
    QColor color = QColor(0xCC, 0x22, 0x22);
    QString imagePath;     // image variant: "stamp-images/<uuid>.png" (relative)
};

class StampLibrary {
public:
    // The five built-ins the research row names (Approved / Draft /
    // Confidential / Received / Reviewed) — always present, not removable.
    static QList<StampTemplate> builtIns();

    // Built-ins first, then the persisted custom stamps.
    static QList<StampTemplate> all();

    // Custom stamp persistence (AppData stamps.json in the app; explicit
    // paths in tests — deterministic, no environment writes).
    static QList<StampTemplate> custom();
    static bool saveCustom(const QList<StampTemplate>& stamps);
    static QList<StampTemplate> loadCustomFrom(const QString& path);
    static bool saveCustomTo(const QString& path, const QList<StampTemplate>& stamps);

    // Row 18: the stamps.json the app persists to (explicit-path functions
    // above remain the testable seam).
    static QString defaultCustomPath();

    // ── Row 18 image-variant import ─────────────────────────────────────────
    // "<dir-of-json>/stamp-images" — the managed folder holding the catalog's
    // own image copies.
    static QString imageStampsDirFor(const QString& jsonPath);
    // Resolve a STORED catalog path against the json location. Empty unless
    // the stored path is a safe relative path (no absolute, no "..").
    static QString imageAbsolutePath(const QString& jsonPath, const QString& storedPath);
    // ONE committed import step: the source must FULLY decode as an image,
    // a normalized PNG copy lands in stamp-images/, the entry is appended
    // and persisted. On any failure returns nullopt and (when provided)
    // fills *errorOut with a typed, user-facing reason — the same
    // bool/opt + errorOut refusal shape as the sibling utility writers
    // (ReviewSummaryWriter, A11yReportWriter) — never a silent accept,
    // never a half import.
    static std::optional<StampTemplate> addImageStampTo(const QString& jsonPath,
                                                        const QString& name,
                                                        const QString& sourceImagePath,
                                                        QString* errorOut = nullptr);
    // Decode the stamp image for placement/preview (EXIF orientation honored).
    // A null image means missing or undecodable — callers must surface that
    // honestly (typed message, no placement), never place a blank stamp.
    static QImage loadStampImage(const QString& jsonPath, const StampTemplate& t);

    static std::optional<StampTemplate> findById(const QString& id);

    // T2-6 core: placeholder substitution at apply time. Pure.
    static QString resolveText(const QString& textTemplate,
                               const QString& author,
                               const QDateTime& when);

    static QString placeholderHelp();
};
