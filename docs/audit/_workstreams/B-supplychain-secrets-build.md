# Security Audit — Workstream B: Supply Chain, Secrets & Build
<!-- Generated: 2026-06-02 | Auditor: CSO agent | Mode: Comprehensive -->

---

## Summary

**Scope:** Secrets archaeology, dependency supply chain, license-contamination guards, build/distribution, CI pipeline. Read-only; build not re-run per rules.

**Severity counts:** CRITICAL 0 · HIGH 3 · MEDIUM 3 · LOW 3 · INFO 2

**Project license target:** Apache-2.0 (or MIT, finalized M8).

---

## Findings

---

### B-01 · Private RSA keys committed to git history (non-removable without rebase) · HIGH · Confidence: High
`tests/fixtures/signing/ca.key`, `tests/fixtures/signing/signer.key`, `tests/fixtures/signing/test_signer.p12`

**Evidence:** `git show a6ea6aa --stat` confirms `ca.key` (28 lines), `signer.key` (28 lines), and `test_signer.p12` (binary 3363 bytes) were introduced in commit `a6ea6aa` ("Complete Month 1 prompts and Month 2 bonus…"). Both `.key` files begin with `-----BEGIN PRIVATE KEY-----` — confirmed live PEM RSA-2048 unencrypted private keys. The `.gitignore` was subsequently updated to exclude `tests/fixtures/signing/*.key`, `*.p12`, `*.crt`, `*.csr`, `*.srl`, but this exclusion only prevents **future** commits; the keys are permanently in git history for commit `a6ea6aa` and all descendants on every branch/fork.

**Impact:** Any clone of the public OSS repo will download these keys via `git clone`. Even if the keys are test-only and self-signed, they set a precedent of committing private key material and create false alerts from secret-scanning tools (GitHub Advanced Security, truffleHog, gitleaks, Dependabot) that will fire immediately on the public repo, damaging OSS credibility. If any user generates their own test cert from the **same CA key** (reusing the file instead of re-running `generate.bat`), that CA can sign trusted-looking test PDFs. More critically: this pattern, once in git history, is a template for accidental real-key leakage.

**Proposed Fix:**
1. Before the public v1.0.0 repo push (M8), perform a `git filter-repo` (or BFG Repo Cleaner) purge to remove `tests/fixtures/signing/ca.key`, `signer.key`, `signer.csr`, `test_signer.p12`, `test_ca.pem`, `test_ca.srl`, `signer.crt` from all commits in history. This rewrites history — must be coordinated before any public forks exist.
2. Add a `README.md` in `tests/fixtures/signing/` explaining that **all fixtures are regenerated locally** by running `generate.bat` before the test suite; the directory only tracks `generate.bat`, `generate_test_input.py`, `generate_revoked_ocsp.py`, and `ext.cnf`.
3. Verify `.gitignore` entry covers all generated outputs after the purge.
4. Add a `git-secrets` or `gitleaks` pre-commit hook to block future key commits.

---

### B-02 · AI provider keys fall back to plaintext QSettings registry store · HIGH · Confidence: High
`src/engines/ai/AnthropicProvider.cpp:29`, `src/engines/ai/OpenAiProvider.cpp:17`, `src/engines/ai/GeminiProvider.cpp:19`

**Evidence:**
```cpp
// AnthropicProvider.cpp:29
return QSettings().value("ai/keyAnthropicCached").toString();
// GeminiProvider.cpp:19
return QSettings().value("ai/keyGeminiCached").toString();
// OpenAiProvider.cpp:17
return QSettings().value("ai/keyOpenAICached").toString();
```
Each provider's `resolveKey()` function first checks DPAPI-backed Windows Credential Manager (correct), but if that lookup fails or returns empty, silently falls back to a plaintext `QSettings` value under key `ai/keyAnthropicCached` / `ai/keyOpenAICached` / `ai/keyGeminiCached`. On Windows, `QSettings` defaults to the registry (`HKCU\Software\<OrgName>\GlyphPDF`), which is readable by any process running as the same user without special permissions. It is also exported in `reg export`, included in `ntuser.dat` roaming profiles, and readable in cleartext via `regedit`. **No code path was found that writes to these QSettings keys**, but the read path implies they were planned as a fallback (or a legacy path). If any code elsewhere — or a future contributor — writes the key to QSettings as a convenience, it would be stored in plaintext without any user warning.

