// src/engines/ocr/OcrDjotMapper.h
// OcrDjotMapper — stateless converter: fused OCR output → SemanticDocument tree.
//
// Design constraints (M5-P4):
//   - Stateless / pure-function: fromOcrResults() has no hidden state.
//   - Every block node carries a BornOCR ProvenanceTag with source path + page + bbox.
//   - Per-word fused result → TextInline with {pdf-page pdf-bbox pdf-font ocr-conf} attrs
//     encoded into the Djot attribute string stored in the Provenance source_file field.
//   - Reading-order (LayoutRegion::readingOrderIndex) → block order in SemanticDocument.
//   - RegionType mapping:
//       Title    → Heading (level 1)
//       Paragraph → Paragraph
//       Table    → ContainerBlock (Table type) — pipe-table via codec emit
//       Figure   → Paragraph (with page-figure attr in provenance source_file)
//       List     → List (ContainerBlock with ListItem children)
//       Header   → Paragraph (page-header attr in provenance source_file)
//       Footer   → Paragraph (page-footer attr in provenance source_file)
//       Equation → Paragraph (page-equation attr)
//       Reference → Paragraph (page-reference attr)
//       Caption  → Paragraph (page-caption attr)
//       Other    → Paragraph
//
// NOTE: Tables use a dedicated cell structure stored in ContainerBlock children.
//       Each row is a ContainerBlock(ListItem), each cell is a TextBlock(Paragraph).
//       LuaDjotCodec::documentToDjot encodes these as Djot pipe-table syntax.
//
// NOTE: Djot attribute strings are not part of the current docmodel::Inline API.
//       Attributes are embedded in Provenance::source_file using the convention:
//           "{attr1=v1 attr2=v2}|<original-pdf-path>"
//       Downstream (M7-P3 MRC sandwich) reads these to align word boxes.
#pragma once

#include <QList>
#include <QString>
#include <memory>

#include "engines/ocr/OcrPipeline.h"      // PageOcrResult, MergedOcrWord
#include "engines/ocr/ILayoutDetector.h"   // LayoutRegion, RegionType
#include "docmodel/SemanticDocument.h"      // SemanticDocument

/// Stateless converter from fused OCR results to a SemanticDocument tree.
///
/// Thread-safety: fromOcrResults() is re-entrant — it captures no shared mutable
/// state (only takes its arguments by value / const-ref). Safe to call from
/// multiple CPU lane workers simultaneously.
class OcrDjotMapper {
public:
    OcrDjotMapper() = default;
    ~OcrDjotMapper() = default;

    // Non-copyable, non-movable (stateless — no need to copy/move).
    OcrDjotMapper(const OcrDjotMapper&) = delete;
    OcrDjotMapper& operator=(const OcrDjotMapper&) = delete;

    /// Convert fused OCR results (from OcrPipeline::recognizeDocument) to a
    /// SemanticDocument tree with full PDF-fidelity provenance attributes.
    ///
    /// \param results   One PageOcrResult per page, in page order.
    /// \param pdfPath   Source PDF path (embedded in every ProvenanceTag).
    ///
    /// \returns         A SemanticDocument where each Section corresponds to one
    ///                  page and blocks are ordered by readingOrderIndex.
    docmodel::SemanticDocument fromOcrResults(
        const QList<PageOcrResult>& results,
        const QString& pdfPath = QString());
};
