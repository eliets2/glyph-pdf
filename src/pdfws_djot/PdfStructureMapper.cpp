#include "PdfStructureMapper.h"

namespace pdfws {

PdfStructureMapper::PdfStructureMapper() = default;
PdfStructureMapper::~PdfStructureMapper() = default;

std::unique_ptr<docmodel::SemanticDocument> PdfStructureMapper::mapPdfToSemantic(const std::string& /*pdfFilePath*/) {
    // Stub: returns an empty SemanticDocument.
    // Full implementation will use PoDoFo/PDFium to extract PDF structure
    // and populate the docmodel AST.
    return std::make_unique<docmodel::SemanticDocument>();
}

bool PdfStructureMapper::applySemanticToPdf(const docmodel::SemanticDocument& /*doc*/,
                                             const std::string& /*inputPdf*/,
                                             const std::string& /*outputPdf*/) {
    // Stub: not yet implemented
    return false;
}

} // namespace pdfws