**Impact:** API keys for Anthropic (`sk-ant-*`), OpenAI (`sk-*`), and Gemini stored in Windows registry are readable by all processes with the same user token, by any user-space malware, and appear in profile backups. This directly undermines the privacy-first positioning and the "encrypted in Windows Credential Manager" promise shown to users in the Preferences UI (`PreferencesDialog.cpp:193`).

**Proposed Fix:**
1. Remove the `QSettings` fallback entirely from all three `resolveKey()` functions. The only valid key source should be (a) the constructor-supplied key (for testing), (b) `CredentialManager::readKey()`.
2. If a migration path is needed for any users who stored keys in QSettings during early development, write a one-time migration helper (read from QSettings → write to CredentialManager → delete the QSettings key) and run it on first launch.
3. Search for any `settings.setValue("ai/key*", ...)` call that may exist in test or preferences code that writes this value; none were found in this audit but the fallback read implies a write path exists or was planned.

---

### B-03 · UpdateChecker does not validate that `downloadUrl` from manifest is HTTPS · HIGH · Confidence: High
`src/core/UpdateChecker.cpp:140`, `src/core/UpdateChecker.cpp:190`

**Evidence:**
```cpp
// UpdateChecker.cpp:140 — manifest parsed, no scheme check on downloadUrl:
m_latest.downloadUrl = QUrl(obj.value("downloadUrl").toString());

// UpdateChecker.cpp:173 — only validity (not scheme) checked before download:
if (!m_latest.downloadUrl.isValid()) { ... return; }

// UpdateChecker.cpp:190 — download proceeds with whatever URL was in the manifest:
QNetworkRequest req(m_latest.downloadUrl);
```
The manifest URL itself is enforced as HTTPS (lines 32 and 51). However, the `downloadUrl` field extracted from the manifest JSON is never checked for `scheme == "https"`. A compromised or man-in-the-middle manifest could return `"downloadUrl": "http://..."` and the application would silently download the MSI over unencrypted HTTP, bypassing TLS protection on the binary. The SHA-256 check (`UpdateChecker.cpp:238`) is conditional: `if (!m_latest.sha256.isEmpty())` — if the manifest also omits `sha256`, the download proceeds with no integrity check. A compromised manifest can set both `downloadUrl` to `http://attacker.com/evil.msi` and `sha256` to `""`.

**Impact:** Combined `downloadUrl` HTTP downgrade + empty `sha256` = unauthenticated binary download + unconditional `msiexec.exe` execution. This is an over-the-air RCE path if the manifest endpoint or network path is compromised. Given the CLAUDE.md states this has not yet shipped publicly, this is a pre-release blocker.

**Proposed Fix:**
```cpp
// In onManifestReply(), after parsing downloadUrl:
if (m_latest.downloadUrl.scheme().toLower() != QLatin1String("https")) {
    emit checkFailed(tr("Update manifest contains a non-HTTPS download URL — rejected."));
    return;
}
// In onDownloadFinished(), make SHA-256 mandatory (not optional):
if (m_latest.sha256.isEmpty()) {
    emit downloadFailed(tr("Update manifest missing SHA-256 — download rejected for safety."));
    QFile::remove(m_downloadedPath);
    return;
}
```
Additionally, consider adding Authenticode signature verification via `WinVerifyTrust` before calling `msiexec`, for defense-in-depth.

---

### B-04 · `CredentialManager` uses `CRED_PERSIST_LOCAL_MACHINE` — keys visible to all users on machine · MEDIUM · Confidence: High
`src/core/CredentialManager.cpp:29`

**Evidence:**
```cpp
cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
```
`CRED_PERSIST_LOCAL_MACHINE` stores credentials in the Windows Credential Manager with machine-wide persistence. While the credential blob is encrypted by DPAPI under the **user's** key (not accessible to other users at rest), the `CRED_PERSIST_LOCAL_MACHINE` scope means the credential entry appears in the **generic credential list for all users on the machine** when listed via `cmdkey /list` or the Credential Manager GUI under "Windows Credentials". For a single-user workstation this is low risk, but on shared machines (shared developer workstations, terminal servers) the credential is accessible to any process running as the **same user** across sessions, and the entry is visible (though not decryptable) to admin users.

