# Workstream A — Security: Cryptographic Correctness + PDF Logic Flaws + Parser Memory Safety

Audit of GlyphPDF @ git HEAD `e1c5394` (CLAUDE.md claims `e09404d` — documented drift). Read-only. C++17 / Qt 6.11 / PoDoFo 1.1.0 / PDFium / qpdf / OpenSSL 3.x. Scope: SignatureManager (PAdES B-LTA), redaction excision, AES-256 + certificate encryption, sanitization, content-stream injection, three-backend parser trust boundary.

## Summary

The signing path is the strongest area: the ByteRange validation is genuinely defended against shadow/overlap attacks, the DSS/VRI key is now correctly SHA-1 over the RAW Contents octets (CLAUDE.md section 7 Pattern 7 TODO is closed), CMS_verify runs a real two-pass trust policy against the Windows system root with EKU/expiry/weak-key/signing-time checks, OCSP is verified with OCSP_basic_verify before DSS embedding, and qpdf is correctly excluded from the signing path. The redaction TEXT excision (PoDoFoBackend::redactCanvasRecursively) is real content-stream surgery with Edact-Ray TJ-gap normalization and OCR Tr==3 scrub, and is byte-verified by tests — a meaningful improvement over black-rectangle redaction. However, two findings are serious: certificate-based encryption (PdfEncryptPubSec) encrypts document streams with an all-zero AES key while the Recipients envelopes carry the real FEK, producing files no conformant reader can decrypt (the M1-B8 fix is unverified — its test never decrypts), and image (bitmap) XObjects are not excised by redaction (the underlying image stream survives in the saved file and the Do intersection test uses the text cursor, not the image CTM). Medium issues include an unclamped render-allocation DoS in PdfiumBackend, unescaped backslash in FDF form-data export, and the text-search Redact-All path using a fixed 80x18pt box that under-covers wide matches. The dead, never-excising legacy PdfViewerWidget::applyRedactions(QString) token parser (the source of the flagged unused currentX/currentY/insideRedactedZone vars) is unreachable but should be deleted before OSS release.

## Findings

### A-01 · Certificate encryption writes streams under an all-zero AES key (PubSec output undecryptable) · Critical · Med · `src/engines/PdfEditorEngine.cpp:706-779`

`PdfEncryptPubSec` overrides `GenerateEncryptionKey()` to copy the real 32-byte FEK into the out-param, but `Encrypt()/Decrypt()/CreateEncryptionOutputStream()` delegate to a separate `m_delegate` object created via `PdfEncrypt::Create("dummy","dummy", ...)`:

```cpp
m_delegate = PoDoFo::PdfEncrypt::Create("dummy", "dummy", ...AESV3R6);
...
void GenerateEncryptionKey(..., unsigned char encryptionKey[32]) override {
    memcpy(encryptionKey, m_fek.data(), 32);          // sets the PubSec object key
}
void Encrypt(...) const override {
    m_delegate->Encrypt(inStr, inLen, context, objref, outStr, outLen);  // uses m_delegate key
}
```

PoDoFo populates a PdfEncrypt internal `m_encryptionKey` (header `PdfEncrypt.h:489`, read via `GetEncryptionKey()`) only when THAT object `GenerateEncryptionKey` runs at save time. PoDoFo calls `GenerateEncryptionKey` on the document encrypt object (the PdfEncryptPubSec), never on `m_delegate`. `Create()` does not generate a key. Therefore `m_delegate->m_encryptionKey` remains zero-initialized, and every stream/string is AES-encrypted with an all-zero key, while the Recipients CMS envelopes carry the real `fek` (SHA-256 of the 24-byte seed). A certificate holder recovers `fek`, which does not match the zero key, so no conformant PubSec reader (Acrobat, etc.) can open the file.

`testCertificateEncryption` (`tests/TestEncryption.cpp:79-149`) only asserts the presence of `/Filter /PubSec`, `/SubFilter`, `/Recipients` — it never decrypts — so this passes despite total breakage (CLAUDE.md section 7 Pattern 5 mock-data-as-real: here a structural check masquerading as a round-trip proof).

Secondary defects in the same function: (a) the FEK derivation `SHA256(seed[20] || perms[4])` does not follow the adbe.pkcs7.s5 spec, which hashes the 20-byte seed followed by each recipient PKCS#7 bytes and the permission bytes; (b) `#define private public` / `#define protected public` (lines 697-701) to reach PoDoFo internals is an ABI-fragility landmine that will silently break on any PoDoFo point upgrade.

