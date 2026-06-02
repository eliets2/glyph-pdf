// src/engines/ocr/OcrDjotMapper.cpp
// OcrDjotMapper — stateless fused-OCR → SemanticDocument converter.
//
// Attribute encoding convention (embedded in Provenance::source_file):
//   "{pdf-page=N pdf-bbox=\"x y w h\" pdf-font=\"Name\" ocr-conf=0.92}|path/to/file.pdf"
//
// The M7-P3 MRC sandwich text layer reads these to align recognized words with
// compressed image tiles by bbox.  Do NOT change the format without updating
// MrcPageProcessor (M7-P3).

#include "engines/ocr/OcrDjotMapper.h"
#include "docmodel/Block.h"
#include "docmodel/Inline.h"
#include "docmodel/ProvenanceTag.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

// ── Internal helpers ─────────────────────────────────────────────────────────

namespace {

/// Build a Provenance with BornOCR tag, source PDF path, page index, and bbox.
docmodel::Provenance makeProv(const QString& pdfPath, int pageIndex,
                              const QRectF& bbox,
                              const std::string& extraAttrs = "")
{
    docmodel::Provenance prov;
    prov.tag        = docmodel::ProvenanceTag::BornOCR;
    prov.page_index = pageIndex;
    prov.bbox.x0    = bbox.left();
    prov.bbox.y0    = bbox.top();
    prov.bbox.x1    = bbox.right();
    prov.bbox.y1    = bbox.bottom();

    // Encode attrs + path into source_file using the agreed convention:
    //   "{attr-string}|pdf-path"
    // Empty attrs: just store the pdf path directly.
    if (!extraAttrs.empty()) {
        prov.source_file = extraAttrs + "|" + pdfPath.toStdString();
    } else {
        prov.source_file = pdfPath.toStdString();
    }

    return prov;
}

/// Format per-word Djot attribute string:
///   {pdf-page=N pdf-bbox="x y w h" pdf-font="Name" ocr-conf=0.92}
std::string wordAttrs(int pageIndex, const QRectF& bbox,
                      const QString& font, int confidence)
{
    // Confidence as [0.0, 1.0] ratio (matches Djot attribute convention)
    double conf = std::round(confidence * 10.0) / 1000.0;  // e.g. 92 → 0.92

    std::ostringstream oss;
    oss << "{pdf-page=" << pageIndex
        << " pdf-bbox=\""
        << static_cast<int>(std::round(bbox.x())) << " "
        << static_cast<int>(std::round(bbox.y())) << " "
        << static_cast<int>(std::round(bbox.width()))  << " "
        << static_cast<int>(std::round(bbox.height())) << "\""
        << " pdf-font=\"" << font.toStdString() << "\""
        << " ocr-conf=" << conf
        << "}";
    return oss.str();
}

/// Build a TextInline for one MergedOcrWord.
/// Attributes are embedded in Provenance::source_file per the agreed convention.
std::shared_ptr<docmodel::TextInline>
makeWordInline(const MergedOcrWord& word, int pageIndex, const QString& pdfPath)
{
    std::string attrs = wordAttrs(pageIndex, word.boundingBox,
                                  QString() /* no font info in MergedOcrWord */,
                                  word.confidence);
    docmodel::Provenance prov = makeProv(pdfPath, pageIndex, word.boundingBox, attrs);

    return std::make_shared<docmodel::TextInline>(word.text.toStdString(), prov);
}

/// Collect all MergedOcrWords that spatially overlap a given LayoutRegion bbox.
/// Words whose centroid falls inside the region bbox are considered part of it.
QList<MergedOcrWord> wordsForRegion(const QList<MergedOcrWord>& words,
                                    const QRectF& regionBbox)
{
    QList<MergedOcrWord> out;
    for (const auto& w : words) {
        // Use word centroid to assign to region
        QPointF centroid(w.boundingBox.x() + w.boundingBox.width() / 2.0,
                         w.boundingBox.y() + w.boundingBox.height() / 2.0);
        if (regionBbox.contains(centroid)) {
            out.append(w);
        }
    }
    return out;
}

/// Sort words by reading order (top-to-bottom, left-to-right within a region).
void sortWordsByReadingOrder(QList<MergedOcrWord>& words)
{
    std::sort(words.begin(), words.end(), [](const MergedOcrWord& a, const MergedOcrWord& b) {
        // Primary: top of bbox (Y axis)
        const double lineThreshold = std::max(a.boundingBox.height(), b.boundingBox.height()) * 0.5;
        if (std::abs(a.boundingBox.y() - b.boundingBox.y()) > lineThreshold) {
            return a.boundingBox.y() < b.boundingBox.y();
        }
        // Same line: left-to-right
        return a.boundingBox.x() < b.boundingBox.x();
    });
}

/// Build TextInline list from a word list.
std::vector<std::shared_ptr<docmodel::Inline>>
buildInlines(const QList<MergedOcrWord>& words, int pageIndex, const QString& pdfPath)
{
    std::vector<std::shared_ptr<docmodel::Inline>> inlines;
    inlines.reserve(static_cast<size_t>(words.size()));
    for (const auto& w : words) {
        inlines.push_back(makeWordInline(w, pageIndex, pdfPath));
    }
    return inlines;
}

/// Map a LayoutRegion (with its associated words) to a Block.
/// Returns nullptr for unsupported/empty regions.
std::shared_ptr<docmodel::Block>
regionToBlock(const LayoutRegion& region,
              const QList<MergedOcrWord>& allWords,
              int pageIndex,
              const QString& pdfPath)
{
    // Collect and sort the words belonging to this region
    QList<MergedOcrWord> regionWords = wordsForRegion(allWords, region.bbox);
    sortWordsByReadingOrder(regionWords);

    // Base provenance for this block
    docmodel::Provenance blockProv = makeProv(pdfPath, pageIndex, region.bbox);

    switch (region.type) {

    case RegionType::Title: {
        // → Heading (level 1)
        auto inlines = buildInlines(regionWords, pageIndex, pdfPath);
        return std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Heading, std::move(inlines), blockProv);
    }

    case RegionType::Header: {
        // → Paragraph with page-header attribute in provenance source_file
        docmodel::Provenance prov = blockProv;
        prov.source_file = "{page-header}|" + pdfPath.toStdString();
        auto inlines = buildInlines(regionWords, pageIndex, pdfPath);
        return std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph, std::move(inlines), prov);
    }

