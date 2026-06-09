# Round-2 Independent Security Re-Verification - GlyphPDF

Branch: audit-remediation - HEAD: b0b12fb - Scope: read-only re-verification of the WP-2/WP-3/WP-7 security fixes + CRL patch a48e7fd, catastrophe-chain re-assessment, new-finding hunt.
Method: static read of committed source with file:line evidence. No build/ctest re-run (per instructions; baseline 33/33 trusted).

Verdict: 10 of 13 re-verified fixes HOLD, 3 WEAK, 0 BROKEN, plus CHAIN-1 only PARTIALLY closed. The crypto core (A-01, E-01, E-06, E-03, E-04, E-09) is genuinely fixed and now test-backed. The dangerous residual is CHAIN-1: the production in-place save path (HomeController::onSave -> saveDocument) still does a full rewrite of signed documents and is NOT gated by the ProvenanceGuard or the E-08 incremental logic.

---

## Re-verification (per fix)

### A-01 - Certificate encryption (real AES-256-CBC under the FEK) - HOLDS
src/engines/podofo/PdfEncryptPubSec.cpp:122-186 - GenerateEncryptionKey() now copies the real 32-byte FEK into PoDoFo key out-param (:130), and Encrypt()/Decrypt() perform AES-256-CBC with m_fek and a per-stream random IV (:147-186), instead of delegating to a zero-key m_delegate. Wiring at src/engines/PdfEditorEngine.cpp:776-818: fek = SHA256(sessionKey[24]), CMS envelopes carry the same seed, doc.SetEncrypt(makePdfEncryptPubSec(fek, envelopes)).
Round-trip test is now real (tests/TestEncryption.cpp:203-342): CMS-unwraps the seed from /Recipients, derives SHA256(seed), asserts FEK is NOT all-zero (:295-296), raw-reads the still-encrypted stream octets, AES-decrypts with the recovered FEK, asserts plaintext INFLATES to valid PDF tokens (:333-338), and asserts the plaintext marker does NOT survive in ciphertext (:286) - exactly the assertion the old all-zero-key code failed. D-01 macro hack now isolated to one TU with a static_assert layout sentinel (:64).
Residual (Low): FEK derivation SHA256(seed-concat-perms) at :776-777 is self-consistent (round-trip proven) but not the literal adbe.pkcs7.s5 algorithm - third-party Acrobat interop unproven. Recipient-cert RSA key size still not enforced.

### A-02 - Redaction excises image XObjects via CTM intersection - HOLDS (core); residual paths in NF-2
PoDoFoBackend.cpp tracks the full graphics CTM (q/Q/cm at :1166-1180), computes the image bbox from the unit square mapped through the CTM (imageBboxIntersects(), :1129-1140), and on intersection (a) drops the Do and (b) calls neutralizeImageXObject() (:1397-1410) which DESTRUCTIVELY overwrites the XObject stream with a raw 1x1 DeviceGray pixel and strips Filter/SMask/Mask/Decode/DecodeParms/Alternates (:1031-1052). Bytes are physically overwritten, so no GC pass is needed - closes the A-02 object-survives defect. Test testRedactionRemovesImageBytes (tests/TestRedaction.cpp:444) places one image under the rect + one clear, asserts the secret is neutralised to 1x1 and the control retained - proving CTM-based intersection, not the text cursor. G-02 image gap closed.

### CRL patch a48e7fd (revocation error mapping) - WEAK (see NF-1)
SignatureManager.cpp:1500-1528. Confirmed: X509_V_ERR_CERT_REVOKED has no explicit mapping, but that is acceptable by design - CRL_CHECK is deliberately NOT enabled in the verify params (:1338-1349), so OpenSSL never emits CERT_REVOKED during CMS_verify; revocation is sourced only from embedded OCSP. OCSP revoked-override is INTACT: :1555-1561 sets trustStatus=Revoked + isValid=false on V_OCSP_CERTSTATUS_REVOKED, and the override runs for every non-WeakKey/non-CertExpired path (:1536-1537), correctly upgrading an UntrustedChain/Valid verdict to Revoked.
WEAK point (the exact question asked): X509_V_ERR_CRL_SIGNATURE_FAILURE and UNABLE_TO_DECRYPT_CRL_SIGNATURE are bucketed into isUntrustedChain -> UntrustedChain alongside the benign UNABLE_TO_GET_CRL (:1508-1512). A forged/tampered CRL signature is hostile evidence, not a reachability problem, and should not be soft-failed identically to no-CRL-available. See NF-1.

