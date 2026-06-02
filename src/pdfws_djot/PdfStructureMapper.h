// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "IDjotMapper.h"

namespace pdfws {

class PdfStructureMapper : public IDjotMapper {
public:
    PdfStructureMapper();
    ~PdfStructureMapper() override;

    std::unique_ptr<docmodel::SemanticDocument> mapPdfToSemantic(const std::string& pdfFilePath) override;
    bool applySemanticToPdf(const docmodel::SemanticDocument& doc, const std::string& inputPdf, const std::string& outputPdf) override;
};

} // namespace pdfws
