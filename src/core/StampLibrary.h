// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QColor>
#include <QDateTime>
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
struct StampTemplate {
    QString id;            // "builtin:<name>" or "custom:<uuid>"
    QString name;          // display name ("Approved")
    QString textTemplate;  // may carry ${...} placeholders
    QColor color = QColor(0xCC, 0x22, 0x22);
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

    static std::optional<StampTemplate> findById(const QString& id);

    // T2-6 core: placeholder substitution at apply time. Pure.
    static QString resolveText(const QString& textTemplate,
                               const QString& author,
                               const QDateTime& when);

    static QString placeholderHelp();
};
