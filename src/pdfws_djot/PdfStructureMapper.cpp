// SPDX-License-Identifier: Apache-2.0
#include "PdfStructureMapper.h"
#include "docmodel/SemanticDocument.h"
#include <vector>

namespace pdfws {

PdfStructureMapper::PdfStructureMapper() = default;
PdfStructureMapper::~PdfStructureMapper() = default;

std::unique_ptr<docmodel::SemanticDocument> PdfStructureMapper::mapPdfToSemantic(const std::string& /*pdfFilePath*/) {
    // Stub: returns an empty SemanticDocument.
    // Full implementation will use PoDoFo/PDFium to extract PDF structure
    // and populate the docmodel AST.
    docmodel::Provenance prov;
    return std::make_unique<docmodel::SemanticDocument>(
        std::vector<std::shared_ptr<docmodel::Section>>{}, prov);
}

bool PdfStructureMapper::applySemanticToPdf(const docmodel::SemanticDocument& /*doc*/,
                                             const std::string& /*inputPdf*/,
                                             const std::string& /*outputPdf*/) {
    // Stub: not yet implemented
    return false;
}

} // namespace pdfws