**Impact/Exploitability:** Not attacker-exploitable; it is a fails-closed data-loss / feature-broken defect. Severity is Critical because the feature is shipped as working, advertised, and silently produces permanently unreadable encrypted documents — a user who certificate-encrypts a sensitive file and deletes the plaintext loses it irrecoverably.

**Proposed Fix:** Do not delegate cryptographic operations to a second key-holder. Either (1) install `fek` into the delegate key slot so `m_delegate->GetEncryptionKey()` returns `fek`, or (2) drop the delegate and let `PdfEncryptPubSec` derive from `PdfEncryptAESBase` directly so the single object both reports `fek` and encrypts with it:

```cpp
// after m_delegate = Create(...): force fek into the delegate key slot
std::memcpy(const_cast<unsigned char*>(m_delegate->GetEncryptionKey()), m_fek.data(), 32);
```

And FIX THE TEST to actually load + decrypt with the private key (CMS-unwrap the recipient envelope, AES-decrypt one stream) and assert recovered plaintext equals input. Replace the FEK derivation with the spec algorithm (seed || recipient-bytes, SHA-256 for V5). Remove `#define private public`; add the needed accessor upstream or vendor a thin shim.

---

### A-02 · Redaction does not excise image (bitmap) XObjects — image data survives; Do intersection uses the text cursor · High · High · `src/engines/podofo/PoDoFoBackend.cpp:1234-1261`, `:1290-1345`

For an image placed via `... cm /Im0 Do`, `redactCanvasRecursively` only drops the `Do` operator if `isIntersectingSpan(textX, textX, textY)` is true:

```cpp
if (kw == "Do" && stack.size() > 0 && stack[0].IsName()) {
    std::string xobjName(stack[0].GetName().GetString());
    if (isIntersectingSpan(textX, textX, textY)) { // Simple check for image position
        ... continue;                               // drops the Do; image OBJECT stays in file
    }
    ... // recurses only into Form XObjects (Subtype==Form); Image XObjects: nothing
```

Two defects compound:
1. WRONG COORDINATE. `textX/textY` is the text cursor from Tm/Td, not the image placement. An image positioned by its own `cm` matrix (the normal case) is at coordinates unrelated to `textX/textY`, so the intersection test is essentially never correct for images. A redaction box squarely over a photo will usually fail to drop its `Do`.
2. OBJECT SURVIVES even when `Do` is dropped. Dropping the `Do` removes only the reference; the image XObject indirect object remains in the document graph. `PoDoFoBackend::saveDocument` (`:202-217`) calls `PdfMemDocument::Save` with default options and performs NO garbage collection (`removeUnusedObjects` is a separate, sanitize-only flag at `:2947`). The bitmap is recoverable from the saved PDF with any extraction tool.

There is no test for bitmap-XObject redaction — the only XObject test (`tests/TestRedaction.cpp:195-225`) redacts TEXT inside a Form XObject, not an image. This directly defeats the CLAUDE.md section 6 non-negotiable "Black rectangle redaction: NEVER — always excise from content stream," for images.

**Impact/Exploitability:** A user redacts a sensitive photo/scan/signature image; the visible result is a black box, but the original image bytes remain in the file and are trivially recoverable (NSA "Redacting with Confidence" incident class). The SecurityController dialog (`src/shell/controllers/SecurityController.cpp:372-376`) does warn "cannot guarantee secure removal of ... images," which mitigates legal exposure but contradicts the privacy-first redaction positioning.

**Proposed Fix:** (1) Track the full graphics CTM (the `cm` stack) so image placement is computed from the transformed unit square, not the text cursor; test the transformed image bbox against `pdfRects`. (2) When an image `Do` is redacted, replace the image XObject stream with a 1x1 opaque pixel (or remove the XObject and its `/XObject` resource entry) so no recoverable pixels remain. (3) After redaction, run a CollectGarbage-equivalent (or route the redacted save through the qpdf object-pruning rebuild already used by `sanitizeDocument`) so orphaned image objects are dropped. (4) Add `testRedactionRemovesImageBytes` that embeds a known-magic image, redacts over it, and asserts the magic bytes are absent from the saved file.

---

### A-03 · PdfiumBackend renderPage/renderTile allocate from attacker-controlled page dimensions x DPI with no clamp (memory-exhaustion DoS / int overflow) · Medium · High · `src/engines/pdfium/PdfiumBackend.cpp:94-104`, `:124-130`