### E-01 - /DocMDP certification - HOLDS
certifyDocument (:1085-1104) routes through signDocumentImpl(..., certificationLevel) (no silent downgrade). :879-902 writes a real /DocMDP via signature->AddCertificationReference(perm) (level 1/2/3 -> NoPerms/FormFill/Annotations); if it throws, the function ABORTS (returns false) rather than falling back to an ordinary signature (:894-901).

### E-06 - Empty TSA token (no null bytes) - HOLDS
TimestampSigner::ComputeSignature (:558-578) THROWS std::runtime_error on an empty token (:574-575) instead of contents.assign(4,0). Caught at :588-593, addDocTimestamp returns false, no malformed /DocTimeStamp is finalized. Dry-run still pads 32 KB (:559-562, correct).

### E-08 - Incremental SaveUpdate on signed docs - WEAK (logic correct, not on the live path)
PoDoFoBackend::writeUpdate (:299-353) is excellent: scans for signature fields (:310-315) and, when present, performs a real incremental SaveUpdate onto a copy of the original bytes (:331-345), preserving every signed /ByteRange. BUT repo-wide search shows NO production caller of writeUpdate - only interface/override declarations (IPdfWriter.h:9, PoDoFoBackend.h:26). Every real save (HomeController::onSave:141, EditController.cpp:280, SecurityController.cpp:426,512) calls saveDocument (:203-219), a bare document->Save() full rewrite with NO signature guard. The protective logic exists but is unreachable from the UI - the core of CHAIN-1 remaining open. The signing path itself is fine: DSS/DocTimeStamp use FileMode::Append+SaveUpdate (:508-509, :582-583).

### E-03 - Narrowed catch in validateSignatures - HOLDS
:1576-1591 - replaced bare catch(...) with typed catch(std::exception) -> logs field+path+what() and sets a distinct trustStatus=VerificationError, plus catch(...) -> qCritical + VerificationError. A legitimate signature can no longer be mislabeled Invalid on an OOM/logic error.

### E-04 - Encrypt-fail surfaced - HOLDS
SecurityController.cpp:142-174 - the worker stores EncryptDocumentHelper::execute return into a shared atomic, wraps the body in try/catch (E-20 too), and on failure shows QMessageBox::critical (original file left unchanged) instead of an unconditional Document-encrypted message.

### E-05 / E-12 - Release error logging - HOLDS (scoped); broader sweep PARTIAL
Security/IO-critical entry points log unconditionally now: loadDocument (:194-199), saveDocument (:212-218), exportPdfA (:1664-1672), encryptDocument (:1701+). However 27 ifdef-QT_DEBUG blocks remain in PoDoFoBackend.cpp; most guard harmless success qDebug lines, but the page-op catches (E-16: rotate/insert/delete/crop/resize/reorder/header-footer/Bates) still suppress PdfError text in Release. Low residual - diagnostics only.

### D-05 - ProvenanceGuard called from save path - WEAK / bypassable (see NF-3)
HomeController::onSave:118-138 now calls provenanceGuard->checkEditVia(...) before saving - so the constructed-but-never-called defect is fixed. BUT it is invoked with a hardcoded EditPath::DirectStructural (:126), and the guard (ProvenanceGuard.cpp:6-31) only throws for the BornPDF + signed + DjotThenSave combination - DirectStructural is UNCONDITIONALLY allowed (:19-23) regardless of signature state. The comment admits DirectStructural is always allowed. So the guard blocks nothing on the live structural-edit-of-signed-PDF path. No-op gate today.

### A-04 - FDF backslash escaping - HOLDS
FormManager.cpp:468-469 emits /T and /V through pdfEscapeLiteralString(...), which escapes backslash before the parens in the correct order (PdfStringEscape.cpp:25-27). The ad-hoc replace chain is gone.

### E-09 - Trust-store load failure - HOLDS
SignatureManager.cpp:122-148 checks the X509_LOOKUP_load_file/add_dir return, logs the OpenSSL error on failure, and sets trustStoreUsedStr=CustomPathLoadFailed (distinct from a clean CustomPath), so an unreadable custom store is no longer silently equivalent to all-signatures-untrusted.