    case RegionType::Footer: {
        // → Paragraph with page-footer attribute
        docmodel::Provenance prov = blockProv;
        prov.source_file = "{page-footer}|" + pdfPath.toStdString();
        auto inlines = buildInlines(regionWords, pageIndex, pdfPath);
        return std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph, std::move(inlines), prov);
    }

    case RegionType::Figure: {
        // → Figure block (caption text from words; bbox in provenance)
        auto inlines = buildInlines(regionWords, pageIndex, pdfPath);
        return std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Figure, std::move(inlines), blockProv);
    }

    case RegionType::List: {
        // → List (ContainerBlock) with one ListItem per logical line of words.
        // Heuristic: group words into lines by Y-band, create one ListItem per line.
        std::vector<std::shared_ptr<docmodel::Block>> items;

        // Split words into lines (words whose Y-centroids are within one line-height)
        QList<QList<MergedOcrWord>> lines;
        for (const auto& w : regionWords) {
            double cy = w.boundingBox.y() + w.boundingBox.height() / 2.0;
            bool added = false;
            for (auto& line : lines) {
                if (line.isEmpty()) continue;
                double lineCy = line.first().boundingBox.y()
                              + line.first().boundingBox.height() / 2.0;
                if (std::abs(cy - lineCy) <= line.first().boundingBox.height() * 0.8) {
                    line.append(w);
                    added = true;
                    break;
                }
            }
            if (!added) lines.append({w});
        }

        for (const auto& line : lines) {
            auto itemInlines = buildInlines(line, pageIndex, pdfPath);
            QRectF lineBbox;
            for (const auto& w : line) lineBbox = lineBbox.united(w.boundingBox);
            docmodel::Provenance itemProv = makeProv(pdfPath, pageIndex, lineBbox);
            items.push_back(std::make_shared<docmodel::TextBlock>(
                docmodel::Block::Type::ListItem, std::move(itemInlines), itemProv));
        }

        return std::make_shared<docmodel::ContainerBlock>(
            docmodel::Block::Type::List, std::move(items), blockProv);
    }

    case RegionType::Table: {
        // → Table (ContainerBlock) with rows as ContainerBlock(ListItem),
        //   each row's child-blocks = cells as TextBlock(Paragraph).
        //
        // Heuristic cell segmentation:
        //   1. Sort words by Y to find rows.
        //   2. Within each row, sort by X to find cells.
        //   3. Cluster words into cells by X-gap analysis.
        //
        // For now, use a simple approach: one row per distinct Y-band,
        // one cell per distinct X-cluster within the row.

        // Group words into rows (Y-bands)
        QList<QList<MergedOcrWord>> rows;
        for (const auto& w : regionWords) {
            double cy = w.boundingBox.y() + w.boundingBox.height() / 2.0;
            bool added = false;
            for (auto& row : rows) {
                if (row.isEmpty()) continue;
                double rowCy = row.first().boundingBox.y()
                             + row.first().boundingBox.height() / 2.0;
                if (std::abs(cy - rowCy) <= row.first().boundingBox.height() * 0.8) {
                    row.append(w);
                    added = true;
                    break;
                }
            }
            if (!added) rows.append({w});
        }

        // Sort each row by X
        for (auto& row : rows) {
            std::sort(row.begin(), row.end(), [](const MergedOcrWord& a, const MergedOcrWord& b) {
                return a.boundingBox.x() < b.boundingBox.x();
            });
        }

        // Build table rows
        std::vector<std::shared_ptr<docmodel::Block>> rowBlocks;
        for (const auto& rowWords : rows) {
            if (rowWords.isEmpty()) continue;

            // Cluster words in this row into cells by X-gap.
            // A gap larger than the average word width / 2 marks a cell boundary.
            double avgWordW = 0.0;
            for (const auto& w : rowWords) avgWordW += w.boundingBox.width();
            avgWordW /= rowWords.size();
            const double cellGapThreshold = avgWordW * 0.8;

            QList<QList<MergedOcrWord>> cells;
            for (const auto& w : rowWords) {
                if (cells.isEmpty()) {
                    cells.append({w});
                } else {
                    const auto& lastCell = cells.last();
                    double lastRight = lastCell.last().boundingBox.right();
                    if (w.boundingBox.left() - lastRight > cellGapThreshold) {
                        cells.append({w});
                    } else {
                        cells.last().append(w);
                    }
                }
            }

            // Build cell blocks
            std::vector<std::shared_ptr<docmodel::Block>> cellBlocks;
            for (const auto& cellWords : cells) {
                auto cellInlines = buildInlines(cellWords, pageIndex, pdfPath);
                QRectF cellBbox;
                for (const auto& w : cellWords) cellBbox = cellBbox.united(w.boundingBox);
                docmodel::Provenance cellProv = makeProv(pdfPath, pageIndex, cellBbox);
                cellBlocks.push_back(std::make_shared<docmodel::TextBlock>(
                    docmodel::Block::Type::Paragraph, std::move(cellInlines), cellProv));
            }

            // Row bbox
            QRectF rowBbox;
            for (const auto& w : rowWords) rowBbox = rowBbox.united(w.boundingBox);
            docmodel::Provenance rowProv = makeProv(pdfPath, pageIndex, rowBbox);
            rowBlocks.push_back(std::make_shared<docmodel::ContainerBlock>(
                docmodel::Block::Type::ListItem, std::move(cellBlocks), rowProv));
        }

        return std::make_shared<docmodel::ContainerBlock>(
            docmodel::Block::Type::Table, std::move(rowBlocks), blockProv);
    }

    case RegionType::Equation: {
        docmodel::Provenance prov = blockProv;
        prov.source_file = "{page-equation}|" + pdfPath.toStdString();
        auto inlines = buildInlines(regionWords, pageIndex, pdfPath);
        return std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph, std::move(inlines), prov);
    }

    case RegionType::Reference: {
        docmodel::Provenance prov = blockProv;
        prov.source_file = "{page-reference}|" + pdfPath.toStdString();
        auto inlines = buildInlines(regionWords, pageIndex, pdfPath);
        return std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph, std::move(inlines), prov);
    }

    case RegionType::Caption: {
        docmodel::Provenance prov = blockProv;
        prov.source_file = "{page-caption}|" + pdfPath.toStdString();
        auto inlines = buildInlines(regionWords, pageIndex, pdfPath);
        return std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph, std::move(inlines), prov);
    }

    case RegionType::Paragraph:
    case RegionType::Other:
    default: {
        // → Paragraph
        auto inlines = buildInlines(regionWords, pageIndex, pdfPath);
        return std::make_shared<docmodel::TextBlock>(
            docmodel::Block::Type::Paragraph, std::move(inlines), blockProv);
    }

    } // switch
}

} // namespace