```cpp
double width  = FPDF_GetPageWidthF(page);   // attacker-controlled MediaBox/UserUnit
double height = FPDF_GetPageHeightF(page);
qreal scale = dpi / 72.0;
int widthPixels  = static_cast<int>(width  * scale);   // no upper bound
int heightPixels = static_cast<int>(height * scale);
QImage image(widthPixels, heightPixels, QImage::Format_ARGB32);   // up to GBs, or negative dims on overflow
```

PDF allows MediaBox dimensions up to 14400 user units and a `/UserUnit` multiplier that pushes effective page size into the millions. At 300 dpi a 14400x14400 page is 60000x60000 px (~14.4 GB for one ARGB32 buffer); larger `/UserUnit` overflows the double-to-int conversion to a negative/wrapped value, yielding a QImage with bogus dimensions and an oversized bytesPerLine, then `FPDFBitmap_CreateEx(..., image.bits(), image.bytesPerLine())` writes into it. Opening a single crafted PDF can OOM-kill the app or trigger UB in the bitmap fill.

**Impact/Exploitability:** Attacker-supplied PDF, hit on first render (opening the document). Worst realistic outcome is a reliable crash/DoS; the negative-dimension path is UB but PDFium/Qt typically reject before a controllable overflow, so RCE potential Low, DoS High. Heuristic caveat: not fuzzed; verdict from code reading, not a sanitizer crash.

**Proposed Fix:** Clamp before allocating:

```cpp
constexpr int kMaxDim = 20000;
constexpr long long kMaxPixels = 120000000LL;
long long wpx = llround((double)width  * scale);
long long hpx = llround((double)height * scale);
if (wpx <= 0 || hpx <= 0 || wpx > kMaxDim || hpx > kMaxDim || wpx*hpx > kMaxPixels) {
    qWarning() << "render: page too large, refusing"; return QImage();
}
```

Apply identically in renderTile. Surface an error to the UI rather than silently returning a blank page.

---

### A-04 · FDF form-data export escapes ( ) but not backslash — output corruption / structural break · Medium · High · `src/engines/FormManager.cpp:457-466`

```cpp
QString k = it.key();
k.replace(")", "\)").replace("(", "\(");   // backslash NOT escaped
QString v = it.value().toString();
v.replace(")", "\)").replace("(", "\(");
out << "<< /T (" << k << ") /V (" << v << ") >>\n";
```

A field value containing a literal backslash is emitted unescaped into a PDF literal string. `a\b` becomes `(a\b)` where `\b` is a PDF backspace escape (data corruption); a value ending in backslash produces `...\)` which escapes the intended closing paren and breaks the FDF object structure. Field names/values can originate from an attacker-supplied PDF AcroForm, so a crafted form can produce malformed or content-spoofed FDF on export. This violates the CLAUDE.md section 6 non-negotiable "Content-stream operators with raw user strings: NEVER — always escape via pdfEscapeLiteralString": that helper exists and handles backslash correctly, but is NOT used here (the only un-converted user-to-PDF-string site found in the audit).

**Impact/Exploitability:** Not memory-safety; data integrity + minor injection in the FDF sidecar (not the signed PDF). User-initiated export, attacker-influenced field data.

**Proposed Fix:** Replace the ad-hoc replace() calls with the existing escaper at both /T and /V:

```cpp
#include "engines/podofo/PdfStringEscape.h"
out << "<< /T (" << pdfEscapeLiteralString(it.key()) << ") /V ("
    << pdfEscapeLiteralString(it.value().toString()) << ") >>\n";
```

(Backslash must be escaped before ( and ); pdfEscapeLiteralString already does this in the correct order.)

---

### A-05 · Redact-All-matches uses a fixed 80x18pt box; wide matches under-covered, recoverable residue · Medium · Med · `src/ui/PdfViewerWidget.cpp:333-360`

```cpp
item.rect = QRectF(pos.x(), pos.y() - 15, 80, 18);   // fixed size, ignores match width
```

`redactAllMatches` (driven by RedactCommand, the search-and-redact UX) builds a redaction annotation with a hardcoded 80pt-wide / 18pt-tall rectangle per hit, with the comment "In a production build, we would use character box data." Because the downstream excision in `PoDoFoBackend::applyRedactions` is rectangle-based (isIntersectingSpan), any matched text wider than 80pt (long emails/IBANs, large fonts) is only partially covered: the visual black box and the content-stream excision both stop at 80pt, leaving the tail glyphs visible-adjacent and recoverable from the stream. Note: `PatternRedactor::findMatches` (`src/engines/PatternRedactor.cpp:173-235`) DOES compute true merged char-box rects via PDFium — but redactAllMatches does not use it; it uses QPdfSearchModel point locations with a constant box.

