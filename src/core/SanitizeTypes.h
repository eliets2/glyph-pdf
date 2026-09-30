// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QFlags>
#include <QMetaType>
#include <QObject>

// PARITY-SCORECARD-2026-09-30 §4 #4 (July rows 72-73): selective Sanitize
// Document + pre-commit summary. The categories mirror the exact walk of
// PoDoFoBackend's sanitize pass. The classify walk and the removal pass are
// ONE implementation (same traversal, same predicates), so a summary count can
// never describe different work than the removal performs — the plan the dialog
// shows and the plan the commit reports come from the same code.
enum class SanitizeCategory {
    Metadata          = 1 << 0,  // /Info keys, catalog XMP /Metadata, page /Metadata
    Attachments       = 1 << 1,  // /Names /EmbeddedFiles name-tree entries
    Annotations       = 1 << 2,  // annotation /Contents + /RC bodies, RichMedia/Screen/Movie annots
    FormFields        = 1 << 3,  // AcroForm /V //DV values, /XFA form data
    JavaScriptActions = 1 << 4,  // /Names /JavaScript, /OpenAction, catalog+page /AA,
                                 // page /A, dangerous annotation /A actions
    HiddenLayers      = 1 << 5,  // /OCProperties — explicit OFF policy (hidden stays hidden)
    Bookmarks         = 1 << 6,  // /Outlines tree
    PrivateData       = 1 << 7,  // /PieceInfo, /Collection, /MarkInfo, /OutputIntents, /Thumb
    StructureAltText  = 1 << 8,  // StructTree /Alt //ActualText //E replacement text
};
Q_DECLARE_FLAGS(SanitizeCategories, SanitizeCategory)
Q_DECLARE_OPERATORS_FOR_FLAGS(SanitizeCategories)

// Every category the sanitize walk knows. The legacy one-argument
// sanitizeDocument() path is exactly "all categories" — all-or-nothing stays
// the default behavior.
inline SanitizeCategories sanitizeAllCategories()
{
    return SanitizeCategory::Metadata | SanitizeCategory::Attachments
         | SanitizeCategory::Annotations | SanitizeCategory::FormFields
         | SanitizeCategory::JavaScriptActions | SanitizeCategory::HiddenLayers
         | SanitizeCategory::Bookmarks | SanitizeCategory::PrivateData
         | SanitizeCategory::StructureAltText;
}

// User-facing label for a category (dialog, result report). Singular topic
// wording, no jargon — the same string the summary dialog and the completion
// message show.
inline QString sanitizeCategoryLabel(SanitizeCategory c)
{
    switch (c) {
        case SanitizeCategory::Metadata:          return QObject::tr("Metadata");
        case SanitizeCategory::Attachments:       return QObject::tr("File attachments");
        case SanitizeCategory::Annotations:       return QObject::tr("Annotation contents");
        case SanitizeCategory::FormFields:        return QObject::tr("Form field values");
        case SanitizeCategory::JavaScriptActions: return QObject::tr("JavaScript and actions");
        case SanitizeCategory::HiddenLayers:      return QObject::tr("Hidden content layers");
        case SanitizeCategory::Bookmarks:         return QObject::tr("Bookmarks");
        case SanitizeCategory::PrivateData:       return QObject::tr("Private application data");
        case SanitizeCategory::StructureAltText:  return QObject::tr("Structure replacement text");
    }
    return QObject::tr("Unknown");
}

// One category's findings: how many items the walk found (and would remove),
// plus short human-readable item labels (file names, field names, bookmark
// titles) for the summary. `items` is a representative subset — size may be
// smaller than `count` (capped), never larger.
struct SanitizeCategoryPlan {
    SanitizeCategory category = SanitizeCategory::Metadata;
    int count = 0;
    QStringList items;

    bool isValid() const { return count >= 0 && items.size() <= count; }
};

// The full classified plan. Only categories with count > 0 appear.
struct SanitizePlan {
    QVector<SanitizeCategoryPlan> categories;

    bool empty() const
    {
        for (const auto &c : categories)
            if (c.count > 0) return false;
        return true;
    }
    int totalCount() const
    {
        int n = 0;
        for (const auto &c : categories) n += c.count;
        return n;
    }
    const SanitizeCategoryPlan *find(SanitizeCategory c) const
    {
        for (const auto &p : categories)
            if (p.category == c) return &p;
        return nullptr;
    }
    // One-line description, e.g.
    // "Metadata: 4 items · File attachments: 2 files (legacy.doc, notes.xlsx)".
    // Items are shown up to a small cap so the line stays readable.
    QString describe(int maxItems = 3) const
    {
        QStringList parts;
        for (const auto &c : categories) {
            if (c.count <= 0) continue;
            QString itemPart;
            if (!c.items.isEmpty()) {
                QStringList shown = c.items.mid(0, maxItems);
                itemPart = QStringLiteral(" (") + shown.join(QStringLiteral(", "))
                         + (c.items.size() > maxItems
                                ? QStringLiteral(", …")
                                : QString())
                         + QStringLiteral(")");
            }
            parts << QStringLiteral("%1: %2 item%3%4")
                         .arg(sanitizeCategoryLabel(c.category),
                              QString::number(c.count),
                              c.count == 1 ? QString() : QStringLiteral("s"),
                              itemPart);
        }
        return parts.join(QStringLiteral(" · "));
    }
};

Q_DECLARE_METATYPE(SanitizeCategoryPlan)
Q_DECLARE_METATYPE(SanitizePlan)
