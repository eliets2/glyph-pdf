#include "IDjotMapper.h"

namespace pdfws {

class PdfStructureMapper : public IDjotMapper {
public:
    std::shared_ptr<docmodel::Document> MapAstToDocument(const std::string& astJson) override {
        // Stub implementation
        return nullptr;
    }
};

}
