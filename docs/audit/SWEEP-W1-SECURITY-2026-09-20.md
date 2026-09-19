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