**Impact/Exploitability:** Incomplete redaction of exactly the PII the feature targets. User-initiated; the failure is silent (looks redacted).

**Proposed Fix:** Replace the constant box with real geometry. Route search-redaction through `PatternRedactor::extractCharsWithPositions` / `mergeCharBoxes` (already implemented for pattern redaction) to get the true per-match bounding rectangle, or query QPdfSearchModel for the full bounding polygon rather than a single point. At minimum, derive width from matched substring length x font metrics instead of the constant 80.

---

### A-06 · Dead, never-excising legacy redaction parser retained (PdfViewerWidget::applyRedactions(QString)) · Low · High · `src/ui/PdfViewerWidget.cpp:947-1151`

This 200-line overload is the origin of the build-flagged unused variables currentX, currentY, insideRedactedZone (declared `:988-990`, never used). Its own comments admit it cannot selectively filter operators and it falls back to whitespace tokenization (`while (iss >> token)`) that mis-parses any PDF string containing spaces or binary, only checks Tj/TJ against the LAST text position, never recurses into XObjects, and ends by painting black rectangles. It is a textbook insecure redaction implementation. Repo-wide grep confirms ZERO callers in src/ and tests — the live path is the engine-side `PoDoFoBackend::applyRedactions`. It still compiles into the binary.

**Impact/Exploitability:** None today (unreachable). Risk is future regression: a later UI wiring could call this overload believing it redacts, silently shipping black-rectangle redaction. Keeping it contradicts section 6.

**Proposed Fix:** Delete the `applyRedactions(const QString&)` declaration (`src/ui/PdfViewerWidget.h:78`) and definition (`:947-1151`). All redaction must flow through `IPdfEditorEngine::applyRedactions(int, const QList<QRectF>&)`. If a viewer-level convenience is wanted, make it a thin forwarder to the engine.

---

### A-07 · pdfEscapeLiteralString idempotency heuristic corrupts genuine backslash-letter user input · Low · Med · `src/engines/podofo/PdfStringEscape.cpp:14-23`

```cpp
if (c == '\' && i + 1 < input.size()) {
    char next = input[i + 1];
    if (next == '(' || next == ')' || next == '\' || next=='n' || next=='r' ||
        next=='t' || next=='b' || next=='f') {
        result += c; result += next; ++i; continue;   // treats user \n as pre-escaped
    }
}
```

The "already escaped" shortcut assumes any backslash followed by n/r/t/b/f/(/)/backslash is a prior escape and passes it through verbatim. But user-typed watermark/header text legitimately containing backslash + n is emitted as `\n` (a newline escape) instead of the correct `\n` (literal backslash, literal n). Result: a watermark "C:\notes" renders with a newline where `\n` was. This is a correctness bug, not an injection (the output remains a valid PDF string — no ) or backslash can leak out, so the section 6 injection guarantee holds), but it silently mangles user content.

**Impact/Exploitability:** Cosmetic/data-integrity only; no security boundary crossed (verified: the function never emits an unbalanced ) or stray backslash for arbitrary input).

**Proposed Fix:** Remove the idempotency special-case and always escape from raw input; the call sites at `PoDoFoBackend.cpp:143/2423/2679` and header/footer/Bates all pass raw text exactly once. If idempotency is genuinely required somewhere, track "already-escaped" with an explicit flag/parameter rather than guessing from byte patterns.

---

### A-08 · ByteRange validator does not constrain the contents of the signature gap (defense-in-depth) · Low · Med · `src/engines/SignatureManager.cpp:894-936`

The validator correctly enforces non-negative offsets, in-bounds ranges, off1==0, off2+len2==fileSize, and non-overlap off1+len1 <= off2 (`:899-921`) — a solid anti-shadow baseline. The gap [off1+len1, off2) is where the hex Contents string lives, but the code does not verify that the gap contains ONLY that signature string. A PDF can be crafted so the gap holds additional bytes (e.g. padding objects) beyond the Contents literal; those bytes are excluded from the signed signedData yet are part of the file. PoDoFo tight placeholder layout makes this hard to weaponize, but a strict validator should bound the gap to the Contents token length.