// ── OcrDjotMapper::fromOcrResults ─────────────────────────────────────────────

docmodel::SemanticDocument OcrDjotMapper::fromOcrResults(
    const QList<PageOcrResult>& results,
    const QString& pdfPath)
{
    // Build one Section per page, containing blocks ordered by readingOrderIndex.
    std::vector<std::shared_ptr<docmodel::Section>> sections;
    sections.reserve(static_cast<size_t>(results.size()));

    for (const auto& pageResult : results) {
        if (!pageResult.success && pageResult.layoutRegions.isEmpty()) {
            // Failed page: emit an empty section with an error paragraph
            docmodel::Provenance pageProv;
            pageProv.tag        = docmodel::ProvenanceTag::BornOCR;
            pageProv.source_file = pdfPath.toStdString();
            pageProv.page_index  = pageResult.pageIndex;

            std::vector<std::shared_ptr<docmodel::Block>> errBlocks;
            if (!pageResult.errorMessage.isEmpty()) {
                std::string errText = "[OCR error: " + pageResult.errorMessage.toStdString() + "]";
                auto errInline = std::make_shared<docmodel::TextInline>(errText, pageProv);
                errBlocks.push_back(std::make_shared<docmodel::TextBlock>(
                    docmodel::Block::Type::Paragraph,
                    std::vector<std::shared_ptr<docmodel::Inline>>{errInline},
                    pageProv));
            }

            sections.push_back(std::make_shared<docmodel::Section>(
                std::string(), // no section title
                std::move(errBlocks),
                std::vector<std::shared_ptr<docmodel::Section>>{},
                pageProv));
            continue;
        }

        // Sort layout regions by reading order
        QList<LayoutRegion> sortedRegions = pageResult.layoutRegions;
        std::sort(sortedRegions.begin(), sortedRegions.end(),
                  [](const LayoutRegion& a, const LayoutRegion& b) {
                      return a.readingOrderIndex < b.readingOrderIndex;
                  });

        // If no layout regions, produce a single paragraph from all words
        std::vector<std::shared_ptr<docmodel::Block>> blocks;
        if (sortedRegions.isEmpty()) {
            QList<MergedOcrWord> allWords = pageResult.words;
            sortWordsByReadingOrder(allWords);

            if (!allWords.isEmpty()) {
                QRectF pageBbox;
                for (const auto& w : allWords) pageBbox = pageBbox.united(w.boundingBox);
                docmodel::Provenance prov = makeProv(pdfPath, pageResult.pageIndex, pageBbox);
                auto inlines = buildInlines(allWords, pageResult.pageIndex, pdfPath);
                blocks.push_back(std::make_shared<docmodel::TextBlock>(
                    docmodel::Block::Type::Paragraph, std::move(inlines), prov));
            }
        } else {
            // Map each layout region to a Block
            for (const auto& region : sortedRegions) {
                auto block = regionToBlock(region, pageResult.words,
                                           pageResult.pageIndex, pdfPath);
                if (block) blocks.push_back(std::move(block));
            }
        }

        // Page provenance for the section
        docmodel::Provenance sectionProv;
        sectionProv.tag        = docmodel::ProvenanceTag::BornOCR;
        sectionProv.source_file = pdfPath.toStdString();
        sectionProv.page_index  = pageResult.pageIndex;

        sections.push_back(std::make_shared<docmodel::Section>(
            std::string(), // section title = empty (page sections have no title heading)
            std::move(blocks),
            std::vector<std::shared_ptr<docmodel::Section>>{},
            sectionProv));
    }

    // Document provenance
    docmodel::Provenance docProv;
    docProv.tag        = docmodel::ProvenanceTag::BornOCR;
    docProv.source_file = pdfPath.toStdString();
    docProv.page_index  = -1;

    return docmodel::SemanticDocument(std::move(sections), docProv);
}
