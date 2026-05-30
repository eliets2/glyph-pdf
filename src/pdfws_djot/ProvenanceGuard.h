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

class ProvenanceGuard {
public:
    ProvenanceGuardResult checkEditVia(docmodel::ProvenanceTag origin, bool isSigned, EditPath path) const;
};

} // namespace pdfws
