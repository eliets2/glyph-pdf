// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QRectF>

struct OcrResult {
    QString text;
    QRectF boundingBox;
    int confidence;
};

// OCR language table (single source of truth, audit 9.4 P0).
struct OcrLanguageInfo {
    const char* uiCode;
    const char* engineCode;
    const char* displayName;
};
inline const QList<OcrLanguageInfo>& ocrLanguages() {
    static const QList<OcrLanguageInfo> languages = {
        { "EN", "eng", "English" },
        { "DE", "deu", "Deutsch" },
        { "FR", "fra", "Fran\u00e7ais" },
        { "ES", "spa", "Espa\u00f1ol" },
        { "IT", "ita", "Italiano" },
        { "PT", "por", "Portugu\u00eas" },
        { "RU", "rus", "\u0420\u0443\u0441\u0441\u043a\u0438\u0439" },
        { "ZH", "chi_sim", "\u4e2d\u6587 (\u7b80)" },
        { "JA", "jpn", "\u65e5\u672c\u8a9e" },
        { "KO", "kor", "\ud55c\uad6d\uc5b4" },
        { "AR", "ara", "\u0627\u0644\u0639\u0631\u0628\u064a\u0629" },
        { "NL", "nld", "Nederlands" },
    };
    return languages;
}

// ── OCR output mode (PARITY-SCORECARD-2026-09-30 §4 #5) ────────────────────
// What an OCR Accept/export produces from the recognized words:
//   Searchable — the original page image stays, with the recognized text as
//                an INVISIBLE text layer (the MRC PDF/A writer path).
//   Editable   — the recognized text REPLACES the page content: visible text
//                on a blank page, no scan image (the review-doc path).
enum class OcrOutputMode {
    Searchable,
    Editable
};

// The persisted preference key carrying the output-mode choice (written by
// the OCRMode toolbar combo, read by EditController at Accept time — the same
// QSettings wire pattern as ocr/language and ocr/orientDetect).
inline QString ocrOutputModePrefKey() {
    return QStringLiteral("ocr/outputMode");
}

// Persisted value → mode. Only an explicit "editable" selects Editable;
// everything else (empty, unknown, legacy values) is Searchable — the shipped
// default and the only output every historical build produced. An unknown
// stored value therefore behaves as the DOCUMENTED default; it can never
// silently flip an explicit "editable" choice off.
inline OcrOutputMode ocrOutputModeFromPref(const QString& value) {
    return value.compare(QStringLiteral("editable"), Qt::CaseInsensitive) == 0
        ? OcrOutputMode::Editable
        : OcrOutputMode::Searchable;
}

// Mode → canonical persisted value (round-trips through ocrOutputModeFromPref).
inline QString ocrOutputModePrefValue(OcrOutputMode mode) {
    return mode == OcrOutputMode::Editable
        ? QStringLiteral("editable")
        : QStringLiteral("searchable");
}

// Map a persisted UI code (case-insensitive) to the engine language code.
// Unknown/empty codes fall back to "eng" so OCR never receives garbage.
inline QString ocrEngineLanguageCode(const QString& uiCode) {
    const QString needle = uiCode.trimmed();
    for (const auto& l : ocrLanguages()) {
        if (needle.compare(QLatin1String(l.uiCode), Qt::CaseInsensitive) == 0)
            return QLatin1String(l.engineCode);
    }
    return QStringLiteral("eng");
}
