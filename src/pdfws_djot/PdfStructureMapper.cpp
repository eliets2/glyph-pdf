// SPDX-License-Identifier: Apache-2.0
//
// PdfStructureMapper — born-PDF → SemanticDocument page-paragraph mapping.
//
// Strategy: page-level text extraction via PoDoFo PdfContentStreamReader.
// Each page becomes one Section; visible text tokens are concatenated into a
// single Paragraph block per page, tagged BornPDF provenance.
//
// Full layout-aware line/column segmentation (PP-DocLayout V2 quality) is
// out of scope for this mapper — that path requires OCR preprocessing and is
// served by OcrDjotMapper.  For born-PDF editing via Djot, use the OCR path
// (OcrPipeline → OcrDjotMapper) for layout-accurate output.
//
// Known limitation (see CHANGELOG Known Limitations):
//   PdfStructureMapper produces page-granularity paragraphs from raw content-
//   stream text tokens.  Fine-grained heading/list/table detection and column
//   re-flow are not performed.  Use OcrDjotMapper for layout-fidelity editing.

#include "PdfStructureMapper.h"
#include "docmodel/SemanticDocument.h"
#include "docmodel/Block.h"
#include "docmodel/Inline.h"
#include "docmodel/ProvenanceTag.h"

#include <podofo/podofo.h>

#include <string>
#include <string_view>
#include <vector>

namespace pdfws {

PdfStructureMapper::PdfStructureMapper() = default;
PdfStructureMapper::~PdfStructureMapper() = default;

// ---------------------------------------------------------------------------
// extractPageText — walk content stream; accumulate Tj/TJ text tokens.
// Returns empty string for image-only pages.
// ---------------------------------------------------------------------------
static std::string extractPageText(PoDoFo::PdfPage& page)
{
    std::string result;
    try {
        PoDoFo::PdfContentStreamReader reader(page);
        PoDoFo::PdfContent content;
        std::string lineAccum;
        bool inTextBlock = false;

        auto flushLine = [&]() {
            if (!lineAccum.empty()) {
                if (!result.empty()) result += '\n';
                result += lineAccum;
                lineAccum.clear();
            }
        };

        while (reader.TryReadNext(content)) {
            if (content.GetType() != PoDoFo::PdfContentType::Operator)
                continue;

            const PoDoFo::PdfOperator op = content.GetOperator();

            switch (op) {
            case PoDoFo::PdfOperator::BT:
                inTextBlock = true;
                break;

            case PoDoFo::PdfOperator::ET:
                inTextBlock = false;
                flushLine();
                break;

            case PoDoFo::PdfOperator::Tj:
                if (inTextBlock) {
                    const auto& stk = content.GetStack();
                    if (stk.size() > 0) {
                        const auto& obj = stk[0];
                        if (obj.IsString()) {
                            auto sv = obj.GetString().GetString();
                            lineAccum.append(sv.data(), sv.size());
                        }
                    }
                }
                break;

            case PoDoFo::PdfOperator::TJ:
                if (inTextBlock) {
                    const auto& stk = content.GetStack();
                    if (stk.size() > 0) {
                        const auto& obj = stk[0];
                        if (obj.IsArray()) {
                            for (const auto& elem : obj.GetArray()) {
                                if (elem.IsString()) {
                                    auto sv = elem.GetString().GetString();
                                    lineAccum.append(sv.data(), sv.size());
                                } else if (elem.IsNumber()) {
                                    // Large negative kerning gap = inter-word space
                                    if (elem.GetNumber() < -100) lineAccum += ' ';
                                }
                            }
                        }
                    }
                }
                break;

            case PoDoFo::PdfOperator::Quote:       // ' — newline + show string
            case PoDoFo::PdfOperator::DoubleQuote: // " — word/char spacing + newline + show
                if (inTextBlock) {
                    flushLine();
                    const auto& stk = content.GetStack();
                    // For DoubleQuote the string is the 3rd operand (index 2); for Quote, index 0
                    size_t strIdx = (op == PoDoFo::PdfOperator::DoubleQuote) ? 2 : 0;
                    if (stk.size() > strIdx) {
                        const auto& obj = stk[strIdx];
                        if (obj.IsString()) {
                            auto sv = obj.GetString().GetString();
                            lineAccum.append(sv.data(), sv.size());
                        }
                    }
                }
                break;

            case PoDoFo::PdfOperator::Td:
            case PoDoFo::PdfOperator::TD:
            case PoDoFo::PdfOperator::T_Star:
                if (inTextBlock) flushLine();
                break;

            default:
                break;
            }
        }
        flushLine(); // flush any trailing content after the last ET
    } catch (...) {
        // Content-stream parse error: return whatever was accumulated so far.
    }
    return result;
}

// ---------------------------------------------------------------------------

std::unique_ptr<docmodel::SemanticDocument>
PdfStructureMapper::mapPdfToSemantic(const std::string& pdfFilePath)
{
    docmodel::Provenance docProv;
    docProv.tag         = docmodel::ProvenanceTag::BornPDF;
    docProv.source_file = pdfFilePath;
    docProv.page_index  = -1;

    std::vector<std::shared_ptr<docmodel::Section>> sections;

    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfFilePath);

