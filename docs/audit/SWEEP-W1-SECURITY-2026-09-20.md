# SWEEP-W1 SECURITY AUDIT — trust boundaries & claims-honesty (2026-09-20)

- **Branch**: `feat/sweep-w1-security` @ base `8f62a17` (== `feat/parity-glm` tip, includes the
  accessibility merge). REVIEW-ONLY lane: no production file was modified; findings carry
  FAILING repros, never fixes.
- **Method**: static code-path audit of the CURRENT candidate, prioritizing newest code
  (policy R24, send-for-signing R26 P1, keystroke tier R18f, signing config R19), plus a
  full egress-construction grep; older surfaces (update chain B-03/N-2, secret store v3,
  OCSP plumbing) spot-checked only — they passed SEP13 + the 2026-09-14 independent review.
- **Repro probe**: `.context/sweep-w1-sec/SweepW1SecProbe.cpp` (gitignored by policy, like
  all prior lane probes) — 5/5 slots FAIL as designed at base SHA; output in
  `.context/sweep-w1-sec/SweepW1SecProbe.log`.
- **Suite state at HEAD**: full build green (511/511 steps, `-j 2`). Security-relevant suites
  9/9 PASS: TestPolicyController, TestNetworkDisclosure, TestSupportBundle,
  TestSendForSigning, TestSidecarReopenState, TestSignatureValidation(+Mock),
  TestSignatureRealCrypto (parallel-lane flake; passed standalone on rerun), TestFormJsCalc.
  None of these cover the findings below — the existing pins exercise only the
  app-authored happy path for each gap (e.g. TestSendForSigning writes
  `reconfirmedSha256` the way the controller does, never the way an attacker would).

## Findings (severity-ranked)

| ID | Sev | Title | Primary evidence |
|----|-----|-------|------------------|
| F1 | High-Med | Machine policy is enforced with zero provenance verification (squatter-able trust boundary) | PolicyController.cpp:62-112; SecurityController.cpp:111-122 |
| F2 | Medium | "PAdES B-T/LT/LTA attained" claimed for ANY non-empty TSA response — token bytes never validated | SignatureManager.cpp:1415 |
| F3 | Medium | Sidecar mutation gate bypassable by pre-seeded `reconfirmedSha256` — no user consult | SigningRequestRunner.cpp:88-89 |
| F4 | Medium | Network page + support bundle misreport TSA touchpoint state under policy (breaks R24(c) contract) | NetworkTouchpoints.cpp:40-41 |
| F5 | Low-Med | Sidecar anchor rect never clamped to page bounds — invisible off-page signature field | SignatureFieldCreator.cpp:95-131 |
| F6 | Low | Support bundle privacyNote says "no file paths" while the policy statusLine embeds one | SupportBundle.cpp:206, 254-256 |

---

## F1 — Machine policy: unverified trust boundary (High-Med)

**Claim under audit** (PolicyController.h:13): "%PROGRAMDATA%\GlyphPDF\policy.json is
admin-controlled" — the file's authority rests entirely on an ACL assumption the code
never checks.

**Facts**:
1. `PolicyController::load()` (PolicyController.cpp:62-112) does `QFile::exists` →
   `open(ReadOnly)` → parse. There is **no ownership, ACL, or provenance check** of any
   kind, and no integrity anchor (no signature, no hash).
2. The installer does **not** pre-create the folder: `packaging/GlyphPDF.wxs` contains no
   ProgramData/CommonAppData component. On default Windows ACLs `%ProgramData%` grants
   BUILTIN\Users create-folder rights (CREATOR OWNER full control on what they create), so
   **any interactive user can pre-create `C:\ProgramData\GlyphPDF\policy.json`** before an
   admin ever deploys one. Every later user of the machine is then told "Machine policy
   loaded from …" — the disclosure is honest about the path but cannot distinguish an
   admin policy from a squatter's.
3. **Value flow into dispatch**: the enforced keys `signing/tsaUrl` + `signing/padesLevel`
   flow `PolicyController::effectiveValue` → `SecurityController::readSigningConfig`
   (SecurityController.cpp:111-122) → into ALL FOUR production dispatch sites:
   signDocument (:552), certifyDocument (:1046), timestampDocument (:1070 →
   `setTsaUrl`/`addDocTimeStamp` :1078-1098), and SendForSigningController.runSignStep
   (:138). The "Enforced app-wide" wording is accurate; the problem is WHO supplies the
   value.
