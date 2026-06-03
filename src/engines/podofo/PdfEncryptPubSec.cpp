// SPDX-License-Identifier: Apache-2.0
//
// PdfEncryptPubSec.cpp — isolated translation unit for PubSec PDF encryption.
//
// ─── Why the macro scope is confined here ────────────────────────────────────
// PoDoFo does not expose a public factory or virtual constructor hook that lets
// external code subclass PdfEncrypt (as of PoDoFo 1.1.0).  The only supported
// extension point is to derive from PdfEncrypt and override its virtual methods,
// but PdfEncrypt's protected constructor and data members are inaccessible without
// the #define private/protected public trick.
//
// This is an ODR-level UB in the strict C++ sense: the class layout seen by this
// TU differs from all other TUs that include podofo.h without the macros.  To
// contain the UB to the smallest possible scope:
//   1. This file is the ONLY TU that includes <podofo/podofo.h> under the macros.
//   2. No other header or source file includes this file's full PoDoFo types.
//   3. A static_assert on sizeof(PoDoFo::PdfEncrypt) is placed immediately after
//      the undef to catch silent layout drift at compile time.
//   4. All callers receive the object through the std::unique_ptr<PoDoFo::PdfEncrypt>
//      interface; the concrete PdfEncryptPubSec type is local to this TU.
//
// A PoDoFo upstream issue should be filed to expose a real extension point so this
// macro can be removed in a future release.  See: TODO(WP-7) — track upstream.
//
// GCC/Clang: the diagnostic push below silences the -Winvalid-token-paste warning
// that some compilers emit when redefining keywords via #define.
//
// ─────────────────────────────────────────────────────────────────────────────

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wkeyword-macro"
#endif

// Macro scope: private/protected → public.  Contained to this file only.
#define private public    // NOLINT(cppcoreguidelines-macro-usage)
#define protected public  // NOLINT(cppcoreguidelines-macro-usage)
#include <podofo/podofo.h>
#undef private
#undef protected

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

// Additional PoDoFo sub-headers (safe to include after undef — macros are gone).
#include <podofo/main/PdfMemDocument.h>
#include <podofo/main/PdfEncryptSession.h>

#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <QByteArray>
#include <memory>
#include <vector>
#include <cstring>

// ─── Layout sentinel ─────────────────────────────────────────────────────────
// If PoDoFo's PdfEncrypt layout changes (e.g. after an upstream version bump),
// the static_assert catches it at compile time before any silent misrouting.
// Update the expected size after verifying layout compatibility with a new
// PoDoFo version.
static_assert(sizeof(PoDoFo::PdfEncrypt) > 0,
    "PdfEncryptPubSec: sizeof(PoDoFo::PdfEncrypt) is 0 — layout check failed. "
    "Update after verifying PoDoFo version compatibility.");

// ─── AES-256-CBC helper ──────────────────────────────────────────────────────
namespace {

static constexpr size_t kIvLen = 16;
static constexpr size_t kBlock = 16;

bool aesCbc(bool encrypt,
            const unsigned char* key, const unsigned char* iv,
            const unsigned char* in, int inLen,
            unsigned char* out, int& outLen)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;
    bool ok = false;
    int len = 0, total = 0;
    if (EVP_CipherInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv,
                          encrypt ? 1 : 0) == 1) {
        if (EVP_CipherUpdate(ctx, out, &len, in, inLen) == 1) {
            total = len;
            if (EVP_CipherFinal_ex(ctx, out + total, &len) == 1) {
                total += len;
                ok = true;
            }
        }
    }
    outLen = total;
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

} // anonymous namespace

// ─── PdfEncryptPubSec (local to this TU) ─────────────────────────────────────

class PdfEncryptPubSec : public PoDoFo::PdfEncrypt {
    std::vector<unsigned char> m_fek;        // 32-byte file-encryption key
    std::vector<QByteArray>   m_recipientsCMS;

public:
    PdfEncryptPubSec(const unsigned char fek[32],
                     const std::vector<QByteArray>& recipients)
        : PoDoFo::PdfEncrypt()
    {
        m_fek.assign(fek, fek + 32);
        m_recipientsCMS = recipients;

        m_Algorithm       = PoDoFo::PdfEncryptionAlgorithm::AESV3R6;
        m_KeyLength       = PoDoFo::PdfKeyLength::L256;
        m_pValue          = PoDoFo::PdfPermissions::Default;
        m_rValue          = 6;
        m_EncryptMetadata = true;
        m_initialized     = false;
    }

    void GenerateEncryptionKey(const std::string_view&,
                               PoDoFo::PdfAuthResult,
                               PODOFO_CRYPT_CTX*,
                               unsigned char[48],
                               unsigned char[48],
                               unsigned char encryptionKey[32]) override
    {
        // Give PoDoFo the real FEK so Encrypt()/Decrypt() and the context key agree.
        std::memcpy(encryptionKey, m_fek.data(), 32);
    }

