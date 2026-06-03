// SPDX-License-Identifier: Apache-2.0
#pragma once
//
// PdfEncryptPubSec — forward header for the PubSec (certificate-based)
// PDF encryption class.
//
// The implementation lives in PdfEncryptPubSec.cpp, which is the ONLY
// translation unit that includes <podofo/podofo.h> under the #define
// private/protected public macro scope needed to subclass PdfEncrypt.
// All other TUs include only this header, which avoids propagating the
// ODR-violating macro to the rest of the build.
//
// DO NOT add #include <podofo/podofo.h> here.  The class is opaque to
// callers; the factory function returns a std::unique_ptr<PoDoFo::PdfEncrypt>
// which callers obtain via the forward-declared type from
// <podofo/main/PdfEncrypt.h> — a header that does NOT require the hack.
//
#include <memory>
#include <vector>
#include <QByteArray>

// Forward-declare PoDoFo::PdfEncrypt using only the sub-header that does NOT
// exercise the private/protected macros (PoDoFo 1.1 exposes this cleanly).
namespace PoDoFo { class PdfEncrypt; }

namespace gp {

/// Factory: construct a PdfEncryptPubSec configured with the given 32-byte
/// file-encryption key and the DER-encoded CMS /Recipients envelopes.
/// Returns a std::unique_ptr<PoDoFo::PdfEncrypt> suitable for passing to
/// PoDoFo::PdfMemDocument::SetEncrypt().
std::unique_ptr<PoDoFo::PdfEncrypt>
makePdfEncryptPubSec(const unsigned char fek[32],
                     const std::vector<QByteArray>& recipients);

} // namespace gp
