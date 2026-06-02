// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "IDjotCodec.h"
#include <string>

namespace pdfws {

class LuaDjotCodec : public IDjotCodec {
public:
    // djotLibPath: filesystem path to the directory containing djot.lua
    explicit LuaDjotCodec(const std::string& djotLibPath);
    ~LuaDjotCodec() override;

    std::string documentToDjot(const docmodel::SemanticDocument& doc) override;
    std::unique_ptr<docmodel::SemanticDocument> djotToDocument(const std::string& djotText) override;

private:
    std::string m_djotLibPath;
};

} // namespace pdfws