### Parser robustness (TestRobustness.cpp) - ADEQUACY: WEAK (Medium)
tests/TestRobustness.cpp exists and is registered (CMakeLists.txt:636-646) but has only 2 cases: a truncated header (:23) and 4 KB of seeded random bytes (:37). Both exercise only loadDocumentForEditing (PoDoFo). Untested crash surfaces: object-reference loops / deep /Kids recursion, malformed xref streams, FlateDecode/JBIG2/JPXDecode filter bombs, the PDFium render path (A-03 unbounded MediaBox x DPI allocation, still un-clamped - NF-4), the qpdf repair entry, and malformed input into the redaction/sign code (not just load). No structure-aware fuzz harness over loadDocument/renderPage. A smoke floor, not adequate for a v1.0 robust-parser claim.

---

## Catastrophe chains (AUDIT section 4) - re-assessment

- CHAIN-1 Silent un-signing of a signed document - PARTIALLY CLOSED (still exploitable via the normal save path). E-01 (no certify downgrade) and the E-08 logic are fixed, BUT the live path onSave -> saveDocument (full rewrite, no signature guard) and EditController find-replace -> saveDocument are NOT gated: editing + saving a signed born-PDF in place silently invalidates its signature. The D-05 ProvenanceGuard meant to break this chain is a no-op for DirectStructural (NF-3), and writeUpdate (the safe path) has no callers (E-08 WEAK). Net: the chain headline outcome is still reachable. Highest-leverage fix: route signed-doc saves through writeUpdate, or add a signature guard inside saveDocument (NF-3).

- CHAIN-2 Confidential-but-plaintext - CLOSED. A-01 (real FEK encryption, round-trip-tested) + E-04 (encrypt failure surfaced, original left unchanged). Password-mode AES-256 V5/R6 unchanged and asserted unreadable-without-password (TestEncryption.cpp:85-137). No residual.

- CHAIN-3 Redacted-but-leaking, and the test is blind - MOSTLY CLOSED. A-02 image excision (destructive 1x1 neutralisation + CTM intersection) and G-02 now decodes/asserts image bytes are gone. Fail-closed on inline-image/binary streams (:1484-1500, returns false rather than visual-only overlay). Residual: (a) the search-and-redact UX (redactAllMatches, A-05) still uses a fixed 80x18 pt box (PdfViewerWidget.cpp:366) -> wide matches under-covered; (b) image Do names resolved only against page resources, not Form-XObject-local resources, and annotation appearance streams are not walked (NF-2). A redaction over a normal page image is safe; an image nested in a Form local resource or an annotation /AP can still leak.

- CHAIN-4 Supply-chain -> key theft - LARGELY NEUTRALISED, one half still open. Cloud AI providers removed (WP-1) and B-03 update-downloadUrl hardened (WP-2, 9e9d3a6) - confirm separately. BUT B-01 (live private keys in git history) is NOT done: ca.key, signer.key, test_signer.p12 are still in the working tree AND reachable in history at commit a6ea6aa. SP-5b (history rewrite) is explicitly deferred. Key-theft-from-repo remains until that runs.

---

## New / residual findings

