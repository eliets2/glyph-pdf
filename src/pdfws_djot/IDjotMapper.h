// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>
#include <string>

// Forward declarations
namespace docmodel {
    class SemanticDocument;
}

namespace pdfws {

class ProvenanceToken;  // AR-9 D2: capability minted only by ProvenanceGuard

// Outcome of a SemanticDocument → PDF write. Distinguishes "the mapper does not
// support this edit" (NotSupported) from a real I/O/serialisation failure
// (WriteFailed) — replacing the old bare `false` that conflated the two.
enum class ApplyOutcome {
    Ok,
    NotSupported,
    WriteFailed
};

class IDjotMapper {
public:
    virtual ~IDjotMapper() = default;

    // Map from PDF structure (like PoDoFo pages/text) to SemanticDocument
    virtual std::unique_ptr<docmodel::SemanticDocument> mapPdfToSemantic(const std::string& pdfFilePath) = 0;

    // Map from SemanticDocument back to PDF structural edits.
    //
    // AR-9 D2: the `token` parameter is the type-level chokepoint. A
    // ProvenanceToken can only be obtained from ProvenanceGuard::mintApplyToken
    // (its constructor is private, friend to the guard), which throws
    // ProvenanceViolation for forbidden edits. Therefore no caller can reach
    // this lossy full-rewrite path without first passing the provenance gate.
    virtual ApplyOutcome applySemanticToPdf(const docmodel::SemanticDocument& doc,
                                            const std::string& inputPdf,
                                            const std::string& outputPdf,
                                            const ProvenanceToken& token) = 0;
};

} // namespace pdfws
