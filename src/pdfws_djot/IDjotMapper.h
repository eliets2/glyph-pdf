#pragma once

#include <memory>
#include <string>

// Forward declarations
namespace docmodel {
    class SemanticDocument;
}

namespace pdfws {

class IDjotMapper {
public:
    virtual ~IDjotMapper() = default;
    
    // Map from PDF structure (like PoDoFo pages/text) to SemanticDocument
    virtual std::unique_ptr<docmodel::SemanticDocument> mapPdfToSemantic(const std::string& pdfFilePath) = 0;
    
    // Map from SemanticDocument back to PDF structural edits
    virtual bool applySemanticToPdf(const docmodel::SemanticDocument& doc, const std::string& inputPdf, const std::string& outputPdf) = 0;
};

} // namespace pdfws