| ID | Sev | Location | Finding and fix |
|----|-----|----------|-----------------|
| NF-1 | Medium | SignatureManager.cpp:1509-1510 | Forged-CRL-signature soft-failed like an unreachable CRL. X509_V_ERR_CRL_SIGNATURE_FAILURE and UNABLE_TO_DECRYPT_CRL_SIGNATURE are bucketed into isUntrustedChain -> UntrustedChain, identical to the benign UNABLE_TO_GET_CRL. A tampered/forged CRL is adversarial evidence and should hard-fail. Fix: split these two reason codes into a distinct hard status (RevocationDataForged / Invalid, isValid=false); keep only UNABLE_TO_GET_CRL / CRL_HAS_EXPIRED / CRL_NOT_YET_VALID as soft UntrustedChain. |
| NF-2 | High | PoDoFoBackend.cpp:1377-1387, :1414 | Redaction misses images reached through Form-XObject-local resources, annotation appearance streams, and SMasks. The Do-name -> XObject lookup always uses page.GetResources() even while recursing inside a Form XObject (the recursion at :1414 passes the same page), so an image whose name resolves against the Form own /Resources is never found/neutralised. Annotation /AP appearance streams and an image /SMask over the redaction region are not walked at all. Result: recoverable pixels survive for nested/annotation-hosted images. Fix: resolve Do names against the current canvas resource dictionary (thread the active Resources through the recursion); also neutralise /SMask images and iterate page-annotation appearance streams that intersect a redaction rect. |
| NF-3 | High | HomeController.cpp:123-141, ProvenanceGuard.cpp:19-23, PoDoFoBackend.cpp:203 | Signed-document in-place save is ungated (CHAIN-1 live). ProvenanceGuard is called with a hardcoded DirectStructural, which the guard always allows; saveDocument then full-rewrites and invalidates any signature. No production caller uses the safe writeUpdate. Fix: in saveDocument (or in onSave/EditController before saving), detect signature fields and route to incremental writeUpdate (already implemented), or refuse + prompt Save-As-copy. At minimum, make checkEditVia reject DirectStructural when isSigned for a BornPDF. |
| NF-4 | Medium | PdfiumBackend.cpp:94-106, :67 | A-03 not remediated: unbounded render allocation. widthPixels = (int)(width*scale) with no clamp; a crafted large /MediaBox + /UserUnit yields a multi-GB QImage or an int-overflow to negative dims, then FPDFBitmap_CreateEx(...,image.bits(),image.bytesPerLine()) writes into it - OOM/DoS on first render (document open). Fix: clamp wpx,hpx>0 and <=20000 and wpx*hpx<=120e6 before allocating in both renderPage and renderTile; surface an error instead of a blank page. |
| NF-5 | Low | PdfStringEscape.cpp:14-23 | A-07 idempotency heuristic still mangles backslash+letter user text (watermark with a Windows path becomes a newline). Not a security boundary (output stays balanced - the section 6 injection guarantee holds), but silent content corruption. Fix: drop the already-escaped special-case; always escape from raw input (all call sites pass raw text exactly once). |
| NF-6 | Low | SignatureManager.cpp:374 vs :1546-1562 | OCSP nonce requested but never checked back on validation. OCSP_request_add1_nonce is added at signing, but the embedded-OCSP revocation check reads OCSP_resp_get0(basic,i) without OCSP_check_nonce or certID-match against the signer - also the documented A-09 first-OCSP-only multi-sig correlation gap. Replay/mismatched-response window not closed. Fix: match OCSP certID (issuer-name-hash + serial) to the signer cert before honouring its status; track per-signature VRI/OCSP (M5). |

---

## Not-done (carried forward, confirmed open in committed code)

- B-01 / SP-5b - private keys in git history. tests/fixtures/signing/{ca.key,signer.key,test_signer.p12} present in tree and reachable at commit a6ea6aa. History rewrite (git filter-repo) explicitly deferred to SP-5b (solo, last). CHAIN-4 repo-key-theft leg stays open until done. Until then, treat these keys as compromised (rotate the test CA).
- A-05 - fixed 80x18 redaction box in redactAllMatches (PdfViewerWidget.cpp:366); wide search-redact matches under-covered. WP-7/SP-4; unaddressed.
- A-06 - dead insecure redaction parser PdfViewerWidget::applyRedactions(const QString and) still compiled in (:900). Unreachable today; delete before OSS release to prevent regression.
- E-16 - page-op error suppression in Release (~9 methods still ifdef-QT_DEBUG). Diagnostics-only.
- A-09 - multi-signature VRI/OCSP correlation still first-entry stub (extractOcspFromDss, :604). Single-sig safe; must close before advertising multi-party B-LTA.
- SP-6 / WP-9 - emergence capstone + final verification not run; WP-4 (UI mock data), WP-5 (backend selectors), WP-7 (test maturation), WP-8 (supply chain) all still pending per the section 7 tracker.
- G-05/G-06 (B-LTA tautology test, OCSP QEXPECT_FAIL soft-pass) and the QSKIP TestSignatureRealCrypto under ctest - not re-verified here; flagged WP-7-pending in the original tracker.

---

## Summary table

| Fix | Verdict |
|-----|---------|
| A-01 cert encryption | HOLDS |
| A-02 image excision | HOLDS (core) |
| A-04 FDF escape | HOLDS |
| E-01 /DocMDP | HOLDS |
| E-03 narrowed catch | HOLDS |
| E-04 encrypt-fail surfaced | HOLDS |
| E-05/E-12 release logging | HOLDS (scoped) |
| E-06 timestamp null-bytes | HOLDS |
| E-09 trust-store load | HOLDS |
| Password AES-256 round-trip | HOLDS |
| CRL patch (forged-CRL bucket) | WEAK (NF-1) |
| E-08 incremental save | WEAK (no live caller) |
| D-05 ProvenanceGuard gate | WEAK / no-op (NF-3) |
| Parser robustness coverage | WEAK adequacy |

10 HOLD - 3 WEAK - 0 BROKEN. Chains: 2 CLOSED (CHAIN-2, mostly CHAIN-3), 1 PARTIAL-still-live (CHAIN-1), 1 largely-neutralised-with-open-leg (CHAIN-4 / B-01).