4. **TOCTOU**: none in-process — the policy is a load-once snapshot (`m_loadedOnce`,
   ensureLoaded() is one-shot; :114-119) and enforcement reads the in-memory hash, so
   there is no read-vs-enforce race window to close. The file is attacker-controllable
   before first load anyway, which subsumes the race.
5. **Command/argument injection**: none — the only policy value that leaves the settings
   domain is `tsaUrl`, which is used as a `QNetworkRequest` URL (SignatureManager httpPost);
   no policy value reaches QProcess/argv.
6. **Attacker gain**: every sign/certify/timestamp dispatch sends the document digest to
   the attacker's endpoint (privacy/integrity of the workflow), and fake timestamp tokens
   are embedded (compounding with F2). `https://` is required (httpPost refuses `http://`,
   SignatureManager.cpp:255), but a Let's Encrypt cert for the attacker's own domain
   satisfies TLS — no MITM needed.

**Repro** (probe slot `policyValueFlowsToSigningDispatchUnverified`):
```
FAIL!  : SweepW1SecProbe::policyValueFlowsToSigningDispatchUnverified() Compared values are not the same
   Actual   (cfg.tsaUrl): "https://attacker.example/rfc3161"
   Expected (QString()) : ""
```
The squatter's policy drives the signing dispatch (asserted DESIRED behavior: an
unverified policy must not choose where the digest is sent).

---

## F2 — Attained-level label overclaims on unvalidated timestamp tokens (Medium)

