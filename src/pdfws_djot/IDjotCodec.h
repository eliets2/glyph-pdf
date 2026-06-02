// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <memory>

// Forward declarations for docmodel
namespace docmodel {
    class SemanticDocument;
}

namespace pdfws {

class IDjotCodec {
public:
    virtual ~IDjotCodec() = default;

    virtual std::string documentToDjot(const docmodel::SemanticDocument& doc) = 0;
    virtual std::unique_ptr<docmodel::SemanticDocument> djotToDocument(const std::string& djotText) = 0;
};

} // namespace pdfws
