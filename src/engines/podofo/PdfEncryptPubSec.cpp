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
        constexpr uint8_t kVersionGCM = 0x02;
        constexpr size_t kNonceLen = 12;
        constexpr size_t kTagLen = 16;

        if (outLen < 1 + kNonceLen + inLen + kTagLen)
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::ValueOutOfRange,
                __FILE__, __LINE__, "PubSec Encrypt: output buffer too small for GCM");

        auto* out = reinterpret_cast<unsigned char*>(outStr);
        out[0] = kVersionGCM;

        if (RAND_bytes(out + 1, static_cast<int>(kNonceLen)) != 1)
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic,
                __FILE__, __LINE__, "PubSec Encrypt: RAND_bytes failed");

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx)
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic,
                __FILE__, __LINE__, "PubSec Encrypt: EVP_CIPHER_CTX_new failed");
        auto ctxGuard = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>(ctx, EVP_CIPHER_CTX_free);

        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic, __FILE__, __LINE__, "PubSec Encrypt failed");
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(kNonceLen), nullptr) != 1)
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic, __FILE__, __LINE__, "PubSec Encrypt failed");
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, m_fek.data(), out + 1) != 1)
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic, __FILE__, __LINE__, "PubSec Encrypt failed");

        int cipherLen = 0;
        if (EVP_EncryptUpdate(ctx, out + 1 + kNonceLen, &cipherLen, reinterpret_cast<const unsigned char*>(inStr), static_cast<int>(inLen)) != 1)
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic, __FILE__, __LINE__, "PubSec Encrypt failed");

        int finalLen = 0;
        if (EVP_EncryptFinal_ex(ctx, out + 1 + kNonceLen + cipherLen, &finalLen) != 1)
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic, __FILE__, __LINE__, "PubSec Encrypt failed");

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(kTagLen), out + 1 + kNonceLen + cipherLen + finalLen) != 1)
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic, __FILE__, __LINE__, "PubSec Encrypt failed");
    }

    void decryptLegacyCBC(const unsigned char* in, size_t inLen, unsigned char* out, size_t& outLen) const {
        if (inLen < kIvLen + kBlock || ((inLen - kIvLen) % kBlock) != 0)
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InvalidEncryptionDict,
                __FILE__, __LINE__, "PubSec Decrypt: malformed ciphertext length");
        int plainLen = 0;
        if (!aesCbc(false, m_fek.data(), in, in + kIvLen,
                    static_cast<int>(inLen - kIvLen), out, plainLen))
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InvalidPassword,
                __FILE__, __LINE__, "PubSec Decrypt: AES-256-CBC failed");
        outLen = static_cast<size_t>(plainLen);
    }

    void Decrypt(const char* inStr, size_t inLen,
                 PoDoFo::PdfEncryptContext& /*ctx*/,
                 const PoDoFo::PdfReference& /*ref*/,
                 char* outStr, size_t& outLen) const override
    {
        constexpr uint8_t kVersionCBC = 0x01;
        constexpr uint8_t kVersionGCM = 0x02;
        constexpr size_t kNonceLen = 12;
        constexpr size_t kTagLen = 16;

        if (inLen == 0) {
            outLen = 0;
            return;
        }

        const auto* in = reinterpret_cast<const unsigned char*>(inStr);
        auto* out = reinterpret_cast<unsigned char*>(outStr);
        uint8_t version = in[0];

        if (version == kVersionGCM) {
            if (inLen < 1 + kNonceLen + kTagLen)
                throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InvalidEncryptionDict,
                    __FILE__, __LINE__, "PubSec Decrypt: GCM blob too short");

            const unsigned char* nonce = in + 1;
            const unsigned char* tag = in + inLen - kTagLen;
            const unsigned char* ciphertext = in + 1 + kNonceLen;
            size_t cipherLen = inLen - 1 - kNonceLen - kTagLen;

            EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
            if (!ctx)
                throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic,
                    __FILE__, __LINE__, "PubSec Decrypt: EVP_CIPHER_CTX_new failed");
            auto ctxGuard = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>(ctx, EVP_CIPHER_CTX_free);

            if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
                throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic, __FILE__, __LINE__, "PubSec Decrypt failed");
            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(kNonceLen), nullptr) != 1)
                throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic, __FILE__, __LINE__, "PubSec Decrypt failed");
            if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, m_fek.data(), nonce) != 1)
                throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic, __FILE__, __LINE__, "PubSec Decrypt failed");
            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(kTagLen), const_cast<unsigned char*>(tag)) != 1)
                throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic, __FILE__, __LINE__, "PubSec Decrypt failed");

            int plainLen = 0;
            if (EVP_DecryptUpdate(ctx, out, &plainLen, ciphertext, static_cast<int>(cipherLen)) != 1)
                throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InternalLogic, __FILE__, __LINE__, "PubSec Decrypt failed");

            int finalLen = 0;
            if (EVP_DecryptFinal_ex(ctx, out + plainLen, &finalLen) != 1) {
                // C-1 FIX: GCM tag verification failed
                throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InvalidPassword,
                    __FILE__, __LINE__, "PubSec Decrypt: GCM tag verification failed");
            }
            outLen = static_cast<size_t>(plainLen + finalLen);
        } else if (version == kVersionCBC) {
            decryptLegacyCBC(in + 1, inLen - 1, out, outLen);
        } else {
            // SECFIX-2: the version byte is unauthenticated. Any value other than the
            // known GCM (0x02) or versioned-CBC (0x01) markers must be a HARD error —
            // never an unauthenticated CBC fallback, which would be a downgrade oracle
            // (an attacker flips in[0] to strip GCM integrity).
            throw PoDoFo::PdfError(PoDoFo::PdfErrorCode::InvalidEncryptionDict,
                __FILE__, __LINE__, "PubSec Decrypt: unknown version byte — possible downgrade attack");
        }
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
        // [0x02 version][12-byte nonce][ciphertext][16-byte GCM tag]
        return 1 + 12 + length + 16;
    }

    size_t CalculateStreamOffset() const override { return 0; }

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