        const unsigned pageCount = doc.GetPages().GetCount();
        sections.reserve(pageCount);

        for (unsigned pi = 0; pi < pageCount; ++pi) {
            PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pi);
            const std::string text = extractPageText(page);

            docmodel::Provenance pageProv;
            pageProv.tag         = docmodel::ProvenanceTag::BornPDF;
            pageProv.source_file = pdfFilePath;
            pageProv.page_index  = static_cast<int>(pi);
            // Full-page bbox in PDF coordinates
            auto mb = page.GetMediaBox();
            pageProv.bbox = { mb.X, mb.Y, mb.X + mb.Width, mb.Y + mb.Height };

            std::vector<std::shared_ptr<docmodel::Block>> blocks;

            if (!text.empty()) {
                auto inlineNode = std::make_shared<docmodel::TextInline>(text, pageProv);
                std::vector<std::shared_ptr<docmodel::Inline>> inlines;
                inlines.push_back(std::move(inlineNode));
                blocks.push_back(std::make_shared<docmodel::TextBlock>(
                    docmodel::Block::Type::Paragraph,
                    std::move(inlines),
                    pageProv));
            }
            // Pages with no extractable text produce an empty section (scan-only pages).

            sections.push_back(std::make_shared<docmodel::Section>(
                std::string{},  // page sections carry no heading title
                std::move(blocks),
                std::vector<std::shared_ptr<docmodel::Section>>{},
                pageProv));
        }
    } catch (const PoDoFo::PdfError& e) {
        // Return a 1-section error document instead of empty.
        docmodel::Provenance errProv;
        errProv.tag         = docmodel::ProvenanceTag::BornPDF;
        errProv.source_file = pdfFilePath;
        errProv.page_index  = 0;
        std::string errText = std::string("[PdfStructureMapper: load error] ") + e.what();
        auto errInline = std::make_shared<docmodel::TextInline>(errText, errProv);
        std::vector<std::shared_ptr<docmodel::Inline>> inlines;
        inlines.push_back(std::move(errInline));
        std::vector<std::shared_ptr<docmodel::Block>> blocks;
        blocks.push_back(std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph, std::move(inlines), errProv));
        sections.push_back(std::make_shared<docmodel::Section>(
            std::string{}, std::move(blocks),
            std::vector<std::shared_ptr<docmodel::Section>>{}, errProv));
    }

    return std::make_unique<docmodel::SemanticDocument>(std::move(sections), docProv);
}

bool PdfStructureMapper::applySemanticToPdf(const docmodel::SemanticDocument& /*doc*/,
                                             const std::string& /*inputPdf*/,
                                             const std::string& /*outputPdf*/)
{
    // applySemanticToPdf for born-PDFs is intentionally not implemented.
    // The ProvenanceGuard (wired in WP-3/D-05) refuses DjotThenSave on born-PDFs,
    // so this path is never reached in production.  Returning false is correct
    // and safe — callers must check the return value.
    return false;
}

} // namespace pdfws