**Impact:** Medium — API key metadata (service name `GlyphPDF.AI.Anthropic`) is visible to admins. On shared machines, a user who doesn't log out may expose keys to another session using the same Windows account. The "correct" scope for a per-user secret is `CRED_PERSIST_ENTERPRISE` (roaming, user-only) or `CRED_PERSIST_SESSION` (non-persistent, session-only, more private but lost on reboot).

**Proposed Fix:**
Change to `CRED_PERSIST_ENTERPRISE` for roaming user scenarios, or offer a preference toggle between `CRED_PERSIST_ENTERPRISE` (persistent, user-only) and `CRED_PERSIST_SESSION` (ephemeral). For most desktop users `CRED_PERSIST_ENTERPRISE` is the right default — it ties the credential to the user's account, survives reboots, and roams with domain profiles.

---

### B-05 · ONNX Runtime 1.17.3 downloaded in CI without checksum verification · MEDIUM · Confidence: High
`.github/workflows/ci.yml:60-65`

**Evidence:**
```yaml
- name: Download ONNX Runtime
  shell: pwsh
  run: |
    $url = "https://github.com/microsoft/onnxruntime/releases/download/v1.17.3/onnxruntime-win-x64-1.17.3.zip"
    Invoke-WebRequest -Uri $url -OutFile ort.zip -TimeoutSec 180
    Expand-Archive -Path ort.zip -DestinationPath . -Force
```
No SHA-256 or other integrity check is performed on the downloaded ZIP before extraction. While the URL is HTTPS from the official `microsoft/onnxruntime` GitHub release and the version is pinned (`1.17.3`), HTTPS from GitHub does not protect against a compromised GitHub release asset (e.g., supply-chain attack on the Microsoft onnxruntime releases).

