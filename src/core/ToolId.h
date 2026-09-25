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
    Undo,
    Redo,
    Watermark,
    Compare,

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
    RTL,          // AR-8 D6: toggle right-to-left layout direction
    // Night Mode: content-level colour inversion of the rendered page pixels
    // (PdfViewerWidget::toggleNightMode) — distinct from DarkMode (application
    // chrome only) and EyeCare (a sepia tint over a still-white page).
    NightMode,

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
    Stamp,
    Callout,
    Erase,
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
    Cut,
    Copy,
    Paste,
    DeleteSelection,
    SelectAll,

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
    CalcField,
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
    ExpiryDate,

    // ── T2-6: dynamic stamps + library ──
    // Appended AFTER ExpiryDate so existing ToolId ordinals stay stable.
    // Not persisted anywhere (ToolMode ordinals are the persisted ones), but
    // the append-only rule keeps ToolId.cpp tables reviewable.
    StampApproved,
    StampDraft,
    StampConfidential,
    StampReceived,
    StampReviewed,
    StampLibraryManage,

    // ── T2-9: auto-bookmarks from text styles ──
    AutoBookmarks,

    // ── R15: promoted planned entries (canonical command identity) ──
    // Appended AFTER AutoBookmarks so existing ToolId ordinals stay stable
    // (same append-only rule as the stamp ids above). These ids give the
    // formerly hidden ribbon entries ONE canonical dispatch/enablement
    // identity through ToolRegistry + TaskNav.
    FindReplace,       // T2-2 dialog route (ribbon "findRep" / "regex")
    Measure,           // T1 measure task panel (ribbon "measure")
    MeasureDistance,   // ribbon "distance"
    MeasureArea,       // ribbon "area"
    OcrVerify,         // OCR Verify screen (ribbon "ocrVerify")
    OcrLanguage,       // OCR Verify screen language selection (ribbon "ocrLang")
    PanePages,         // View ▸ Panes: thumbnails (ribbon "thumbs")
    PaneBookmarks,     // View ▸ Panes: bookmarks panel
    PaneComments,      // View ▸ Panes: comments review list
    PaneLayers,        // View ▸ Panes: OCG layer list
    BatchConvert,      // Batch workspace (ribbon "batchConv")
    WatchFolder,       // Batch hot-folder section (ribbon "watch")

    // ── N17 (parity 2026-09-14): certificate-encryption recipient picker ──
    // Appended AFTER WatchFolder so existing ToolId ordinals stay stable
    // (same append-only rule as the stamp/auto-bookmark ids above).
    CertEncrypt,       // Protect ▸ Security "Encrypt (Certs)" (ribbon "certEncrypt")

    // ── R26: send-for-signing workflow (single-document request) ───────────
    // Appended AFTER CertEncrypt so existing ToolId ordinals stay stable
    // (same append-only rule).
    PrepareSigningRequest, // Protect ▸ Sign "Prepare Request" (ribbon "prepareSigningReq")

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