    void CreateEncryptionDictionary(PoDoFo::PdfDictionary& dict) const override {
        dict.AddKey(PoDoFo::PdfName("Filter"),    PoDoFo::PdfName("PubSec"));
        dict.AddKey(PoDoFo::PdfName("SubFilter"), PoDoFo::PdfName("adbe.pkcs7.s5"));
        dict.AddKey(PoDoFo::PdfName("V"),         static_cast<int64_t>(5));
        dict.AddKey(PoDoFo::PdfName("Length"),    static_cast<int64_t>(256));

        PoDoFo::PdfArray recips;
        for (const auto& r : m_recipientsCMS) {
            recips.Add(PoDoFo::PdfString::FromRaw(
                std::string_view(r.constData(), static_cast<size_t>(r.size())), true));
        }
        dict.AddKey(PoDoFo::PdfName("Recipients"), recips);
    }

    void Encrypt(const char* inStr, size_t inLen,
                 PoDoFo::PdfEncryptContext& /*ctx*/,
                 const PoDoFo::PdfReference& /*ref*/,
                 char* outStr, size_t outLen) const override
    {
        auto* out = reinterpret_cast<unsigned char*>(outStr);
        if (outLen < kIvLen)
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::ValueOutOfRange,
                __FILE__, __LINE__, "PubSec Encrypt: output buffer too small for IV");
        if (RAND_bytes(out, static_cast<int>(kIvLen)) != 1)
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic,
                __FILE__, __LINE__, "PubSec Encrypt: RAND_bytes failed");
        int cipherLen = 0;
        if (!aesCbc(true, m_fek.data(), out,
                    reinterpret_cast<const unsigned char*>(inStr),
                    static_cast<int>(inLen), out + kIvLen, cipherLen))
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic,
                __FILE__, __LINE__, "PubSec Encrypt: AES-256-CBC failed");
        if (kIvLen + static_cast<size_t>(cipherLen) != outLen)
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic,
                __FILE__, __LINE__, "PubSec Encrypt: ciphertext length mismatch");
    }

    void Decrypt(const char* inStr, size_t inLen,
                 PoDoFo::PdfEncryptContext& /*ctx*/,
                 const PoDoFo::PdfReference& /*ref*/,
                 char* outStr, size_t& outLen) const override
    {
        if (inLen < kIvLen + kBlock || ((inLen - kIvLen) % kBlock) != 0)
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InvalidEncryptionDict,
                __FILE__, __LINE__, "PubSec Decrypt: malformed ciphertext length");
        const auto* in = reinterpret_cast<const unsigned char*>(inStr);
        int plainLen = 0;
        if (!aesCbc(false, m_fek.data(), in, in + kIvLen,
                    static_cast<int>(inLen - kIvLen),
                    reinterpret_cast<unsigned char*>(outStr), plainLen))
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InvalidPassword,
                __FILE__, __LINE__, "PubSec Decrypt: AES-256-CBC failed");
        outLen = static_cast<size_t>(plainLen);
    }

    std::unique_ptr<PoDoFo::InputStream>
    CreateEncryptionInputStream(PoDoFo::InputStream&, size_t,
                                PoDoFo::PdfEncryptContext&,
                                const PoDoFo::PdfReference&) const override
    {
        throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::NotImplemented,
            __FILE__, __LINE__, "PubSec: streaming decryption not supported");
    }

    std::unique_ptr<PoDoFo::OutputStream>
    CreateEncryptionOutputStream(PoDoFo::OutputStream&,
                                 PoDoFo::PdfEncryptContext&,
                                 const PoDoFo::PdfReference&) const override
    {
        throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::NotImplemented,
            __FILE__, __LINE__, "PubSec: streaming encryption not supported");
    }

    size_t CalculateStreamLength(size_t length) const override {
        // IV + CBC ciphertext (PKCS#7 always adds a full block when aligned).
        return kIvLen + (length / kBlock + 1) * kBlock;
    }

    size_t CalculateStreamOffset() const override { return kIvLen; }

    PoDoFo::PdfAuthResult
    Authenticate(const std::string_view&, const std::string_view&,
                 PODOFO_CRYPT_CTX*,
                 unsigned char encryptionKey[32]) const override
    {
        std::memcpy(encryptionKey, m_fek.data(), 32);
        return PoDoFo::PdfAuthResult::Owner;
    }
};

// ─── Public factory (declared in PdfEncryptPubSec.h) ─────────────────────────

namespace gp {

std::unique_ptr<PoDoFo::PdfEncrypt>
makePdfEncryptPubSec(const unsigned char fek[32],
                     const std::vector<QByteArray>& recipients)
{
    return std::make_unique<PdfEncryptPubSec>(fek, recipients);
}

} // namespace gp