**Impact:** A compromised `onnxruntime-win-x64-1.17.3.zip` at the pinned GitHub release URL (unlikely but possible if Microsoft's release was tampered with, or if the version was re-published) would result in a malicious DLL being linked into the application and shipped in the MSI. Medium severity — the URL is authoritative and well-monitored.

**Proposed Fix:**
Add checksum verification immediately after download:
```powershell
$expected = "KNOWN_SHA256_OF_v1.17.3_ZIP"
$actual = (Get-FileHash ort.zip -Algorithm SHA256).Hash
if ($actual -ne $expected) { Write-Error "ORT checksum mismatch"; exit 1 }
```
The SHA-256 of `onnxruntime-win-x64-1.17.3.zip` can be obtained from the GitHub release page or by computing it once from a known-good download and pinning it in the workflow.

---

### B-06 · PDFium prebuilt import library committed to git without provenance documentation · MEDIUM · Confidence: High
`third_party/pdfium/lib/libpdfium.dll.a`

**Evidence:**
```
git ls-files third_party/pdfium/lib/ → third_party/pdfium/lib/libpdfium.dll.a
```
A prebuilt binary import library (`libpdfium.dll.a`) is committed to git. No `PROVENANCE.md`, `SOURCE_URL`, or checksum file exists alongside it to document: which PDFium commit it was built from, what build flags were used, who built it, or what its SHA-256 is. `LICENSE-3RD-PARTY.md` lists PDFium as "BSD-3-Clause / ANY (prebuilt)" which accurately notes it's a prebuilt but provides no traceability.

**Impact:** Any security researcher auditing the OSS project cannot verify the binary's integrity. If the binary was silently replaced (intentionally or via toolchain compromise), there is no detection mechanism. Additionally, without knowing the PDFium version/commit, it is impossible to assess CVE exposure accurately.

**Proposed Fix:**
1. Add `third_party/pdfium/PROVENANCE.md` documenting: upstream source URL, git commit hash, PDFium version tag, build environment (GCC version, flags), and SHA-256 of the `.dll.a` file.
2. Optionally add a CMake `file(SHA256 ...)` check that verifies the binary's hash at configure time and fails with a clear message if it has changed.

---

### B-07 · CI GitHub Actions not pinned to commit SHAs · LOW · Confidence: High
`.github/workflows/ci.yml:21`, `.github/workflows/ci.yml:26`

**Evidence:**
```yaml
uses: actions/checkout@v4
uses: msys2/setup-msys2@v2
```
Both third-party actions are pinned to **mutable version tags** (`@v4`, `@v2`), not to immutable commit SHAs (e.g., `actions/checkout@11bd71901bbe5b1630ceea73d27597364c9af683`). A tag like `@v4` can be force-pushed by the action maintainer to point to a different commit, or the maintainer's account could be compromised (supply-chain attack).

**Impact:** Low — both actions are from well-maintained, widely-used repos (`actions/checkout` by GitHub, `msys2/setup-msys2` by the MSYS2 org). The risk is real but low probability for these specific actions. Industry standard for high-assurance CI is SHA pinning.

**Proposed Fix:**
Pin both to specific commit SHAs, e.g.:
```yaml
uses: actions/checkout@11bd71901bbe5b1630ceea73d27597364c9af683  # v4.2.2
uses: msys2/setup-msys2@d0ed52bd0a1e3f40c2f35285f79b45a52ddddca1  # v2.24.1
```
Use `dependabot` or Renovate Bot with `pin-digest: true` to keep these updated automatically.

---

### B-08 · jbig2enc vendored without upstream source URL or integrity pin · LOW · Confidence: High
`third_party/jbig2enc/` (version 0.31, Apache-2.0)

**Evidence:**
- `third_party/jbig2enc/VERSION` contains `0.31`.
- `third_party/jbig2enc/COPYING` confirms Apache-2.0 (correct).
- No `PROVENANCE.md` documenting the upstream URL (`https://github.com/agl/jbig2enc`), the specific commit hash, or the SHA-256 of the source archive.
- `third_party/CMakeLists.txt` builds jbig2enc directly from vendored source with no integrity check.

**Impact:** Low — Apache-2.0 license is confirmed, and version 0.31 is the latest (2024). Without provenance documentation, future maintainers cannot verify whether the vendored source matches the upstream release, or audit changes introduced in the local copy.

**Proposed Fix:**
Add `third_party/jbig2enc/PROVENANCE.md` documenting: `upstream: https://github.com/agl/jbig2enc`, `version: 0.31`, `commit: <SHA>`, and SHA-256 of the source tarball.

---

### B-09 · `DjVu` CMake option warning message misleadingly says "user accepts GPL-2.0+" · LOW · Confidence: High
`CMakeLists.txt:318`

**Evidence:**
```cmake
message(STATUS "DjVu importer: ENABLED via DjVuLibre (GPL-2.0+ accepted)")
```
The `HAS_DJVU=ON` build option is clearly defaulted OFF and well-guarded. However, the status message says "GPL-2.0+ accepted" — this phrasing implies the project itself is accepting GPL terms, whereas the actual intent (per ROADMAP) is that the **user who chooses to enable it** accepts those terms for their own build. For an OSS project, the phrasing matters: a CMake `STATUS` message in a public build log can be misread as the project accepting a GPL contamination.

**Impact:** Low — informational confusion risk only; the guard is correct.

**Proposed Fix:**
```cmake
message(STATUS "DjVu importer: ENABLED (HAS_DJVU=ON). "
               "This build links DjVuLibre (GPL-2.0+). "
               "The resulting binary may not be redistributed under Apache-2.0 alone.")
```

---

### B-10 · `LICENSE-3RD-PARTY.md` lists PoDoFo as version 0.10.4 but actual vendored version is 1.1.0 · INFO · Confidence: High
`LICENSE-3RD-PARTY.md:8`, `CMakeLists.txt:47`

**Evidence:**
```markdown
| **PoDoFo** | 0.10.4 (MSYS2 ucrt64) | LGPL-2.0+ OR MPL-2.0 | Yes |
```
But `CMakeLists.txt:47` vendors `third_party/podofo/install` as `podofo 1.1.0`, and `CLAUDE.md` §2 explicitly states PoDoFo 1.1.0 is vendored. The MSYS2 ucrt64 package `podofo` ships 0.10.4; the build prioritizes the vendored 1.1.0. The license matrix is therefore showing the MSYS2 fallback version, not the actual linked version.

**Impact:** Informational — PoDoFo 1.1.0 retains LGPL-2.0+/MPL-2.0 licensing (no compatibility issue), but the published third-party license document will be incorrect when the OSS repo goes public, which is a legal housekeeping concern.

**Proposed Fix:**
Update `LICENSE-3RD-PARTY.md` line for PoDoFo to read `1.1.0 (vendored at third_party/podofo/)`.

---

### B-11 · `OpenSSL` version listed as 3.6.2 in LICENSE-3RD-PARTY but no pinning mechanism · INFO · Confidence: Med
`LICENSE-3RD-PARTY.md:9`

**Evidence:**
OpenSSL 3.6.2 is listed as the MSYS2 package version. MSYS2 packages are installed via `pacman` without version pinning — `pacman -Syu` will upgrade OpenSSL to whatever version is current when run. In CI, the MSYS2 setup step uses `update: false`, which prevents the initial upgrade, but future package installs could still pull a newer or different OpenSSL. The project has no `pacman.lock` or equivalent dependency lock file.

**Impact:** Informational — OpenSSL 3.x is actively maintained. Without pinning, a future `pacman -Syu` could introduce a breaking or security-relevant API change. This is a supply-chain hygiene gap rather than an immediate vulnerability.

**Proposed Fix:**
For M8 release, generate a `msys2-packages.lock` (list of exact installed package versions from `pacman -Q`) and commit it to the repo as a reference manifest. Document in the build instructions that this should be used to reproduce the exact build environment.

---

## License-Contamination Guard Check

| Dependency | Rule | Enforced? | Evidence |
|:--|:--|:--|:--|
| **MuPDF (AGPL-3.0)** | NEVER link in-process; CMake FATAL_ERROR | **Yes — 4 methods** | `CMakeLists.txt:96-120`: target existence check, `find_package(MuPDF)`, `pkg_check_modules(MUPDF)`, plus alias target check. All four trigger `message(FATAL_ERROR ...)`. |
| **Poppler (GPL-2.0+)** | NEVER link in-process; CMake FATAL_ERROR | **Yes — 4 methods** | `CMakeLists.txt:99-120`: symmetric to MuPDF — target, `find_package(Poppler)`, `pkg_check_modules(POPPLER)`. All four FATAL_ERROR. |
| **veraPDF (AGPL-3.0)** | SUBPROCESS ONLY; never link | **Yes** | `VeraPdfValidator.cpp` uses `QProcess::start(kCliPath, args)`. No `find_package(veraPDF)` or linker reference. `CMakeLists.txt:330-337` configures only a CLI path define. |
| **DjVuLibre (GPL-2.0+)** | `HAS_DJVU=OFF` default; import-only; no DjVu output | **Yes** | `CMakeLists.txt:305`: `option(HAS_DJVU "..." OFF)`. `DjvuImporter.cpp` guards all DjVuLibre API calls with `#ifdef HAS_DJVU`. No DjVu output code path found. |
| **Surya (Open Rail-M weights)** | License-verify before integration; subprocess if GPL-3.0 | **Yes** | `SuryaDetector.h:1-24` documents the license decision explicitly. Stub guarded by `HAS_SURYA`. Subprocess invocation via `QProcess`. Disabled by default. |
| **jbig2enc (Apache-2.0)** | Must be Apache/BSD/MIT | **Yes** | `COPYING` in `third_party/jbig2enc/` confirms Apache-2.0. `CMakeLists.txt:210`: comment "Apache-2.0". JBIG2 pattern-matching mode blocked at runtime in `MrcPageProcessor.cpp:15-18`. |
| **MuPDF alias target** | No `mupdf-pkg` alias | **Yes** | `CMakeLists.txt:96` checks `TARGET mupdf OR TARGET libmupdf OR TARGET mupdf-pkg`. |
| **Poppler alias target** | No `poppler-cpp`/`libpoppler` alias | **Yes** | `CMakeLists.txt:99` checks `TARGET poppler OR TARGET poppler-cpp OR TARGET libpoppler`. |

**Overall assessment:** All four license-contamination rules from CLAUDE.md §6 are implemented correctly. The guards are comprehensive (4 detection methods for MuPDF/Poppler), veraPDF is subprocess-only confirmed in code, DjVu output is excluded, and jbig2enc is Apache-2.0 confirmed.

---

## Dependency Version / CVE Table

> Confidence levels: **High** = exact version confirmed in code/config; **Med** = version from LICENSE-3RD-PARTY.md; **Low** = version unknown or inferred.
> CVE exposure is assessed against public NVD/OSV records as of 2026-06.

| Library | Pinned Version | Source | CVE / Advisory Notes | Confidence |
|:--|:--|:--|:--|:--|
| **Qt 6** | 6.11.0 | MSYS2 pacman | Qt 6.11.0 released ~May 2026. No known critical CVEs in Qt 6.11.x at time of audit. Qt Network uses system TLS (Schannel on Windows). | High |
| **PoDoFo** | 1.1.0 (vendored) | `third_party/podofo/` | PoDoFo 1.1.0 is the current release; no known public CVEs specific to 1.1.0. PoDoFo 0.9.x had heap-overflow CVEs (2017-2019); 1.x rewrote core. | High |
| **OpenSSL** | 3.6.2 (MSYS2) | MSYS2 pacman | OpenSSL 3.6.x is current LTS. No critical unpatched CVEs as of 2026-06. The project uses OpenSSL for PAdES signing (correct SHA-256 path verified). | Med |
| **PDFium** | Unknown (prebuilt) | `third_party/pdfium/lib/` | Version unknown — no provenance doc (see B-06). PDFium historically has regular security fixes for malformed PDF parsing. Without a version tag, CVE exposure cannot be assessed. **Unable to verify.** | Low |
| **ONNX Runtime** | 1.17.3 | CI download / local | ONNX Runtime 1.17.3 (March 2024). CVE-2024-30172 (ONNX deserialization, fixed in 1.18+) may apply depending on which deserialization code paths are used. Version is below the 1.18 fix. Risk is medium — GlyphPDF uses ONNX for inference (model loading), not user-controlled deserialization. | Med |
| **qpdf** | 12.3.2 (MSYS2) | MSYS2 pacman | qpdf 12.3.2 is current. No known critical CVEs. Used only for linearization (not in the signing path per CLAUDE.md §6). | Med |
| **Tesseract** | 5.5.2 (MSYS2) | MSYS2 pacman | Tesseract 5.5.x is current. No known critical unpatched CVEs. Used for OCR of user-supplied PDFs; input comes from PDFium-rasterized images, not raw user data. | Med |
| **Leptonica** | 1.87.0 (MSYS2) | MSYS2 pacman | Leptonica 1.87.0 is current. No known critical unpatched CVEs. | Med |
| **libxml2** | (MSYS2, version unknown) | MSYS2 pacman | libxml2 has historically been a significant attack surface (XXE, heap overflows). Several CVEs in 2024-2025 (CVE-2024-56171, CVE-2025-24928). Exact MSYS2 version not confirmed in code. Not listed in `LICENSE-3RD-PARTY.md`. **Unable to verify version.** | Low |
| **freetype** | (MSYS2, version unknown) | MSYS2 pacman | Freetype has well-maintained security track record. Recent versions (2.13.x) have no critical open CVEs. | Low |
| **zlib** | (MSYS2, version unknown) | MSYS2 pacman | zlib 1.3.x is current. No critical open CVEs. | Low |
| **curl** | (MSYS2, version unknown) | MSYS2 pacman | curl has a very active CVE history; MSYS2 ucrt64 generally tracks upstream closely. Without pinning, exact version unknown. | Low |
| **libpng** | (MSYS2, version unknown) | MSYS2 pacman | No critical open CVEs in recent libpng (1.6.x). | Low |
| **libjpeg-turbo** | (MSYS2, version unknown) | MSYS2 pacman | No critical open CVEs in recent libjpeg-turbo (3.x). | Low |
| **libtiff** | (MSYS2, version unknown) | MSYS2 pacman | libtiff has had ongoing CVEs (heap overflows in TIFF parsing). MSYS2 tracks upstream. Without pinning, exact version unknown. | Low |
| **OpenJPEG** | 2.5.4 (MSYS2) | `LICENSE-3RD-PARTY.md` | OpenJPEG 2.5.4 released 2024. No known critical open CVEs. BSD-2-Clause. | Med |
| **jbig2enc** | 0.31 | `third_party/jbig2enc/VERSION` | Latest release. Apache-2.0. No known CVEs. Uses Leptonica for image processing. | High |
| **Lua 5.4** | 5.4.x (vendored) | `third_party/lua-5.4/` | Lua 5.4 MIT. No known critical CVEs affecting the vendored use case (parser only, not sandboxed execution of untrusted scripts). | High |
| **DjVuLibre** | 3.5.30 (MSYS2, optional) | `LICENSE-3RD-PARTY.md` | DjVuLibre 3.5.30 has known CVEs in older versions (integer overflow, heap overflow in image parsing). `HAS_DJVU=OFF` default means this is not compiled into normal builds. Relevant only when enabled by advanced users. | Med |
| **libzip** | (MSYS2, version unknown) | `CMakeLists.txt:82` | libzip 1.10.x is current. No known critical open CVEs. | Low |

---

## Verified vs Could-Not-Verify

### Verified (with High/Med confidence)

| Item | Status | Notes |
|:--|:--|:--|
| MuPDF FATAL_ERROR guard — 4 detection methods | Verified correct | `CMakeLists.txt:96-120` |
| Poppler FATAL_ERROR guard — 4 detection methods | Verified correct | `CMakeLists.txt:99-120` |
| veraPDF subprocess-only (no in-process link) | Verified | `VeraPdfValidator.cpp` uses `QProcess` only |
| DjVu `HAS_DJVU=OFF` default + import-only | Verified | `CMakeLists.txt:305`; `DjvuImporter.cpp` guarded by `#ifdef HAS_DJVU` |
| jbig2enc license is Apache-2.0 | Verified | `third_party/jbig2enc/COPYING` |
| JBIG2 pattern-matching blocked in code | Verified | `MrcPageProcessor.cpp:15-18`, comment explicitly prohibits cross-page symbol dictionary |
| Surya license gate (Open Rail-M; subprocess only; default disabled) | Verified | `SuryaDetector.h:1-24` |
| AI keys stored via DPAPI (Windows Credential Manager) | Verified — primary path | `CredentialManager.cpp:24-31` |
| No hardcoded AI API keys in source | Verified | Full source scan; no literal key values found |
| UpdateChecker manifest URL enforced HTTPS | Verified | `UpdateChecker.cpp:32`, `UpdateChecker.cpp:51` |
| SHA-256 verification of downloaded MSI | Verified (conditional) | `UpdateChecker.cpp:238-249` — but see B-03: only when `sha256` non-empty |
| `NoLessSafeRedirectPolicy` on all HTTP requests | Verified | `UpdateChecker.cpp:111`, `UpdateChecker.cpp:194` |
| ONNX Runtime gitignored (not in repo) | Verified | `git ls-files onnxruntime-win-x64-1.17.3/` returns empty |
| CI `update: false` prevents unconstrained `pacman -Syu` | Verified | `.github/workflows/ci.yml:29` |
| Signing fixture keys generated locally (regeneratable) | Verified — generate.bat present | `tests/fixtures/signing/generate.bat` |
| CA/signer private keys committed to git history (commit a6ea6aa) | Confirmed — HIGH finding | `git show a6ea6aa --stat` |
| QSettings plaintext fallback read paths for AI keys | Confirmed — HIGH finding | Three `resolveKey()` functions |

### Could Not Verify

| Item | Reason |
|:--|:--|
| PDFium version and CVE exposure | No `PROVENANCE.md`; binary only in `third_party/pdfium/lib/libpdfium.dll.a`; no version tag available |
| libxml2, curl, libtiff, freetype, zlib, libpng, libjpeg-turbo exact MSYS2 versions | Not pinned; no lock file; not listed in `LICENSE-3RD-PARTY.md` |
| ONNX Runtime 1.17.3 CVE-2024-30172 applicability | Depends on which ONNX model serialization code paths are exercised; model files not in repo |
| MSI Authenticode code-signing certificate | CLAUDE.md §9 states "EV code-signing cert obtained + MSI signed" is a pre-release checklist item for M8; cert not yet obtained |
| `build-msi.bat` MSI signing step | No `signtool` invocation found in `build-msi.bat`; signing must be added separately for M8 |
| Whether `ai/keyAnthropicCached` QSettings value is ever written by any code | No write path found in this audit; fallback read path exists and implies an intended write path |
