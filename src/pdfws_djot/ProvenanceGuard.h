// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdexcept>
#include <string>
#include "docmodel/ProvenanceTag.h"

namespace pdfws {

enum class EditPath {
    DirectStructural,
    DjotThenSave,
    DjotThenSaveAsCopy
};

class ProvenanceViolation : public std::runtime_error {
public:
    explicit ProvenanceViolation(const std::string& message)
        : std::runtime_error(message) {}
};

struct ProvenanceGuardResult {
    bool allowed = true;
    bool requiresWarning = false;
};

class ProvenanceGuard;  // fwd-decl for friendship

// ---------------------------------------------------------------------------
// ProvenanceToken — type-level capability proving the ProvenanceGuard approved
// a SemanticDocument → PDF write for a specific (origin, isSigned, path) tuple.
//
// AR-9 D2: this token is the chokepoint. Its constructor is PRIVATE; only
// ProvenanceGuard::mintApplyToken() (a friend) can create one, and it does so
// only after checkEditVia() succeeds (i.e. it throws ProvenanceViolation rather
// than minting a token for a forbidden edit). Because IDjotMapper::
// applySemanticToPdf() takes a `const ProvenanceToken&`, no caller can reach
// the lossy full-rewrite PDF path without first passing through the guard —
// the compiler enforces it. There is no public way to fabricate a token.
// ---------------------------------------------------------------------------
class ProvenanceToken {
public:
    docmodel::ProvenanceTag origin() const { return origin_; }
    bool isSigned() const { return isSigned_; }
    EditPath path() const { return path_; }

private:
    friend class ProvenanceGuard;
    ProvenanceToken(docmodel::ProvenanceTag origin, bool isSigned, EditPath path)
        : origin_(origin), isSigned_(isSigned), path_(path) {}

    docmodel::ProvenanceTag origin_;
    bool isSigned_;
    EditPath path_;
};

class ProvenanceGuard {
public:
    ProvenanceGuardResult checkEditVia(docmodel::ProvenanceTag origin, bool isSigned, EditPath path) const;

    // Mint a capability token authorising a SemanticDocument → PDF write.
    // Runs the same policy as checkEditVia(); throws ProvenanceViolation if the
    // edit is forbidden (signed + DjotThenSave). On success returns a token the
    // caller passes to IDjotMapper::applySemanticToPdf(). This is the ONLY way
    // to obtain a ProvenanceToken.
    ProvenanceToken mintApplyToken(docmodel::ProvenanceTag origin, bool isSigned, EditPath path) const;
};

} // namespace pdfws
