// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "SemanticDocument.h"
#include <memory>

namespace docmodel {

class DocumentFuzzer {
public:
    static std::shared_ptr<SemanticDocument> generateRandomDocument(int seed, int max_depth = 3);
};

} // namespace docmodel