`SignatureManager.cpp:1415` (ComputeSignature B-T branch):
```cpp
if (!tsToken.isEmpty()) {
    CMS_unsigned_add1_attr_by_NID(si, NID_id_smime_aa_timeStampToken, ...);
    m_priv->timestampMissing = false;   // token embedded: B-T attained
```
The TSA response bytes are **never parsed** (no `d2i_TS_RESP`, no TimeStampToken check, no
status/imprint match). ANY HTTP-200 body — an HTML error page from a misconfigured TSA, or
arbitrary bytes from a hostile TSA (or F1's squatter URL) — is embedded as
`id-aa-timeStampToken` and `attainedLevelLabel` (SecurityController.cpp:154-176) then
reports the requested level as ATTAINED. The honest-degradation machinery (SEP13 lead 1,
R19c) only covers the *missing* case; the *garbage* case is silently upgraded.

**Repro** (`attainedLevelLabelsGarbageTimestampAsBt`):
```
FAIL!  : SweepW1SecProbe::attainedLevelLabelsGarbageTimestampAsBt() Compared values are not the same
   Actual   (label)                : "B-T"
   Expected (QStringLiteral("B-B")): "B-B"
```
The detail struct has no field that could distinguish a validated token from garbage — the
API cannot express the honest answer today. Not just adversarial: a plain
misconfiguration (TSA returning 200 + HTML) makes the UI tell the user "Document signed …
(PAdES B-T)" for a signature that is B-B.

---

## F3 — Signing-request mutation gate: sidecar-written re-confirm skips the user (Medium)

**Claim under audit** (SigningRequestModel.h:18-21): "the fill flow refuses steps while the
on-disk bytes differ, **unless the user explicitly re-confirmed the change**
(`reconfirmedSha256`)".

`SigningRequestRunner::precheck` (SigningRequestRunner.cpp:88-89):
```cpp
const bool preparedMatches = (!prepared.isEmpty() && current == prepared)
                             || (!reconfirmed.isEmpty() && current == reconfirmed);
```
accepts whatever `reconfirmedSha256` sits in the sidecar. The USER is only consulted when
precheck returns `DocumentChanged` (SendForSigningController.cpp:179-201 writes the field
after a Yes/No dialog). The sidecar is unsigned JSON (P1-documented residual), so anyone
with write access to `<doc>.signrequest.json` — same-user malware, a sync/share tool, a
malicious sender — can mutate the document AND pre-seed `reconfirmedSha256` to the mutated
bytes: the step then proceeds **with no dialog, no disclosure**, and the user signs the
mutated document inside the familiar SignatureDialog. The engine-level signatures still
detect document mutation for LTV purposes, but the workflow's own honesty rule ("never
sign anything other than the prepared bytes without the user deciding") is defeated.

**Repro** (`reconfirmGateSkipsUserOnPreseededSidecar`) — real engine, real fixtures:
```
FAIL!  : SweepW1SecProbe::reconfirmGateSkipsUserOnPreseededSidecar() Compared values are not the same
   Actual   (int(pre.code))                                          : 0
   Expected (int(SigningRequestRunner::StepRefusal::DocumentChanged)): 5
```
precheck returned `None` on mutated bytes — the DocumentChanged dialog would never run.
(The schema handshake itself is genuinely fail-closed — MissingMagic/UnknownVersion/
SchemaInvalid refusals all pinned by TestSendForSigning — the gap is only this field.)

Downstream forgery limits (verified): pre-forged `signed=true` entries cannot fake
completion silently — `verifyAgainstDocument` re-derives from the document on every panel
refresh and the warnings are surfaced in the panel title (SigningProgressPanel.cpp:85-86,
105); `runFillStep` attributes only signatures it can actually observe post-step
(SigningRequestRunner.cpp:227-244) and `applyStepToModel` refuses anything without
committed bytes. The sidecar's attested records are display-trusted but
document-verified — the residual is social-engineering display, not enforcement.

---

## F4 — Network disclosure surface misreports TSA state under policy (Medium)

**Contract under audit** (NetworkTouchpoints.h:19-23): "`enabled` = the touchpoint **will
fire** under the CURRENT settings when its invocation moment comes."

`NetworkTouchpoints::enumerate` reads the RAW QSettings value
(`NetworkTouchpoints.cpp:40-41`) while enforcement uses the policy-effective value
(SecurityController readSigningConfig). With a policy-managed `signing/tsaUrl` and an empty
user setting, the Preferences "Network" page — which introduces itself with "Every network
touchpoint in GlyphPDF" (PreferencesDialog.cpp:537) — and the support bundle both report
the TSA touchpoint as **Disabled** while the next signing above B-B (or any document
timestamp) **will** fetch from the policy URL. The R24 keys with "pending" enforcement
(update/ai/ocr) are consistent today; only the enforced tsa key misreports.

**Repro** (`networkTouchpointMissingPolicyEnabledTsa`):
```
FAIL!  : SweepW1SecProbe::networkTouchpointMissingPolicyEnabledTsa() 'tsa->enabled' returned FALSE.
  (TSA touchpoint shown DISABLED while the enforced policy URL makes it fire …)
```

## F5 — Sidecar anchor rect: no page-bounds clamp → invisible signature fields (Low-Med)

The fill flow's lazy placement consumes the sidecar's `anchorPage`/`anchorRect` with only
these checks: `rectFromJson` demands x,y ≥ 0 and w,h > 0 (SigningRequestModel.cpp:38);
precheck demands `anchorRect.isValid()` (SigningRequestRunner.cpp:111-112);
`SignatureFieldCreator` validates positive size + page-index range (lines 95-121) — and
**nothing bounds the rect against the page MediaBox**. A crafted `<doc>.signrequest.json`
can therefore bind the signer's field at (200000, 200000): the field is created off-page
(widgets outside the MediaBox render in no viewer), the signer clicks "Sign as this
signer", enters their P12, and the signature is committed to a field they can never see.
Page-index IS range-checked, so no crash — the impact is pure placement honesty.

**Repro** (`sidecarAnchorNotClampedToPage`):
```
FAIL!  : SweepW1SecProbe::sidecarAnchorNotClampedToPage() '!created' returned FALSE.
  (createSignatureFields accepted an off-page anchor — an invisible signature field can be planted via the sidecar)
```

## F6 — Support bundle "no file paths" note vs the policy disclosure path (Low)

`buildFromSettings` embeds `policy.statusLine()` verbatim (SupportBundle.cpp:206), which by
design names the policy path (PolicyController.cpp:127-148). For the default location that
is `C:/ProgramData/…` — harmless — but with `GLYPHPDF_POLICY_PATH` (test seam, sanctioned
for portable installs) pointing into a profile, the bundle carries
`C:/Users/<redacted>/…` after the scrub pass. The username is scrubbed (the scrub regex
covers /Users|/home on both slash spellings), yet the privacyNote still asserts "no file
paths" (SupportBundle.cpp:254-256) — an overclaim by one redacted path. All other emitted
strings verified: settings allowlist (10 keys, URL-shaped keys excluded AND disclosed as
excluded), recents/documents as counts only, capabilities omit `c.detail` precisely because
it can carry absolute paths, network = id+enabled+invocation only.

---

## Never-network claim: complete egress grep table

Every socket/HTTP/TLS/process-egress construction site in `src/`, traced to its gate:

| # | Construction site | Destination | Gate (honor check) |
|---|-------------------|-------------|--------------------|
| 1 | SignatureManager.cpp:268 `QNetworkAccessManager s_nam` (httpPost) — TSA fetch (fetchTimestampToken :247) + OCSP fetch (AIA URL) | user/policy TSA URL; cert-AIA URL | Scheme: `http://` refused at :255 (https enforced). TSA fires only when `tsaUrl` non-empty + preflight refusals (SecurityController signingPreflightRefusal) at all 4 dispatch sites. OCSP: **no consent switch — fires automatically during Validate Signatures; disclosed, not gated** (NetworkTouchpoints.cpp:62-73). Destination = attacker-influenceable via the PDF's own cert AIA (disclosed). |
| 2 | OllamaProvider.cpp:239 `QNetworkAccessManager nam` (AI chat / Test connection) | `ai/ollamaEndpoint` (default localhost:11434) | User-invoked only (chat UI / Test connection). Endpoint allowlist R04/SECFIX-5: HTTP only loopback spellings (localhost/127/8/::1 via QHostAddress), HTTPS only allowlisted hosts, user-info rejected, redirects Manual. Raw QSettings read — policy `ai/ollamaEndpoint` NOT enforced, matching its "pending" disclosure. |
| 3 | OcrEngine.cpp:53 `QNetworkAccessManager networkManager` (traineddata download) | fixed `https://raw.githubusercontent.com/tesseract-ocr/tessdata_best/…` | Gated at OcrEngine.cpp:217 by `ocr/allowNetworkDownload` **default OFF** (raw QSettings; policy pending — disclosed). Fixed https host, `setMaximumRedirectsAllowed(0)`, 100 MiB cap, language allowlist, QSaveFile commit. |
| 4 | UpdateChecker.cpp:32 `m_nam` (manifest GET + MSI download) | `update/manifestUrl` / manifest downloadUrl | Startup leg gated at GpMainWindow.cpp:1550 `update/checkOnStartup` **default OFF**; Check Now is a dialog click (UpdateDialog.cpp:91,99). HTTPS enforced in ctor AND setManifestUrl; manifest refuses non-https downloadUrl / missing sha256 (B-03, fail-closed BEFORE advertising); mandatory SHA-256 re-verify at download+apply time; Authenticode required before msiexec (:326-340); non-Windows refuses to launch; `NoLessSafeRedirectPolicy`. |
| 5 | UpdateChecker.cpp:352 `QProcess::startDetached("msiexec.exe")` | local installer | Reachable only via applyUpdate() after the full verify chain above; user clicked through UpdateDialog. |
| 6 | VeraPdfValidator.cpp:91 `QProcess proc` (veraPDF CLI) | local CLI, argv = pdfPath + flags | Local validation subprocess; no network in invocation or flags. Not a network touchpoint (consistent with its absence from the enumeration). |

Grep coverage: `QNetworkAccessManager|QTcpSocket|QUdpSocket|QSslSocket|QNetworkRequest|
QNetworkReply|curl|QProcess|::execute|ShellExecute|system\(|BIO_new_connect|SSL_connect|
getaddrinfo|::socket\(|::connect\(` across src/**. {1..6} is the complete construction
set; no font/theme/other downloaders exist. QDesktopServices::openUrl (browser handoff) is
not in-app network I/O. **Verdict: no ungated network path found; the claim's honest form
is "no network except: user-invoked local chat, configured TSA, automatic-but-disclosed
OCSP, opt-in updates, opt-in OCR downloads" — which the Network page states.** F4 shows the
one place the *reporting* of that state goes wrong under policy.

## Repro inventory (`.context/sweep-w1-sec/`, gitignored by repo policy)

| File | Purpose | Result at base 8f62a17 |
|------|---------|------------------------|
| `SweepW1SecProbe.cpp` | 5 failing repros (S1↔F3, S2↔F5, S3↔F2, S4↔F1, S5↔F4), QtTest, offscreen, FU-2 unique temp dirs | 5 failed, 0 passed of the 5 slots (exit 5) — each FAIL is the finding |
| `build_probe.py` | Builds the probe against `build-presets` using the gateA lane's ninja link-line recipe | build exit 0 |
| `SweepW1SecProbe.log` | Probe output (quoted above per finding) | in tree |
| `SweepW1SecProbe.moc`, `*-build.log`, `dlls.txt`, `run1.txt`, `probe-run.txt` | build/runtime debris | removable |
