// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QHash>
#include <QList>
#include <optional>

/// Strongly-typed enumeration of every tool action in GlyphPDF.
/// Each controller maps a subset of these to its activate() handler.
enum class ToolId {
    // ── Home ──
    Open,
    Save,
    SaveAs,
    Print,
    PrintPreview,
    PageSetup,
    ExportPresets,
    Share,
    Properties,

    // ── View ──
    ZoomIn,
    ZoomOut,
    ActualSize,
    FitWidth,
    FitPage,
    SinglePage,
    Continuous,
    TwoPage,
    Presentation,
    Fullscreen,
    DarkMode,
    EyeCare,

    // ── Edit ──
    Hand,
    Select,
    SelectObject,
    EditText,
    EditObject,
    EditImage,
    Search,
    Ocr,
    Highlight,
    Underline,
    Strikeout,
    Squiggly,
    Note,
    Comment,
    Pencil,
    Freehand,
    TextBox,
    AddText,
    Line,
    Arrow,
    Rectangle,
    Oval,
    Signature,
    Image,
    MarkRedact,

    // ── Pages ──
    RotateCW,
    RotateCCW,
    DeletePage,
    InsertPage,
    Extract,
    Split,
    Reorder,
    Crop,
    Resize,
    AddHeader,
    AddFooter,
    AddPageNumbers,
    BatesNumber,

    // ── Convert ──
    Combine,
    ToWord,
    ToExcel,
    ToCsv,
    ToHtml,
    ToText,
    ToPPT,
    ToImage,
    Compress,
    Linearize,
    PdfA,
    ImportOffice,
    ImagesToPdf,

    // ── Forms ──
    TextField,
    Checkbox,
    Radio,
    Dropdown,
    CreateForm,
    ListBox,
    Button,
    DateField,
    NumField,
    SigField,
    AutoDetect,
    Tabs,
    ImportData,
    ExportData,

    // ── Security / Protect ──
    Encrypt,
    Password,
    Sign,
    ValidateSig,
    Sanitize,
    ApplyRedact,
    ExportAnno,
    ImportAnno,
    Permissions,
    RemoveSecurity,
    Certify,
    Timestamp,
    PatternRedact,
    RegexRedact,

    COUNT  // sentinel for array sizing — must be last
};

/// Convert a ToolId to its canonical string representation.
QString toolIdToString(ToolId id);

/// Parse a string (case-insensitive, alias-aware) to a ToolId.
/// Returns std::nullopt for unknown strings.
std::optional<ToolId> toolIdFromString(const QString& str);

/// Returns true if `str` maps to a known ToolId.
bool isValidToolIdString(const QString& str);

/// QHash support for ToolId as a key.
inline size_t qHash(ToolId key, size_t seed = 0) noexcept {
    return ::qHash(static_cast<int>(key), seed);
}