**Impact/Exploitability:** Low. Crafting exploitable content in the gap that a renderer also honors is difficult given the gap sits inside the signature dictionary; included as defense-in-depth, not a demonstrated bypass.

**Proposed Fix:** After locating Contents, assert gapStart equals the byte offset of the `<` opening the Contents hex string and gapEnd equals the offset just past its closing `>`, i.e. the gap length equals `2 + contentsHexLen` (plus delimiter). Reject with trustStatus=Malformed otherwise.

---

### A-09 · extractSignatureContentsRaw / extractOcspFromDss return the first signature/OCSP only — wrong VRI key + revocation for multi-signature PDFs · Info · High · `src/engines/SignatureManager.cpp:574-590`, `:544-569`

Both helpers iterate and return on the FIRST signature field (`:578-588`) or first OCSPs entry (`:558-565`). For a document with two or more signatures, buildDssDictionary keys the VRI dictionary by the first signature Contents SHA-1 regardless of which signature the cert/OCSP data belongs to, and validateSignatures (`:1104`) reports the first OCSP revocation status for every signature. The code comments acknowledge this is a stub pending M5 full VRI-to-field correlation.

**Impact/Exploitability:** Not a bypass on single-signature documents (the common case). On multi-signature documents it produces incorrect LTV metadata and could mislabel a revoked signature as valid (or vice-versa). Flagged Info because it is documented-incomplete and out of the single-signature happy path, but it must be closed before B-LTA is advertised for multi-party workflows.

**Proposed Fix:** Key VRI and OCSP correlation by each signature own Contents SHA-1. Iterate all signature fields, compute the per-signature VRI key, attach only that signature cert/OCSP/CRL. In validateSignatures, match the embedded OCSP certID (issuer name hash + serial) against the signer cert rather than taking OCSP_resp_get0(basic, 0).

---

## Section 6 Non-negotiables: enforced-in-code check

| Section 6 Non-negotiable | Enforced in code? | Evidence / Gap |
|---|---|---|
| SHA-256 only for sig hashing (SHA-1 only for VRI key per ETSI) | YES | `SignatureManager.cpp:643` Hashing=SHA256; TSA imprint `:165` NID_sha256; VRI `:405` Sha1 over raw Contents only |
| DSS/VRI key = SHA-1 of RAW Contents bytes (no hex round-trip) | YES (Pattern 7 closed) | `:402-406` hashes sigContentsRaw (raw octets); hex value discarded `:707` `auto [raw, hexUnused]` |
| CMS_verify with real trust policy (Win root + VERIFY_PARAM + EKU + CRL) | YES | `:128-142` CertOpenSystemStoreA("ROOT"); `:971-977` VERIFY_PARAM + CRL flags + SMIME purpose; `:1018-1029` EKU; two-pass `:980-988` |
| OCSP verified (OCSP_basic_verify) BEFORE DSS embed | YES | `:741` OCSP_basic_verify(...)==1 gates ocsps.append before buildDssDictionary at `:758` |
| Incremental save (SaveUpdate/Append) when signatures exist | YES | First sig full SaveOnSigning `:692-694` (correct PoDoFo signing flow); DSS `:459-460` and DocTimeStamp `:528-529` use FileMode::Append + SaveUpdate |
| qpdf NEVER in signing path | YES | SignatureManager.cpp has zero qpdf refs; export-linearize guard PdfEditorEngine.cpp:173-191 skips qpdf when hasSignatures |
| Black-rectangle redaction NEVER (excise from stream) — TEXT | YES | redactCanvasRecursively excises Tj/TJ; byte-verified TestRedaction.cpp:187,224,360; binary/inline-image stream aborts (fail-closed) PoDoFoBackend.cpp:1340-1345 |
| Black-rectangle redaction NEVER — IMAGES | NO (A-02) | Image XObject bytes survive; Do test uses text cursor; no GC on save; no image-redaction test |
| Edact-Ray glyph-advance normalization | YES | `:1207-1225` numeric-only [N] TJ gap; GlyphAdvanceCalculator sums advances; TestRedaction.cpp:227-252 |
| OCR invisible-text scrub (Tr==3) | YES | `:1078-1080` tracks Tr; `:1192-1205` drops invisible glyphs without advance substitution |
| Content-stream operators with raw user strings NEVER (escape) | PARTIAL | Watermark/header/footer/Bates/annotation use pdfEscapeLiteralString (verified); FDF export FormManager.cpp:464 does not escape backslash (A-04) |
| AES-256 password encryption (V5/R6) | YES | PoDoFoBackend.cpp:1537-1542 SetEncrypted(...AESV3R6); TestEncryption.cpp:47-48 asserts /V 5 /R 6 |
| Certificate (PubSec) encryption real /Filter /PubSec (M1-B8) | PARTIAL/BROKEN | Dictionary correct (PdfEditorEngine.cpp:734-745) but streams encrypted under zero key (A-01); UAF appears resolved (RAII SkX509Ptr/CmsPtr/BioPtr) |
| RSA key-size enforcement (>=2048) | PARTIAL | Enforced for SIGNING leaf cert SignatureManager.cpp:629-639 and validation `:1002-1008`; NOT enforced on cert-ENCRYPTION recipient certs (encryptWithCertificate accepts any RSA size) |
| Telemetry without opt-in NEVER | YES (not re-audited here) | No telemetry calls found in security paths |

## Verified vs Could-Not-Verify

Verified (read + cross-checked against tests):
- ByteRange shadow/overlap/bounds validation logic (SignatureManager.cpp:894-921) — correct.
- VRI key = SHA-1 of raw Contents (Pattern 7 regression closed) — confirmed `:402-406`.
- CMS_verify two-pass trust policy, Windows root, EKU/expiry/weak-key/signing-time, OCSP pre-embed verification — present and ordered correctly.
- qpdf excluded from signing; incremental SaveUpdate for post-sign DSS/timestamp — confirmed.
- Redaction TEXT excision is real content surgery, byte-verified for simple + Form-XObject + CID fonts; fail-closed on binary/inline-image streams.
- Sanitization strips ~20 vectors (Info/Metadata/PieceInfo/Names/EmbeddedFiles/JavaScript/OpenAction/AA/StructTree Alt/ActualText/OCProperties/Outlines/Collection/Thumb/annotations/form-values) + ID randomization + qpdf stream rebuild — matches README "15+".
- Content-stream escaping applied at watermark/header/footer/Bates/annotation sites (escape tests TestSanitization.cpp:283-372).
- Password-encryption AES-256 V5/R6 applied; page-count DoS cap (10,000) at load.

Could-Not-Verify (no build/ctest per instructions; needs dynamic confirmation):
- A-01 runtime proof that PubSec output is actually undecryptable (requires loading output with a real PubSec reader / manual CMS-unwrap + AES decrypt). Verdict from static key-flow analysis + the test failure to decrypt; confidence Med, not High.
- A-03 overflow-to-UB vs PDFium/Qt internal clamping — needs a crafted large-MediaBox PDF run under ASan to confirm crash vs graceful reject.
- Whether PoDoFo PdfMemDocument::Save ever opportunistically drops orphaned image objects (A-02) — header review says no GC by default, but a runtime extraction test is the proof.
- OCSP nonce in the response is requested (OCSP_request_add1_nonce, `:325`) but the response nonce is never checked back; replay-window impact not dynamically assessed.

## Coverage gaps

- NO real PDF-parser fuzzing. src/docmodel/DocumentFuzzer.cpp is a semantic-tree generator for Djot round-trip property tests — it does NOT mutate untrusted PDF bytes. Memory safety of the three real parsers (PoDoFo/PDFium/qpdf) on malformed input is delegated entirely to the upstream libraries; there is no AFL++/libFuzzer harness over loadDocument. Recommend a structure-aware fuzz target on PoDoFoBackend::loadDocument + PdfiumBackend::renderPage (JBIG2/JPXDecode/CFF sinks, xref/tokenizer) before v1.0.0 — the FORCEDENTRY/CVE-2025-9394 class.
- Parser differential testing not implemented. Three backends sit behind IPdfBackend; running the same crafted PDF through all three and diffing object/page/text results is nearly free and would surface parser-confusion / signature-shadow primitives. Not present.
- Encryption round-trip never tested for decryptability (A-01) for either password or certificate mode — tests assert dictionary shape, not recoverability.
- Multi-signature LTV correlation (A-09) untested; only single-signature fixtures exist.
- OCSP/CRL freshness + nonce-match on validation not asserted.
- `#define private public` (PdfEditorEngine.cpp:697) is an undeclared ABI dependency on PoDoFo internals with no compile-time guard — a silent breakage risk on dependency bump; flag for the build/supply-chain workstream.
